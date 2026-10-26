#pragma once

// AudioPadd on a Nintendo DS (include AudioPadd.hpp, which picks this there).
//
// The DS has no floating-point hardware, so the audio engine's mixer, which
// mixes in float, is far too slow there. The DS mixes in hardware instead: 16
// sound channels, each playing samples from memory at its own rate, volume
// and pan. This AudioPadd answers the same messages as the mixing one, and
// plays each sound on a channel:
//
// - Sounds are kept in memory as the hardware plays them: 16-bit or 8-bit
//   mono (stereo is mixed down).
//   - LoadSoundPcm16 (no float work at all) and LoadSoundSamples load samples
//     made in code.
//   - LoadSound loads a WAV file (Wav::read: 8- or 16-bit PCM, or IMA-ADPCM,
//     decoded to 16-bit), typically from the app's file system
//     ("nitro:/sounds/door.wav", once the app has called nitroFSInit()).
// - PlaySound plays a sound on a channel, at its rate times its pitch, with
//   its volume and pan (and its group's, the master volume and mute). With
//   every channel busy, it isn't played (Playing says so).
// - PlayMusic plays a WAV file the same way, in the music group: loaded whole
//   (there's no streaming), so keep it short, or IMA-ADPCM.
// - PlayMidi plays a MIDI file (.mid), one at a time: its notes are timed by
//   repeat() (every 10 ms, so a note may be up to 10 ms late) and played on
//   the instrument below. `pitch` is its speed, and `loop` starts it over.
// - PlayMidiEvent plays an event now on the instrument: MadeSounds' note
//   (rendered when the program is built: MadeSoundsPcm16) at each note's
//   pitch (velocity is volume); notes on MIDI's drum channel (10) play a
//   knock. A note rings out its decay: noteOff doesn't cut it short.
// - SetVoice, StopVoice, StopAll and StopMusic change or stop sounds (and a
//   MIDI file).
// - SoundFonts aren't supported (too big for the DS, and played in float),
//   and are answered with why.
//
// VoiceFinished is sent when a sound's length has played (worked out from its
// rate: the hardware doesn't report it), or a MIDI file's.

#include <AudioPadd/AudioPaddControls.hpp>
#include <ScratchPadd/Audio/MadeSounds.hpp>
#include <ScratchPadd/Audio/MadeSoundsPcm16.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/Audio/MidiSequence.hpp>
#include <ScratchPadd/Audio/WavReader.hpp>
#include <ScratchPadd/Base.hpp>

#include <nds.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

class AudioPadd : public ScratchPadd::Base {
public:
  static constexpr int kDrumChannel = 9; // MIDI's channel 10

