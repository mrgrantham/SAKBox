#pragma once

#include <ScratchPadd/Audio/DeviceOutput.hpp>
#include <ScratchPadd/Audio/Engine.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/Base.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

// Plays sounds and music for the other padds, which ask with the messages
// in ScratchPadd/Audio/Messages.hpp. The engine mixes on the sound device's
// own thread; this padd's thread does the slow work (loading files, decoding
// music ahead) and tells each padd when the voices it started finish.
// Controls: master, sounds and music volume, and mute.
class AudioPadd : public ScratchPadd::Base {
public:
  // What AudioPadd plays through, made when it's constructed: the sound
  // device, unless set otherwise first (tests use an OfflineOutput).
  using OutputFactory = std::function<std::unique_ptr<ScratchPadd::Audio::Output>()>;
  static OutputFactory &outputFactory() {
    static OutputFactory factory = [] {
      return std::make_unique<ScratchPadd::Audio::DeviceOutput>();
    };
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

  ScratchPadd::ControlValueMap generateControls() override {
    const auto volume = [](double initial) {
      return ScratchPadd::ControlType::Double(initial, std::make_pair(0.0, 1.0));
    };
    return {{"master", volume(1.0)},
            {"sounds", volume(1.0)},
            {"music", volume(1.0)},
            {"mute", ScratchPadd::ControlType::Boolean(false)}};
  }

  void controlChanged(const std::string &) override { applyVolumes(); }

  // Often enough that music decoded ahead never runs dry, and finished
  // voices are reported promptly
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::milliseconds(20);
  }
  void repeat() override {
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
    } else {
      outputError_ = "no sound output: " + error;
      logger().error("{}: {}", paddName(), outputError_);
    }
  }
  void finishing() override { engine_.stop(); }

private:
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

  std::unique_ptr<ScratchPadd::Audio::Output> output_;
  ScratchPadd::Audio::Engine engine_;
  std::string outputError_; // set if the output couldn't start
  std::map<ScratchPadd::Audio::VoiceId, Owner> owners_;
};
