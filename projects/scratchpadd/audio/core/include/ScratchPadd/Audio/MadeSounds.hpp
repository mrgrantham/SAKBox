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

// A note: C5 (523 Hz), a bright attack ringing down over 1.5 s, like a
// vibraphone. The DS's AudioPadd plays it at each MIDI note's pitch, as its
// instrument.
std::vector<float> note(std::uint32_t sampleRate);

// The C5 that note() plays, as a MIDI key
inline constexpr int kNoteKey = 72;

// A gunshot: a crack of noise and a low thump (0.4 s). Played lower for a
// bigger gun.
std::vector<float> shot(std::uint32_t sampleRate);

// A ray gun's zap: a buzzing tone falling from 1600 Hz to 200 Hz (0.3 s)
std::vector<float> zap(std::uint32_t sampleRate);

// Something rushing past or up: noise swelling and fading, its pitch
// rising (0.8 s). A firework's launch, a fireball.
std::vector<float> whoosh(std::uint32_t sampleRate);

// A firework's crackle: pops scattered over 1.2 s, thinning out
std::vector<float> crackle(std::uint32_t sampleRate);

// Loops, to play with VoiceSettings::loop: the end runs on into the start
// with no click, so they play seamlessly for as long as they're wanted.
// Played faster, they sound faster (an engine revving up).

// An engine's drone: a 50 Hz hum and its harmonics, with a rumble (1 s)
std::vector<float> engine(std::uint32_t sampleRate);

// Wind: noise swelling and easing, gusting once a loop (2 s)
std::vector<float> wind(std::uint32_t sampleRate);

} // namespace ScratchPadd::Audio::MadeSounds
