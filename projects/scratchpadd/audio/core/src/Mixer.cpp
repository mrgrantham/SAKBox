#include <ScratchPadd/Audio/Mixer.hpp>

#include <ScratchPadd/Audio/RealTime.hpp>

#include <algorithm>
#include <cmath>

namespace ScratchPadd::Audio {

Mixer::Mixer(std::uint32_t maxVoices, std::uint32_t queueCapacity)
    : voices_(maxVoices), scratch_(2 * kMaxBlock), synthBus_(2 * kMaxBlock),
      commands_(queueCapacity),
      // Each voice can finish once and each command forget one sound, so
      // this many events can't overflow before the owner reads them
      events_(queueCapacity + maxVoices) {}

void Mixer::render(float *out, std::uint32_t frames) noexcept {
  const MixingScope mixing; // counts any allocation made here (there should be none)
  // Commands first, so this block already plays what was asked before it
  Command command;
  while (commands_.pop(command)) {
    apply(command);
  }
  while (frames > 0) {
    const std::uint32_t block = std::min(frames, kMaxBlock);
    mixBlock(out, block);
    out += 2 * block;
    frames -= block;
    stats_.frames.fetch_add(block, std::memory_order_relaxed);
  }
  stats_.blocks.fetch_add(1, std::memory_order_relaxed);
}

void Mixer::apply(const Command &command) noexcept {
  auto voiceWithId = [this](VoiceId id) -> Voice * {
    for (Voice &voice : voices_) {
      if (voice.id == id) {
        return &voice;
      }
    }
    return nullptr;
  };
  switch (command.kind) {
  case Command::Kind::Play:
  case Command::Kind::PlayStream:
  case Command::Kind::PlaySequence: {
    // The owner counts voices, so a free one is always there; if not,
    // report the voice finished rather than lose it
    Voice *free = voiceWithId(0);
    if (!free) {
      report({.kind = Event::Kind::Finished, .voice = command.voice});
      return;
    }
    *free = Voice{.id = command.voice,
                  .sound = command.sound,
                  .stream = command.stream,
                  .sequence = command.sequence,
                  .settings = command.settings,
                  .group = command.group};
    // Start at full gain: a fade-in would blunt every sound's attack
    targetGain(free->settings, free->group, false, free->gain);
    if (free->sequence) {
      synthSettings_ = free->settings;
    }
    stats_.voicesPlaying.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  case Command::Kind::Stop:
    if (Voice *voice = voiceWithId(command.voice)) {
      voice->stopping = true; // fades out over the next block
    }
    return;
  case Command::Kind::Set:
    if (Voice *voice = voiceWithId(command.voice)) {
      voice->settings = command.settings; // gains ramp to the new values
      if (voice->sequence) {
        synthSettings_ = command.settings;
      }
    }
    return;
  case Command::Kind::StopAll:
    for (Voice &voice : voices_) {
      voice.stopping = voice.id != 0;
    }
    return;
  case Command::Kind::Forget:
    // The owner is about to free `sound`: stop what plays it now, without
    // a fade (which would read it for another block), and say so
    for (Voice &voice : voices_) {
      if (voice.id != 0 && voice.sound == command.sound) {
        finish(voice);
      }
    }
    report({.kind = Event::Kind::Forgotten, .sound = command.sound});
    return;
  case Command::Kind::SetGain:
    if (command.master) {
      masterGain_ = command.gain;
    } else {
      groupGain_[static_cast<std::size_t>(command.group)] = command.gain;
    }
    return;
  case Command::Kind::SetSynth:
    // The owner frees the old one once told it's no longer played
    if (synth_) {
      report({.kind = Event::Kind::Forgotten, .synth = synth_});
    }
    synth_ = command.synth;
    return;
  case Command::Kind::Midi:
    if (synth_) {
      synth_->handle(command.midi);
    }
    return;
  }
}

void Mixer::targetGain(const VoiceSettings &settings, Group group, bool stopping,
                       float (&gain)[2]) const noexcept {
  // Balance, as on a stereo amplifier: every sound is stereo, so the centre
  // leaves both sides as recorded, and moving right turns the left side
  // down (and the other way), to silence at the end
  const float pan = std::clamp(settings.pan, -1.0f, 1.0f);
  const float level =
      stopping ? 0.0f : settings.volume * masterGain_ * groupGain_[static_cast<std::size_t>(group)];
  gain[0] = level * (pan > 0.0f ? 1.0f - pan : 1.0f);
  gain[1] = level * (pan < 0.0f ? 1.0f + pan : 1.0f);
}

void Mixer::mixBlock(float *out, std::uint32_t frames) noexcept {
  std::fill(out, out + 2 * frames, 0.0f);
  for (Voice &voice : voices_) {
    if (voice.id == 0) {
      continue;
    }
    if (voice.sequence) { // played by the instrument, in mixSynth
      if (voice.stopping) {
        if (synth_) {
          synth_->allNotesOff();
        }
        finish(voice);
      }
      continue;
    }
    float target[2];
    targetGain(voice.settings, voice.group, voice.stopping, target);
    const bool playing =
        voice.sound ? mixSound(voice, out, frames, target) : mixStream(voice, out, frames, target);
    voice.gain[0] = target[0];
    voice.gain[1] = target[1];
    if (!playing || voice.stopping) {
      finish(voice);
    }
  }
  mixSynth(out, frames);
}

void Mixer::mixSynth(float *out, std::uint32_t frames) noexcept {
  if (!synth_) { // nothing can play a sequence
    for (Voice &voice : voices_) {
      if (voice.id != 0 && voice.sequence) {
        finish(voice);
      }
    }
    return;
  }
  // In steps short enough that each event lands within a millisecond of
  // its time
  constexpr std::uint32_t kStep = 32;
  float *bus = synthBus_.data();
  std::fill(bus, bus + 2 * frames, 0.0f);
  for (std::uint32_t done = 0; done < frames;) {
    const std::uint32_t step = std::min(kStep, frames - done);
    for (Voice &voice : voices_) {
      if (voice.id != 0 && voice.sequence && !advanceSequence(voice, step)) {
        finish(voice);
      }
    }
    synth_->render(bus + 2 * done, step);
    done += step;
  }
  float target[2];
  targetGain(synthSettings_, Group::Music, false, target);
  const float stepLeft = (target[0] - synthGain_[0]) / static_cast<float>(frames);
  const float stepRight = (target[1] - synthGain_[1]) / static_cast<float>(frames);
  float left = synthGain_[0], right = synthGain_[1];
  for (std::uint32_t i = 0; i < frames; ++i) {
    out[2 * i] += bus[2 * i] * left;
    out[2 * i + 1] += bus[2 * i + 1] * right;
    left += stepLeft;
    right += stepRight;
  }
  synthGain_[0] = target[0];
  synthGain_[1] = target[1];
  stats_.notesSounding.store(synth_->notesSounding(), std::memory_order_relaxed);
}

bool Mixer::advanceSequence(Voice &voice, std::uint32_t frames) noexcept {
  const MidiSequence &sequence = *voice.sequence;
  // Pitch is speed here too: 2 plays it twice as fast
  double end = voice.position + frames * static_cast<double>(std::max(voice.settings.pitch, 0.0f));
  for (;;) {
    while (voice.nextEvent < sequence.events.size() &&
           sequence.events[voice.nextEvent].frame < end) {
      synth_->handle(sequence.events[voice.nextEvent].event);
      ++voice.nextEvent;
    }
    if (end < sequence.frames) {
      voice.position = end;
      return true;
    }
    // Past the end, every event played: over, or round again, without
    // notes left hanging
    if (!voice.settings.loop || sequence.frames == 0) {
      return false;
    }
    synth_->allNotesOff();
    end -= sequence.frames;
    voice.nextEvent = 0;
  }
}

bool Mixer::mixSound(Voice &voice, float *out, std::uint32_t frames,
                     const float (&target)[2]) noexcept {
  const Sound &sound = *voice.sound;
  const std::uint32_t length = sound.frames();
  if (length == 0) {
    return false;
  }
  const float *samples = sound.samples.data();
  const double step = std::max(voice.settings.pitch, 0.0f);
  const bool loop = voice.settings.loop;
  // Gains move linearly from last block's to this block's, so changes and
  // stops are ramps rather than clicks
  const float stepLeft = (target[0] - voice.gain[0]) / static_cast<float>(frames);
  const float stepRight = (target[1] - voice.gain[1]) / static_cast<float>(frames);
  float left = voice.gain[0], right = voice.gain[1];
  double position = voice.position;
  for (std::uint32_t i = 0; i < frames; ++i) {
    const auto index = static_cast<std::uint32_t>(position);
    const float fraction = static_cast<float>(position - index);
    // The next frame: the start again if looping, silence after the end
    const std::uint32_t next = index + 1 < length ? index + 1 : (loop ? 0 : length);
    const float l0 = samples[2 * index], r0 = samples[2 * index + 1];
    const float l1 = next < length ? samples[2 * next] : 0.0f;
    const float r1 = next < length ? samples[2 * next + 1] : 0.0f;
    out[2 * i] += (l0 + (l1 - l0) * fraction) * left;
    out[2 * i + 1] += (r0 + (r1 - r0) * fraction) * right;
    left += stepLeft;
    right += stepRight;
    position += step;
    if (position >= length) {
      if (!loop) {
        return false;
      }
      position = std::fmod(position, static_cast<double>(length));
    }
  }
  voice.position = position;
  return true;
}

bool Mixer::mixStream(Voice &voice, float *out, std::uint32_t frames,
                      const float (&target)[2]) noexcept {
  StreamBuffer &stream = *voice.stream;
  // Read whether the decoder had ended before reading the ring: then an
  // empty ring really is the end, not just the decoder falling behind
  const bool ended = stream.ended.load(std::memory_order_acquire);
  const std::uint32_t got = stream.ring.read(scratch_.data(), 2 * frames) / 2;
  if (got < frames && !ended) {
    stats_.underruns.fetch_add(1, std::memory_order_relaxed);
  }
  const float stepLeft = (target[0] - voice.gain[0]) / static_cast<float>(frames);
  const float stepRight = (target[1] - voice.gain[1]) / static_cast<float>(frames);
  float left = voice.gain[0], right = voice.gain[1];
  for (std::uint32_t i = 0; i < got; ++i) {
    out[2 * i] += scratch_[2 * i] * left;
    out[2 * i + 1] += scratch_[2 * i + 1] * right;
    left += stepLeft;
    right += stepRight;
  }
  return !(ended && got < frames);
}

void Mixer::finish(Voice &voice) noexcept {
  report({.kind = Event::Kind::Finished, .voice = voice.id});
  voice = Voice{};
  stats_.voicesPlaying.fetch_sub(1, std::memory_order_relaxed);
}

void Mixer::report(const Event &event) noexcept {
  if (!events_.push(event)) {
    stats_.droppedEvents.fetch_add(1, std::memory_order_relaxed);
  }
}

} // namespace ScratchPadd::Audio