  AudioPadd(ScratchPadd::System *system) : Base(system) {
    using namespace ScratchPadd::Audio;
    receive<LoadSound>([this](const LoadSound &load, ScratchPadd::PaddAddress from) {
      std::string error;
      if (std::optional<Sound> sound = loadWav(load.path, error)) {
        keep(load.name, std::move(*sound));
      }
      answerLoad(from, load.name, error);
    });
    receive<LoadSoundSamples>([this](const LoadSoundSamples &load, ScratchPadd::PaddAddress from) {
      if (!load.samples || load.channels == 0) {
        answerLoad(from, load.name, "no samples");
        return;
      }
      keep(load.name, mono(*load.samples, load.rate, load.channels));
      answerLoad(from, load.name, "");
    });
    receive<LoadSoundPcm16>([this](const LoadSoundPcm16 &load, ScratchPadd::PaddAddress from) {
      if (!load.samples || load.channels == 0) {
        answerLoad(from, load.name, "no samples");
        return;
      }
      keep(load.name, mono(*load.samples, load.rate, load.channels));
      answerLoad(from, load.name, "");
    });
    receive<UnloadSound>([this](const UnloadSound &unload) { forget(unload.name); });
    receive<PlaySound>([this](const PlaySound &play, ScratchPadd::PaddAddress from) {
      auto sound = sounds_.find(play.name);
      if (sound == sounds_.end()) {
        answerPlay(from, play.name, std::nullopt, "no sound named " + play.name);
        return;
      }
      const std::optional<VoiceId> voice =
          start(sound->second, play.name, play.settings, Group::Sounds, from);
      answerPlay(from, play.name, voice, voice ? "" : "every sound channel is in use");
    });
    receive<PlayMusic>([this](const PlayMusic &play, ScratchPadd::PaddAddress from) {
      std::string error;
      std::optional<Sound> sound = loadWav(play.path, error);
      if (!sound) {
        answerPlay(from, play.path, std::nullopt, error);
        return;
      }
      // Kept while it plays, under its path (a voice points at its sound)
      const Sound &kept = keep(kMusicPrefix + play.path, std::move(*sound));
      const std::optional<VoiceId> voice =
          start(kept, kMusicPrefix + play.path, play.settings, Group::Music, from);
      answerPlay(from, play.path, voice, voice ? "" : "every sound channel is in use");
    });
    receive<PlayMidi>(
        [this](const PlayMidi &play, ScratchPadd::PaddAddress from) { playMidiFile(play, from); });
    receive<PlayMidiEvent>([this](const PlayMidiEvent &play) { midi(play.event, 1.0f, 0.0f); });
    receive<LoadSoundFont>([this](const LoadSoundFont &load, ScratchPadd::PaddAddress from) {
      reply(from, SoundFontLoaded{.path = load.path,
                                  .ok = false,
                                  .error = "SoundFonts aren't supported on the DS"});
    });
    receive<UseBuiltInInstrument>([](const UseBuiltInInstrument &) {}); // the only one
    receive<StopVoice>([this](const StopVoice &stop) {
      if (sequence_ && sequence_->voice == stop.voice) {
        stopSequence();
      } else {
        stopVoice(stop.voice);
      }
    });
    receive<SetVoice>([this](const SetVoice &set) {
      auto voice = voices_.find(set.voice);
      if (voice != voices_.end()) {
        voice->second.settings = set.settings;
        apply(voice->second);
      }
    });
    receive<StopMusic>([this](const StopMusic &) {
      stopSequence();
      stopGroup(Group::Music);
    });
    receive<StopAll>([this](const StopAll &) {
      stopSequence();
      stopGroup(Group::Sounds);
      stopGroup(Group::Music);
    });
  }

  std::string name() override { return "AudioPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return audioPaddControls(); }
  void controlChanged(const std::string &) override {
    for (auto &[id, voice] : voices_) {
      apply(voice);
    }
  }

  // Plays a MIDI file's notes as they come due, and reports voices that
  // have finished, promptly
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::milliseconds(10);
  }
  void repeat() override {
    const auto now = Clock::now();
    advanceSequence(now);
    std::vector<std::pair<ScratchPadd::Audio::VoiceId, Voice>> done;
    std::erase_if(voices_, [&](auto &entry) {
      if (!entry.second.ends || now < *entry.second.ends) {
        return false;
      }
      done.emplace_back(entry.first, entry.second);
      return true;
    });
    for (const auto &[id, voice] : done) {
      finished(voice, id);
    }
  }

  void starting() override {
    soundEnable();
    using namespace ScratchPadd::Audio;
    note_ = mono(MadeSoundsPcm16::note(), MadeSoundsPcm16::kRate, 1);
    knock_ = mono(MadeSoundsPcm16::knock(), MadeSoundsPcm16::kRate, 1);
  }
  void finishing() override {
    stopSequence();
    stopGroup(ScratchPadd::Audio::Group::Sounds);
    stopGroup(ScratchPadd::Audio::Group::Music);
    soundDisable();
  }

private:
  using Clock = std::chrono::steady_clock;
  // MIDI files are read with times in milliseconds
  static constexpr std::uint32_t kMidiTicksPerSecond = 1000;
  // A music file is kept among the sounds under its path, after this
  static constexpr const char *kMusicPrefix = "music:";

