#pragma once

// AudioPadd on desktops and the 3DS: mixes in software with the audio engine
// (include AudioPadd.hpp, which picks this or the DS's)

#include <AudioPadd/AudioPaddControls.hpp>
#include <AudioPadd/ControlPreview.hpp>
#include <ScratchPadd/Audio/Engine.hpp>
#include <ScratchPadd/Audio/MadeSounds.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/Base.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The platform's sound output: the 3DS's DSP there, the sound device
// (miniaudio) on desktops
#if defined(__3DS__)
#include <ScratchPadd/Audio/NdspOutput.hpp>
using PlatformAudioOutput = ScratchPadd::Audio::NdspOutput;
#else
#include <ScratchPadd/Audio/DeviceOutput.hpp>
using PlatformAudioOutput = ScratchPadd::Audio::DeviceOutput;
#endif

// Plays sounds and music for the other padds, which ask with the messages
// in ScratchPadd/Audio/Messages.hpp. The engine mixes on the sound device's
// own thread; this padd's thread does the slow work (loading files, decoding
// music ahead) and tells each padd when the voices it started finish.
// Controls: master, sounds and music volume, and mute; a change from a
// control panel is heard at its new level (AudioPadd/ControlPreview.hpp).
class AudioPadd : public ScratchPadd::Base {
public:
  // What AudioPadd plays through, made when it's constructed: the
  // platform's (PlatformAudioOutput: the sound device, or a 3DS's DSP),
  // unless set otherwise first (tests use an OfflineOutput).
  using OutputFactory = std::function<std::unique_ptr<ScratchPadd::Audio::Output>()>;
  static OutputFactory &outputFactory() {
    static OutputFactory factory = [] { return std::make_unique<PlatformAudioOutput>(); };
    return factory;
  }

  AudioPadd(ScratchPadd::System *system)
      : Base(system), output_(outputFactory()()), engine_(*output_) {
    using namespace ScratchPadd::Audio;
    receive<LoadSound>([this](const LoadSound &load, ScratchPadd::PaddAddress from) {
      answerLoad(from, load.name, engine_.loadFile(load.name, load.path));
    });
    receive<LoadSoundSamples>([this](const LoadSoundSamples &load, ScratchPadd::PaddAddress from) {
      answerLoad(from, load.name,
                 load.samples ? engine_.loadSamples(load.name, load.samples->data(),
                                                    load.samples->size(), load.rate, load.channels)
                              : LoadResult{.error = "no samples"});
    });
    receive<LoadSoundPcm16>([this](const LoadSoundPcm16 &load, ScratchPadd::PaddAddress from) {
      if (!load.samples) {
        answerLoad(from, load.name, LoadResult{.error = "no samples"});
        return;
      }
      std::vector<float> samples(load.samples->size());
      std::transform(load.samples->begin(), load.samples->end(), samples.begin(),
                     [](std::int16_t sample) { return sample / 32767.0f; });
      answerLoad(
          from, load.name,
          engine_.loadSamples(load.name, samples.data(), samples.size(), load.rate, load.channels));
    });
    receive<UnloadSound>([this](const UnloadSound &unload) { engine_.unload(unload.name); });
    receive<PlaySound>([this](const PlaySound &play, ScratchPadd::PaddAddress from) {
      std::optional<VoiceId> voice;
      std::string error = outputError_;
      if (error.empty()) {
        voice = engine_.play(play.name, play.settings);
        if (!voice) {
          error =
              engine_.hasSound(play.name) ? "every voice is in use" : "no sound named " + play.name;
        }
      }
      answerPlay(from, play.name, voice, error);
    });
    receive<PlayMusic>([this](const PlayMusic &play, ScratchPadd::PaddAddress from) {
      std::optional<VoiceId> voice;
      std::string error = outputError_;
      if (error.empty()) {
        voice = engine_.playMusic(play.path, play.settings, error);
      }
      answerPlay(from, play.path, voice, error, /*music=*/true);
    });
    receive<PlayMidi>([this](const PlayMidi &play, ScratchPadd::PaddAddress from) {
      std::optional<VoiceId> voice;
      std::string error = outputError_;
      if (error.empty()) {
        voice = engine_.playMidi(play.path, play.settings, error);
      }
      answerPlay(from, play.path, voice, error, /*music=*/true);
    });
    receive<PlayMidiEvent>([this](const PlayMidiEvent &play) { engine_.midi(play.event); });
    receive<LoadSoundFont>([this](const LoadSoundFont &load, ScratchPadd::PaddAddress from) {
      const auto started = std::chrono::steady_clock::now();
      const LoadResult result = engine_.loadSoundFont(load.path);
      if (result.ok) {
        logger().info(
            "{}: loaded {} in {:.1f} s", paddName(), load.path,
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
      } else {
        logger().error("{}: {}", paddName(), result.error);
      }
      reply(from, SoundFontLoaded{.path = load.path, .ok = result.ok, .error = result.error});
    });
    receive<UseBuiltInInstrument>(
        [this](const UseBuiltInInstrument &) { engine_.useBuiltInInstrument(); });
    receive<StopVoice>([this](const StopVoice &stop) { engine_.stop(stop.voice); });
    receive<SetVoice>([this](const SetVoice &set) { engine_.set(set.voice, set.settings); });
    receive<StopMusic>([this](const StopMusic &) {
      for (const auto &[voice, owner] : owners_) {
        if (owner.music) {
          engine_.stop(voice);
        }
      }
    });
    receive<StopAll>([this](const StopAll &) { engine_.stopAll(); });
  }

  std::string name() override { return __CLASS_NAME__; }

  ScratchPadd::ControlValueMap generateControls() override { return audioPaddControls(); }

  void controlChanged(const std::string &name,
                      const ScratchPadd::ControlTypeVariant &previous) override {
    applyVolumes();
    preview(name, previous);
  }

  // Often enough that music decoded ahead never runs dry, and finished
  // voices are reported promptly
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::milliseconds(20);
  }
  void repeat() override {
    releasePreviewNotes();
    for (const ScratchPadd::Audio::VoiceId voice : engine_.update()) {
      const auto owner = owners_.find(voice);
      if (owner != owners_.end()) {
        reply(owner->second.from,
              ScratchPadd::Audio::VoiceFinished{.name = owner->second.name, .voice = voice});
        owners_.erase(owner);
      }
    }
  }

