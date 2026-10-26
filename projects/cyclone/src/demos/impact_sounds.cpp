#include "impact_sounds.h"

#include <ScratchPadd/Audio/MadeSounds.hpp>

#include <vector>

ImpactSounds::ImpactSounds(ScratchPadd::Audio::Output &output) : engine_(output) {
  const std::uint32_t rate = engine_.sampleRate();
  const auto load = [&](const char *name, const std::vector<float> &samples) {
    engine_.loadSamples(name, samples.data(), samples.size(), rate, 1);
  };
  load("knock", ScratchPadd::Audio::MadeSounds::knock(rate));
  load("blast", ScratchPadd::Audio::MadeSounds::blast(rate));
  engine_.start(error_);
}

void ImpactSounds::playHits() {
  hits_.takeKnocks(knocksToPlay_);
  if (!enabled_ || !playing()) {
    return;
  }
  for (const Knock &knock : knocksToPlay_) {
    if (engine_.play("knock", {.volume = knock.volume, .pan = knock.pan, .pitch = knock.pitch})) {
      ++knocks_;
    } else {
      ++dropped_;
    }
  }
}

void ImpactSounds::blast(const cyclone::Vector3 &position) {
  if (enabled_ && playing()) {
    engine_.play("blast", {.volume = 1.0f, .pan = hits_.panFor(position)});
  }
}