  // A sound as the hardware plays it: 16-bit or 8-bit mono, at `rate`
  struct Sound {
    std::vector<std::int16_t> samples16;
    std::vector<std::int8_t> samples8; // (8-bit sounds only)
    std::uint32_t rate = 0;

    bool eightBit() const { return !samples8.empty(); }
    const void *data() const {
      return eightBit() ? static_cast<const void *>(samples8.data()) : samples16.data();
    }
    std::size_t bytes() const {
      return eightBit() ? samples8.size() : samples16.size() * sizeof(std::int16_t);
    }
    std::size_t frames() const { return eightBit() ? samples8.size() : samples16.size(); }
  };
  struct Voice {
    int channel = -1;
    std::string sound;
    const Sound *data = nullptr;
    ScratchPadd::Audio::VoiceSettings settings;
    ScratchPadd::Audio::Group group = ScratchPadd::Audio::Group::Sounds;
    ScratchPadd::PaddAddress from;
    std::optional<Clock::time_point> ends; // none while it loops
  };
  // The MIDI file playing
  struct Sequence {
    std::shared_ptr<const ScratchPadd::Audio::MidiSequence> midi;
    std::string path;
    ScratchPadd::Audio::VoiceSettings settings;
    ScratchPadd::PaddAddress from;
    ScratchPadd::Audio::VoiceId voice = 0; // for Playing and VoiceFinished
    Clock::time_point started;
    std::size_t next = 0; // the next event to play
  };

  // `interleaved` (float, 16-bit or 8-bit) as a sound the hardware can play:
  // mono, stereo mixed down; 8-bit stays 8-bit, the rest is 16-bit. The
  // hardware reads main memory directly, so it's written out of the CPU's
  // cache.
  template <typename Sample>
  static Sound mono(const std::vector<Sample> &interleaved, std::uint32_t rate,
                    std::uint32_t channels) {
    Sound sound{.rate = rate};
    const std::size_t frames = interleaved.size() / channels;
    if constexpr (std::is_floating_point_v<Sample>) {
      sound.samples16.resize(frames);
      for (std::size_t f = 0; f < frames; ++f) {
        float mixed = 0;
        for (std::uint32_t c = 0; c < channels; ++c) {
          mixed += interleaved[f * channels + c];
        }
        sound.samples16[f] =
            static_cast<std::int16_t>(std::clamp(mixed / channels, -1.0f, 1.0f) * 32767);
      }
    } else {
      // Integer samples are averaged in integers: no float
      std::vector<Sample> &out = [&]() -> std::vector<Sample> & {
        if constexpr (std::is_same_v<Sample, std::int8_t>) {
          return sound.samples8;
        } else {
          return sound.samples16;
        }
      }();
      out.resize(frames);
      for (std::size_t f = 0; f < frames; ++f) {
        int mixed = 0;
        for (std::uint32_t c = 0; c < channels; ++c) {
          mixed += interleaved[f * channels + c];
        }
        out[f] = static_cast<Sample>(mixed / static_cast<int>(channels));
      }
    }
    DC_FlushRange(sound.data(), sound.bytes());
    return sound;
  }

  static std::optional<std::vector<std::uint8_t>> readFile(const std::string &path,
                                                           std::string &error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      error = "can't open " + path;
      return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
  }

  static std::optional<Sound> loadWav(const std::string &path, std::string &error) {
    std::optional<std::vector<std::uint8_t>> bytes = readFile(path, error);
    if (!bytes) {
      return std::nullopt;
    }
    ScratchPadd::Audio::Wav::Sound wav;
    if (!ScratchPadd::Audio::Wav::read(bytes->data(), bytes->size(), wav, error)) {
      error = path + ": " + error;
      return std::nullopt;
    }
    return wav.bits == 8 ? mono(wav.samples8, wav.rate, wav.channels)
                         : mono(wav.samples16, wav.rate, wav.channels);
  }

