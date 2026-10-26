#include <ScratchPadd/Audio/MadeSounds.hpp>

#include <algorithm>
#include <cmath>

namespace ScratchPadd::Audio::MadeSounds {

namespace {

constexpr float kTwoPi = 6.2831853f;

// Noise from -1 to 1, the same every time (so a made sound is too)
class Noise {
public:
  float next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return static_cast<float>(state_) / 2147483648.0f - 1.0f;
  }

private:
  std::uint32_t state_ = 0x9E3779B9u;
};

// `seconds` of samples from `sample(t)`, t in seconds, scaled to peak at 0.9
template <typename Sample>
std::vector<float> make(std::uint32_t sampleRate, float seconds, Sample sample) {
  std::vector<float> samples(static_cast<std::size_t>(seconds * static_cast<float>(sampleRate)));
  float peak = 0;
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] = sample(static_cast<float>(i) / static_cast<float>(sampleRate));
    peak = std::max(peak, std::abs(samples[i]));
  }
  if (peak > 0) {
    for (float &value : samples) {
      value *= 0.9f / peak;
    }
  }
  return samples;
}

// Rises from 0 to 1 over `seconds`, so a sound doesn't start with a click
float fadeIn(float t, float seconds) { return std::min(1.0f, t / seconds); }

} // namespace

std::vector<float> chime(std::uint32_t sampleRate) {
  return make(sampleRate, 0.8f, [](float t) {
    return fadeIn(t, 0.005f) * std::exp(-5.0f * t) *
           (0.6f * std::sin(kTwoPi * 880.0f * t) + 0.25f * std::sin(kTwoPi * 2.76f * 880.0f * t));
  });
}

std::vector<float> knock(std::uint32_t sampleRate) {
  Noise noise;
  float previous = 0;
  return make(sampleRate, 0.3f, [&](float t) {
    // The click: noise with its lows taken out, gone in about 10 ms
    const float hiss = noise.next();
    const float click = (hiss - previous) * 0.5f * std::exp(-t / 0.004f);
    previous = hiss;
    // The body: a low tone and an inharmonic partial, damped fast
    const float body = std::exp(-t / 0.05f) * (0.7f * std::sin(kTwoPi * 160.0f * t) +
                                               0.3f * std::sin(kTwoPi * 430.0f * t));
    return fadeIn(t, 0.001f) * (0.5f * click + body);
  });
}

std::vector<float> blast(std::uint32_t sampleRate) {
  Noise noise;
  float rumble = 0, phase = 0;
  const float rate = static_cast<float>(sampleRate);
  return make(sampleRate, 2.5f, [&](float t) {
    // Noise, low-passed more and more as it dies away (a one-pole filter)
    const float cutoff = 0.02f + 0.3f * std::exp(-t / 0.08f);
    rumble += cutoff * (noise.next() - rumble);
    // A deep thump falling from 90 Hz to 35 Hz
    phase += kTwoPi * (35.0f + 55.0f * std::exp(-t / 0.15f)) / rate;
    const float thump = std::sin(phase) * std::exp(-t / 0.35f);
    return fadeIn(t, 0.002f) * (2.5f * rumble * std::exp(-t / 0.6f) + 0.6f * thump);
  });
}

} // namespace ScratchPadd::Audio::MadeSounds
