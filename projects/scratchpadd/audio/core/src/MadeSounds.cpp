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

// A loop of `seconds` from `sample(t)`: made a little longer, with what's
// past the end faded into the start, so the end runs on into the start as
// smoothly as the sound runs on
template <typename Sample>
std::vector<float> makeLoop(std::uint32_t sampleRate, float seconds, Sample sample) {
  constexpr float kBlendSeconds = 0.05f;
  const auto length = static_cast<std::size_t>(seconds * static_cast<float>(sampleRate));
  const auto blend = static_cast<std::size_t>(kBlendSeconds * static_cast<float>(sampleRate));
  const std::vector<float> made = make(sampleRate, seconds + kBlendSeconds, sample);
  std::vector<float> samples(made.begin(), made.begin() + static_cast<std::ptrdiff_t>(length));
  float peak = 0;
  for (std::size_t i = 0; i < length; ++i) {
    if (i < blend) {
      const float in = static_cast<float>(i) / static_cast<float>(blend);
      samples[i] = in * made[i] + (1 - in) * made[length + i];
    }
    peak = std::max(peak, std::abs(samples[i]));
  }
  for (float &value : samples) {
    value *= 0.9f / peak;
  }
  return samples;
}

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

std::vector<float> note(std::uint32_t sampleRate) {
  const float frequency = 440.0f * std::pow(2.0f, static_cast<float>(kNoteKey - 69) / 12.0f);
  return make(sampleRate, 1.5f, [=](float t) {
    const float phase = kTwoPi * frequency * t;
    return fadeIn(t, 0.002f) * std::exp(-2.5f * t) *
           (std::sin(phase) + 0.25f * std::sin(4 * phase));
  });
}

std::vector<float> shot(std::uint32_t sampleRate) {
  Noise noise;
  float body = 0;
  return make(sampleRate, 0.4f, [&](float t) {
    // The crack: bright noise, gone in about 15 ms
    const float crack = noise.next() * std::exp(-t / 0.006f);
    // The thump: the same noise low-passed, and a 70 Hz tone, dying slower
    body += 0.08f * (noise.next() - body);
    const float thump = std::exp(-t / 0.07f) * (2.0f * body + 0.5f * std::sin(kTwoPi * 70.0f * t));
    return fadeIn(t, 0.0005f) * (crack + thump);
  });
}

std::vector<float> zap(std::uint32_t sampleRate) {
  float phase = 0;
  const float rate = static_cast<float>(sampleRate);
  return make(sampleRate, 0.3f, [&](float t) {
    phase += kTwoPi * (200.0f + 1400.0f * std::exp(-t / 0.06f)) / rate;
    // A sine and its odd harmonics: a buzz, softer than a square wave
    const float buzz = std::sin(phase) + 0.3f * std::sin(3 * phase) + 0.15f * std::sin(5 * phase);
    return fadeIn(t, 0.002f) * std::exp(-t / 0.08f) * buzz;
  });
}

std::vector<float> whoosh(std::uint32_t sampleRate) {
  Noise noise;
  float low = 0, band = 0;
  return make(sampleRate, 0.8f, [&](float t) {
    // Noise through a filter whose band rises with the swell (two one-pole
    // filters: what passes the first but not the second)
    const float cutoff = 0.02f + 0.15f * t / 0.8f;
    low += cutoff * (noise.next() - low);
    band += 0.5f * cutoff * (low - band);
    const float swell = std::sin(3.1415927f * t / 0.8f);
    return swell * swell * (low - band);
  });
}

std::vector<float> crackle(std::uint32_t sampleRate) {
  Noise noise;
  float pop = 0;
  const float rate = static_cast<float>(sampleRate);
  return make(sampleRate, 1.2f, [&](float t) {
    // A pop starts at random, more often early on, and dies in 3 ms
    const float chance = 600.0f * std::exp(-t / 0.4f) / rate;
    if ((noise.next() + 1) * 0.5f < chance) {
      pop = 0.5f + 0.5f * std::abs(noise.next());
    }
    pop *= std::exp(-1.0f / (0.003f * rate));
    // Fades out over the last 0.2 s
    const float end = std::min(1.0f, (1.2f - t) / 0.2f);
    return fadeIn(t, 0.002f) * end * pop * noise.next();
  });
}

std::vector<float> engine(std::uint32_t sampleRate) {
  Noise noise;
  float rumble = 0;
  return makeLoop(sampleRate, 1.0f, [&](float t) {
    const float phase = kTwoPi * 50.0f * t;
    const float hum = std::sin(phase) + 0.6f * std::sin(2 * phase) + 0.4f * std::sin(3 * phase) +
                      0.2f * std::sin(5 * phase);
    rumble += 0.05f * (noise.next() - rumble);
    // Each turn of the engine pulses (a 25 Hz throb)
    const float throb = 0.75f + 0.25f * std::sin(kTwoPi * 25.0f * t);
    return throb * (hum + 3.0f * rumble);
  });
}

std::vector<float> wind(std::uint32_t sampleRate) {
  Noise noise;
  float low = 0, band = 0;
  return makeLoop(sampleRate, 2.0f, [&](float t) {
    // A gust once a loop: louder and higher
    const float gust = 0.5f - 0.5f * std::cos(kTwoPi * t / 2.0f);
    const float cutoff = 0.01f + 0.03f * gust;
    low += cutoff * (noise.next() - low);
    band += 0.3f * cutoff * (low - band);
    return (0.4f + 0.6f * gust) * (low - band);
  });
}

} // namespace ScratchPadd::Audio::MadeSounds