  // The voice's volume, with its group's, the master volume and mute
  float loudness(const Voice &voice) const {
    using ScratchPadd::ControlType::Boolean;
    using ScratchPadd::ControlType::Double;
    if (control<Boolean>("mute").value) {
      return 0;
    }
    const char *group = voice.group == ScratchPadd::Audio::Group::Music ? "music" : "sounds";
    return voice.settings.volume *
           static_cast<float>(control<Double>("master").value * control<Double>(group).value);
  }
  static u8 hardwareVolume(float volume) {
    return static_cast<u8>(std::clamp(volume, 0.0f, 1.0f) * 127);
  }
  static u8 hardwarePan(float pan) {
    return static_cast<u8>(64 + std::clamp(pan, -1.0f, 1.0f) * 63);
  }
  static u16 hardwareRate(const Sound &sound, float pitch) {
    return static_cast<u16>(std::clamp(sound.rate * pitch, 1.0f, 65535.0f));
  }

  std::optional<ScratchPadd::Audio::VoiceId>
  start(const Sound &sound, const std::string &name,
        const ScratchPadd::Audio::VoiceSettings &settings, ScratchPadd::Audio::Group group,
        ScratchPadd::PaddAddress from) {
    Voice voice{.sound = name, .data = &sound, .settings = settings, .group = group, .from = from};
    const u16 rate = hardwareRate(sound, settings.pitch);
    voice.channel = soundPlaySample(
        sound.data(), sound.eightBit() ? SoundFormat_8Bit : SoundFormat_16Bit, sound.bytes(), rate,
        hardwareVolume(loudness(voice)), hardwarePan(settings.pan), settings.loop, 0);
    if (voice.channel < 0) {
      return std::nullopt;
    }
    if (!settings.loop) {
      voice.ends = Clock::now() +
                   std::chrono::duration_cast<Clock::duration>(
                       std::chrono::duration<double>(static_cast<double>(sound.frames()) / rate));
    }
    // A channel the hardware has given again belongs to the new voice now
    std::erase_if(voices_,
                  [&](const auto &entry) { return entry.second.channel == voice.channel; });
    const ScratchPadd::Audio::VoiceId id = nextVoice_++;
    voices_[id] = voice;
    return id;
  }

  void apply(const Voice &voice) {
    soundSetVolume(voice.channel, hardwareVolume(loudness(voice)));
    soundSetPan(voice.channel, hardwarePan(voice.settings.pan));
    soundSetFreq(voice.channel, hardwareRate(*voice.data, voice.settings.pitch));
  }

  // Stops the voices playing sound `name` (the hardware reads its samples
  // while they play) and lets the sound go
  void forget(const std::string &name) {
    std::vector<ScratchPadd::Audio::VoiceId> playing;
    for (const auto &[id, voice] : voices_) {
      if (voice.sound == name) {
        playing.push_back(id);
      }
    }
    for (ScratchPadd::Audio::VoiceId id : playing) {
      stopVoice(id);
    }
    sounds_.erase(name);
  }

  // Keeps `sound` as `name`, replacing (and first stopping) any sound of
  // that name
  const Sound &keep(const std::string &name, Sound sound) {
    forget(name);
    return sounds_[name] = std::move(sound);
  }

  // Tells whoever played `voice` that it's done; a music file's sound is let
  // go with it
  void finished(const Voice &voice, ScratchPadd::Audio::VoiceId id) {
    const bool music = voice.sound.starts_with(kMusicPrefix);
    reply(voice.from,
          ScratchPadd::Audio::VoiceFinished{
              .name = music ? voice.sound.substr(std::string(kMusicPrefix).size()) : voice.sound,
              .voice = id});
    if (music) {
      sounds_.erase(voice.sound);
    }
  }

  void stopVoice(ScratchPadd::Audio::VoiceId id) {
    auto voice = voices_.find(id);
    if (voice != voices_.end()) {
      soundKill(voice->second.channel);
      const Voice stopped = voice->second;
      voices_.erase(voice);
      finished(stopped, id);
    }
  }

  void stopGroup(ScratchPadd::Audio::Group group) {
    std::vector<ScratchPadd::Audio::VoiceId> stopping;
    for (const auto &[id, voice] : voices_) {
      if (voice.group == group) {
        stopping.push_back(id);
      }
    }
    for (ScratchPadd::Audio::VoiceId id : stopping) {
      stopVoice(id);
    }
  }