  void starting() override {
    applyVolumes();
    std::string error;
    if (engine_.start(error)) {
      logger().info("{}: playing at {} Hz", paddName(), engine_.sampleRate());
      const std::vector<float> note = ScratchPadd::Audio::MadeSounds::note(engine_.sampleRate());
      engine_.loadSamples(kPreviewNote, note.data(), note.size(), engine_.sampleRate(), 1);
    } else {
      outputError_ = "no sound output: " + error;
      logger().error("{}: {}", paddName(), outputError_);
    }
  }
  void finishing() override { engine_.stop(); }

private:
  using Clock = std::chrono::steady_clock;
  // The note the sounds' previews play, kept among the sounds
  static constexpr const char *kPreviewNote = "AudioPadd preview note";

  struct Owner {
    ScratchPadd::PaddAddress from; // the padd that started the voice
    std::string name;
    bool music = false;
  };

  void answerLoad(ScratchPadd::PaddAddress from, const std::string &name,
                  const ScratchPadd::Audio::LoadResult &result) {
    if (!result.ok) {
      logger().error("{}: {}", paddName(), result.error);
    }
    reply(from,
          ScratchPadd::Audio::SoundLoaded{.name = name, .ok = result.ok, .error = result.error});
  }

  void answerPlay(ScratchPadd::PaddAddress from, const std::string &name,
                  std::optional<ScratchPadd::Audio::VoiceId> voice, const std::string &error,
                  bool music = false) {
    if (voice) {
      owners_[*voice] = {.from = from, .name = name, .music = music};
    } else {
      logger().warn("{}: can't play {}: {}", paddName(), name, error);
    }
    reply(from, ScratchPadd::Audio::Playing{.name = name,
                                            .voice = voice.value_or(0),
                                            .ok = voice.has_value(),
                                            .error = voice ? "" : error});
  }

  void applyVolumes() {
    using ScratchPadd::ControlType::Boolean;
    using ScratchPadd::ControlType::Double;
    const auto volume = [this](const char *name) {
      return static_cast<float>(control<Double>(name).value);
    };
    engine_.setMasterVolume(control<Boolean>("mute").value ? 0.0f : volume("master"));
    engine_.setGroupVolume(ScratchPadd::Audio::Group::Sounds, volume("sounds"));
    engine_.setGroupVolume(ScratchPadd::Audio::Group::Music, volume("music"));
  }

  // Plays what a change to control `name` sounds like at its new level:
  // notes among the sounds (the made note, at each key's pitch), or a chord
  // on the instrument, released after kPreviewLength
  void preview(const std::string &name, const ScratchPadd::ControlTypeVariant &previous) {
    using namespace ScratchPadd::Audio;
    if (!control<ScratchPadd::ControlType::Boolean>("previews").value) {
      return;
    }
    const auto preview = AudioPaddPreview::forChange(name, previous, controlValue(name));
    if (!preview || !outputError_.empty() || !previewPacer_.start(Clock::now())) {
      return;
    }
    for (const int key : preview->keys) {
      if (preview->group == Group::Music) {
        const int velocity = static_cast<int>(preview->volume * 127);
        engine_.midi(MidiEvent::noteOn(AudioPaddPreview::kChannel, key, velocity));
        previewNotes_.emplace_back(Clock::now() + AudioPaddPreview::kPreviewLength, key);
      } else {
        const float pitch = std::exp2((key - MadeSounds::kNoteKey) / 12.0f);
        engine_.play(kPreviewNote, {.volume = preview->volume, .pitch = pitch});
      }
    }
  }

  // Releases the instrument's preview notes whose time is up
  void releasePreviewNotes() {
    const auto now = Clock::now();
    std::erase_if(previewNotes_, [&](const std::pair<Clock::time_point, int> &note) {
      if (now < note.first) {
        return false;
      }
      engine_.midi(ScratchPadd::Audio::MidiEvent::noteOff(AudioPaddPreview::kChannel, note.second));
      return true;
    });
  }

  std::unique_ptr<ScratchPadd::Audio::Output> output_;
  ScratchPadd::Audio::Engine engine_;
  std::string outputError_; // set if the output couldn't start
  std::map<ScratchPadd::Audio::VoiceId, Owner> owners_;
  AudioPaddPreview::Pacer previewPacer_;
  std::vector<std::pair<Clock::time_point, int>> previewNotes_; // (release at, key)
};
