#pragma once

// AudioPadd on a Nintendo DS (include AudioPadd.hpp, which picks this there).
//
// The DS has no floating-point hardware, so the audio engine's mixer, which
// mixes in float, is far too slow there. The DS mixes in hardware instead: 16
// sound channels, each playing a sample from memory at its own rate, volume
// and pan. This AudioPadd answers the same messages as the mixing one, and
// plays each sound on a channel:
//
// - LoadSoundPcm16 (best here: no float work at all) and LoadSoundSamples:
//   the samples are kept as 16-bit mono (stereo is mixed down), in memory
//   for the sound hardware to read.
// - PlaySound: a channel, at the sound's rate times its pitch, with its
//   volume and pan (and the group's, master and mute controls). With every
//   channel busy, the sound isn't played (Playing says so).
// - SetVoice, StopVoice, StopAll, StopMusic: change or stop channels.
// - PlayMidiEvent: a simple instrument, MadeSounds' note, rendered when the
//   program is built (MadeSoundsPcm16), played at each note's pitch
//   (velocity is volume); notes on MIDI's drum channel (10) play a knock. A
//   note rings out its decay: noteOff doesn't cut it short.
// - Files (LoadSound, PlayMusic, PlayMidi) and SoundFonts aren't supported:
//   the DS app has no file system set up, and they're answered with why.
//
// VoiceFinished is sent when a sound's length has played (worked out from its
// rate), as the hardware doesn't report it.

#include <AudioPadd/AudioPaddControls.hpp>
#include <ScratchPadd/Audio/MadeSounds.hpp>
#include <ScratchPadd/Audio/MadeSoundsPcm16.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/Base.hpp>

