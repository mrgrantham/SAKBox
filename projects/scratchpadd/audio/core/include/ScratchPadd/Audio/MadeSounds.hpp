#pragma once

#include <cstdint>
#include <vector>

// Sounds made in code, so programs can make noise without sound files:
// mono samples at `sampleRate`, to load with Engine::loadSamples (or
// AudioPadd's LoadSoundSamples). Each peaks at 0.9, a little under full
// scale.
namespace ScratchPadd::Audio::MadeSounds {

// A soft bell: two sine partials dying away over 0.8 s, at 880 Hz (A5)
std::vector<float> chime(std::uint32_t sampleRate);

// Two things knocking together: a click of noise and a low, quickly damped
// tone (0.3 s). Played higher for small, light things, lower for big ones.
std::vector<float> knock(std::uint32_t sampleRate);

// A blast: a burst of noise and a deep, falling rumble dying away over
// about 2 s
std::vector<float> blast(std::uint32_t sampleRate);

} // namespace ScratchPadd::Audio::MadeSounds
