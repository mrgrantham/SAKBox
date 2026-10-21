#include <ScratchPadd/Audio/Mixer.hpp>

#include <algorithm>
#include <cmath>

namespace ScratchPadd::Audio {

Mixer::Mixer(std::uint32_t maxVoices, std::uint32_t queueCapacity)
    : voices_(maxVoices), scratch_(2 * kMaxBlock), commands_(queueCapacity),
      // Each voice can finish once and each command forget one sound, so
      // this many events can't overflow before the owner reads them
      events_(queueCapacity + maxVoices) {}

void Mixer::render(float *out, std::uint32_t frames) noexcept {
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
  case Command::Kind::PlayStream: {
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
                  .settings = command.settings,
                  .group = command.group};
    // Start at full gain: a fade-in would blunt every sound's attack
    targetGain(*free, free->gain);
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
  }
}

void Mixer::targetGain(const Voice &voice, float (&gain)[2]) const noexcept {
  // Balance, as on a stereo amplifier: every sound is stereo, so the centre
  // leaves both sides as recorded, and moving right turns the left side
  // down (and the other way), to silence at the end
  const float pan = std::clamp(voice.settings.pan, -1.0f, 1.0f);
  const float level = voice.stopping ? 0.0f
                                     : voice.settings.volume * masterGain_ *
                                           groupGain_[static_cast<std::size_t>(voice.group)];
  gain[0] = level * (pan > 0.0f ? 1.0f - pan : 1.0f);
  gain[1] = level * (pan < 0.0f ? 1.0f + pan : 1.0f);
}

void Mixer::mixBlock(float *out, std::uint32_t frames) noexcept {
  std::fill(out, out + 2 * frames, 0.0f);
  for (Voice &voice : voices_) {
    if (voice.id == 0) {
      continue;
    }
    float target[2];
    targetGain(voice, target);
    const bool playing =
        voice.sound ? mixSound(voice, out, frames, target) : mixStream(voice, out, frames, target);
    voice.gain[0] = target[0];
    voice.gain[1] = target[1];
    if (!playing || voice.stopping) {
      finish(voice);
    }
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