#include <nds.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
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
      answerLoad(from, load.name, "sound files aren't supported on the DS");
    });
    receive<LoadSoundSamples>([this](const LoadSoundSamples &load, ScratchPadd::PaddAddress from) {
      if (!load.samples || load.channels == 0) {
        answerLoad(from, load.name, "no samples");
        return;
      }
      sounds_[load.name] = mono(*load.samples, load.rate, load.channels);
      answerLoad(from, load.name, "");
    });
    receive<LoadSoundPcm16>([this](const LoadSoundPcm16 &load, ScratchPadd::PaddAddress from) {
      if (!load.samples || load.channels == 0) {
        answerLoad(from, load.name, "no samples");
        return;
      }
      sounds_[load.name] = mono(*load.samples, load.rate, load.channels);
      answerLoad(from, load.name, "");
    });
    receive<UnloadSound>([this](const UnloadSound &unload) {
      std::erase_if(voices_, [&](auto &entry) {
        if (entry.second.sound == unload.name) {
          soundKill(entry.second.channel);
          return true;
        }
        return false;
      });
      sounds_.erase(unload.name);
    });
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
      answerPlay(from, play.path, std::nullopt, "music files aren't supported on the DS");
    });
    receive<PlayMidi>([this](const PlayMidi &play, ScratchPadd::PaddAddress from) {
      answerPlay(from, play.path, std::nullopt, "MIDI files aren't supported on the DS");
    });
    receive<PlayMidiEvent>([this](const PlayMidiEvent &play) { midi(play.event); });
    receive<LoadSoundFont>([this](const LoadSoundFont &load, ScratchPadd::PaddAddress from) {
      reply(from, SoundFontLoaded{.path = load.path,
                                  .ok = false,
                                  .error = "SoundFonts aren't supported on the DS"});
    });
    receive<UseBuiltInInstrument>([](const UseBuiltInInstrument &) {}); // the only one
    receive<StopVoice>([this](const StopVoice &stop) { stopVoice(stop.voice); });
    receive<SetVoice>([this](const SetVoice &set) {
      auto voice = voices_.find(set.voice);
      if (voice != voices_.end()) {
        voice->second.settings = set.settings;
        apply(voice->second);
      }
    });
    receive<StopMusic>([this](const StopMusic &) { stopGroup(Group::Music); });
    receive<StopAll>([this](const StopAll &) {
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

  // Reports voices that have finished, promptly
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::milliseconds(20);
  }
  void repeat() override {
    const auto now = Clock::now();
    std::erase_if(voices_, [&](auto &entry) {
      Voice &voice = entry.second;
      if (!voice.ends || now < *voice.ends) {
        return false;
      }
      reply(voice.from,
            ScratchPadd::Audio::VoiceFinished{.name = voice.sound, .voice = entry.first});
      return true;
    });
  }

  void starting() override {
    soundEnable();
    using namespace ScratchPadd::Audio;
    note_ = mono(MadeSoundsPcm16::note(), MadeSoundsPcm16::kRate, 1);
    knock_ = mono(MadeSoundsPcm16::knock(), MadeSoundsPcm16::kRate, 1);
  }
  void finishing() override {
    stopGroup(ScratchPadd::Audio::Group::Sounds);
    stopGroup(ScratchPadd::Audio::Group::Music);
    soundDisable();
  }

private:
  using Clock = std::chrono::steady_clock;

  // A sound as the hardware plays it: 16-bit mono at `rate`
  struct Sound {
    std::vector<std::int16_t> samples;
    std::uint32_t rate = 0;
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

  // `interleaved` (float or 16-bit) as a sound the hardware can play: 16-bit
  // mono, stereo mixed down
  template <typename Sample>
  static Sound mono(const std::vector<Sample> &interleaved, std::uint32_t rate,
                    std::uint32_t channels) {
    constexpr float kFullScale = std::is_floating_point_v<Sample> ? 1.0f : 32767.0f;
    Sound sound{.rate = rate};
    const std::size_t frames = interleaved.size() / channels;
    sound.samples.resize(frames);
    for (std::size_t f = 0; f < frames; ++f) {
      if (channels == 1 && !std::is_floating_point_v<Sample>) {
        sound.samples[f] = static_cast<std::int16_t>(interleaved[f]); // as it is: no float
        continue;
      }
      float mixed = 0;
      for (std::uint32_t c = 0; c < channels; ++c) {
        mixed += static_cast<float>(interleaved[f * channels + c]) / kFullScale;
      }
      sound.samples[f] =
          static_cast<std::int16_t>(std::clamp(mixed / channels, -1.0f, 1.0f) * 32767);
    }
    // The sound hardware reads main memory directly: write it out of the
    // CPU's cache
    DC_FlushRange(sound.samples.data(), sound.samples.size() * sizeof(std::int16_t));
    return sound;
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
        sound.samples.data(), SoundFormat_16Bit, sound.samples.size() * sizeof(std::int16_t), rate,
        hardwareVolume(loudness(voice)), hardwarePan(settings.pan), settings.loop, 0);
    if (voice.channel < 0) {
      return std::nullopt;
    }
    if (!settings.loop) {
      voice.ends =
          Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(
                             static_cast<double>(sound.samples.size()) / rate));
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

  void stopVoice(ScratchPadd::Audio::VoiceId id) {
    auto voice = voices_.find(id);
    if (voice != voices_.end()) {
      soundKill(voice->second.channel);
      reply(voice->second.from,
            ScratchPadd::Audio::VoiceFinished{.name = voice->second.sound, .voice = id});
      voices_.erase(voice);
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

  void midi(const ScratchPadd::Audio::MidiEvent &event) {
    using ScratchPadd::Audio::MidiEvent;
    if (event.kind() != MidiEvent::NoteOn || event.data2 == 0) {
      return; // (noteOff: the note rings out its decay)
    }
    const bool drum = event.channel() == kDrumChannel;
    const float pitch =
        drum ? 1.0f
             : std::pow(2.0f, (event.data1 - ScratchPadd::Audio::MadeSounds::kNoteKey) / 12.0f);
    start(drum ? knock_ : note_, drum ? "drum" : "note",
          {.volume = event.data2 / 127.0f, .pan = 0, .pitch = pitch},
          ScratchPadd::Audio::Group::Music, ScratchPadd::PaddAddress());
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
  ScratchPadd::Audio::VoiceId nextVoice_ = 1;
  Sound note_, knock_; // the instrument
};