  // A note on the instrument now, at `volume` and `pan` (the MIDI file's, or
  // as made for an event)
  void midi(const ScratchPadd::Audio::MidiEvent &event, float volume, float pan) {
    using ScratchPadd::Audio::MidiEvent;
    if (event.kind() != MidiEvent::NoteOn || event.data2 == 0) {
      return; // (noteOff: the note rings out its decay)
    }
    const bool drum = event.channel() == kDrumChannel;
    const float pitch =
        drum ? 1.0f
             : std::pow(2.0f, (event.data1 - ScratchPadd::Audio::MadeSounds::kNoteKey) / 12.0f);
    start(drum ? knock_ : note_, drum ? "drum" : "note",
          {.volume = volume * event.data2 / 127.0f, .pan = pan, .pitch = pitch},
          ScratchPadd::Audio::Group::Music, ScratchPadd::PaddAddress());
  }

  void playMidiFile(const ScratchPadd::Audio::PlayMidi &play, ScratchPadd::PaddAddress from) {
    std::string error;
    std::shared_ptr<const ScratchPadd::Audio::MidiSequence> midi;
    if (std::optional<std::vector<std::uint8_t>> bytes = readFile(play.path, error)) {
      midi = ScratchPadd::Audio::readMidi(bytes->data(), bytes->size(), kMidiTicksPerSecond, error);
    }
    if (!midi) {
      answerPlay(from, play.path, std::nullopt, error);
      return;
    }
    stopSequence(); // one at a time
    sequence_ = Sequence{.midi = midi,
                         .path = play.path,
                         .settings = play.settings,
                         .from = from,
                         .voice = nextVoice_++,
                         .started = Clock::now()};
    answerPlay(from, play.path, sequence_->voice, "");
  }

  // Plays the MIDI file's events that are due at `now`; at its end, starts
  // it over (looping) or reports it finished
  void advanceSequence(Clock::time_point now) {
    if (!sequence_) {
      return;
    }
    Sequence &s = *sequence_;
    const double speed = std::max(s.settings.pitch, 0.01f);
    const auto elapsed = static_cast<std::uint32_t>(
        std::chrono::duration<double, std::milli>(now - s.started).count() * speed);
    const auto &events = s.midi->events;
    while (s.next < events.size() && events[s.next].frame <= elapsed) {
      midi(events[s.next].event, s.settings.volume, s.settings.pan);
      ++s.next;
    }
    if (elapsed < s.midi->frames) {
      return;
    }
    if (s.settings.loop) {
      s.started += std::chrono::duration_cast<Clock::duration>(
          std::chrono::duration<double, std::milli>(s.midi->frames / speed));
      s.next = 0;
      return;
    }
    stopSequence();
  }

  // Stops the MIDI file (its notes ring out) and tells whoever played it
  void stopSequence() {
    if (sequence_) {
      reply(sequence_->from,
            ScratchPadd::Audio::VoiceFinished{.name = sequence_->path, .voice = sequence_->voice});
      sequence_.reset();
    }
  }

  void answerLoad(ScratchPadd::PaddAddress from, const std::string &name,
                  const std::string &error) {
    reply(from, ScratchPadd::Audio::SoundLoaded{.name = name, .ok = error.empty(), .error = error});
  }

  void answerPlay(ScratchPadd::PaddAddress from, const std::string &name,
                  std::optional<ScratchPadd::Audio::VoiceId> voice, const std::string &error) {
    reply(from,
          ScratchPadd::Audio::Playing{
              .name = name, .voice = voice.value_or(0), .ok = voice.has_value(), .error = error});
  }

  std::map<std::string, Sound> sounds_;
  std::map<ScratchPadd::Audio::VoiceId, Voice> voices_;
  std::optional<Sequence> sequence_;
  ScratchPadd::Audio::VoiceId nextVoice_ = 1;
  Sound note_, knock_; // the instrument
};
