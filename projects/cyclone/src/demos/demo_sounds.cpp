#include "demo_sounds.h"

#include <ScratchPadd/Audio/MadeSounds.hpp>

#include <vector>

namespace MadeSounds = ScratchPadd::Audio::MadeSounds;
using ScratchPadd::Audio::VoiceSettings;

DemoSounds::DemoSounds(ScratchPadd::Audio::Output &output) : engine_(output) {
  const std::uint32_t rate = engine_.sampleRate();
  const auto load = [&](const char *name, const std::vector<float> &samples) {
    engine_.loadSamples(name, samples.data(), samples.size(), rate, 1);
  };
  load("knock", MadeSounds::knock(rate));
  load("blast", MadeSounds::blast(rate));
  load("shot", MadeSounds::shot(rate));
  load("zap", MadeSounds::zap(rate));
  load("whoosh", MadeSounds::whoosh(rate));
  load("crackle", MadeSounds::crackle(rate));
  load("chime", MadeSounds::chime(rate));
  load("engine", MadeSounds::engine(rate));
  load("wind", MadeSounds::wind(rate));
  engine_.start(error_);
}

bool DemoSounds::play(const std::string &name, const VoiceSettings &settings) {
  if (!enabled_ || !playing() || !engine_.play(name, settings)) {
    return false;
  }
  ++played_;
  return true;
}

void DemoSounds::setLoop(const std::string &name, const VoiceSettings &settings) {
  Loop &loop = loops_[name];
  loop.settings = settings;
  loop.settings.loop = true;
  if (!playing()) {
    return;
  }
  VoiceSettings now = loop.settings;
  if (!enabled_) {
    now.volume = 0;
  }
  if (loop.voice && engine_.set(loop.voice, now)) {
    return;
  }
  // Not playing yet (or taken over for a louder sound): start it
  const auto voice = engine_.play(name, now);
  loop.voice = voice.value_or(0);
  if (voice) {
    ++played_;
  }
}

void DemoSounds::stopLoop(const std::string &name) {
  const auto loop = loops_.find(name);
  if (loop == loops_.end()) {
    return;
  }
  if (loop->second.voice) {
    engine_.stop(loop->second.voice);
  }
  loops_.erase(loop);
}

void DemoSounds::setEnabled(bool enabled) {
  enabled_ = enabled;
  // The loops go quiet, or come back as last set
  for (auto &[name, loop] : loops_) {
    setLoop(name, loop.settings);
  }
}
