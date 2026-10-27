// FlightSoundPadd.hpp - the game's sounds: turns the simulation's events
// into sounds AudioPadd plays. The simulation knows nothing of sound; this
// padd hears the events it broadcasts, as EventLogPadd does, and asks
// AudioPadd (found by name) to play each one's cue. The sounds are made in
// code (MadeSounds), so there are no sound files. A system without
// AudioPadd runs silent.
#pragma once

#include "FlightMessages.hpp"

#include <ScratchPadd/Audio/MadeSounds.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/Base.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace PrecisionFlight {

// What plays for an event: a sound, and how
struct SoundCue {
  std::string sound;
  ScratchPadd::Audio::VoiceSettings settings;
};

// Each event's cue. A gate chimes, a gold one higher; hitting an energy
// wall zaps and clipping a gate's frame clanks, louder the more shield it
// costs; FOCUS chimes up and PHASE down (precision up, precision down);
// slipping through a wall whooshes; the game ends with a boom.
inline SoundCue cueFor(const FlightEvent &event) {
  struct Cue {
    SoundCue operator()(const GatePassed &e) const {
      return e.gold ? SoundCue{"chime", {.volume = 0.8f, .pitch = 1.5f}}
                    : SoundCue{"chime", {.volume = 0.5f}};
    }
    SoundCue operator()(const Crashed &e) const {
      const float volume = std::clamp(0.5f + e.damage / 50.0f, 0.5f, 1.0f);
      return e.wall ? SoundCue{"zap", {.volume = volume, .pitch = 0.6f}}
                    : SoundCue{"knock", {.volume = volume, .pitch = 0.7f}};
    }
    SoundCue operator()(const PowerupCollected &e) const {
      return e.kind == Kind::FocusPickup ? SoundCue{"chime", {.volume = 0.7f, .pitch = 2.0f}}
                                         : SoundCue{"chime", {.volume = 0.7f, .pitch = 0.75f}};
    }
    SoundCue operator()(const WallPhased &) const { return {"whoosh", {.volume = 0.8f}}; }
    SoundCue operator()(const GameOver &) const { return {"blast", {.volume = 1.0f}}; }
  };
  return std::visit(Cue{}, event);
}

class FlightSoundPadd : public ScratchPadd::Base {
public:
  FlightSoundPadd(ScratchPadd::System *system) : Base(system) {
    cueEvents<GatePassed, Crashed, PowerupCollected, WallPhased, GameOver>();
  }

  std::string name() override { return "FlightSound"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  // Finds AudioPadd, if the system has one, and gives it the sounds
  void prepare() override {
    audio_ = addressOf("AudioPadd");
    if (!audio_) {
      return;
    }
    namespace MadeSounds = ScratchPadd::Audio::MadeSounds;
    constexpr std::uint32_t kRate = 48000;
    const auto load = [&](const char *name, std::vector<float> samples) {
      sendTo(audio_, ScratchPadd::Audio::LoadSoundSamples{
                         .name = name,
                         .samples = std::make_shared<const std::vector<float>>(std::move(samples)),
                         .rate = kRate,
                         .channels = 1});
    };
    load("chime", MadeSounds::chime(kRate));
    load("zap", MadeSounds::zap(kRate));
    load("knock", MadeSounds::knock(kRate));
    load("whoosh", MadeSounds::whoosh(kRate));
    load("blast", MadeSounds::blast(kRate));
  }

private:
  template <typename... Events> void cueEvents() {
    (receive<Events>([this](const Events &event) { play(cueFor(event)); }), ...);
  }

  void play(const SoundCue &cue) {
    if (audio_) {
      sendTo(audio_, ScratchPadd::Audio::PlaySound{.name = cue.sound, .settings = cue.settings});
    }
  }

  ScratchPadd::PaddAddress audio_;
};

} // namespace PrecisionFlight
