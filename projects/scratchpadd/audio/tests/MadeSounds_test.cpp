// Sounds made in code: each the length it says, peaking at 0.9, starting
// without a click, and the same every time.
#include <ScratchPadd/Audio/MadeSounds.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

using namespace ScratchPadd::Audio;

namespace {

struct Made {
  std::string name;
  std::function<std::vector<float>(std::uint32_t)> make;
  float seconds;
};

const std::vector<Made> kSounds = {
    {"chime", MadeSounds::chime, 0.8f},
    {"knock", MadeSounds::knock, 0.3f},
    {"blast", MadeSounds::blast, 2.5f},
};

} // namespace

TEST(MadeSoundsTest, LengthPeakAndStart) {
  for (const Made &sound : kSounds) {
    const std::vector<float> samples = sound.make(48000);
    EXPECT_EQ(samples.size(), static_cast<std::size_t>(sound.seconds * 48000)) << sound.name;
    float peak = 0;
    for (float sample : samples)
      peak = std::max(peak, std::abs(sample));
    EXPECT_NEAR(peak, 0.9f, 1e-5) << sound.name;
    EXPECT_LT(std::abs(samples[0]), 0.01f) << sound.name << " starts with a click";
    EXPECT_EQ(samples, sound.make(48000)) << sound.name << " differs from one making to the next";
    // At another rate: the same length in seconds
    EXPECT_EQ(sound.make(32728).size(), static_cast<std::size_t>(sound.seconds * 32728))
        << sound.name;
  }
}

// The knock and the blast fade out by their end, so they don't stop with
// a click
TEST(MadeSoundsTest, DieAwayByTheEnd) {
  for (const Made &sound : kSounds) {
    const std::vector<float> samples = sound.make(48000);
    float tail = 0;
    for (std::size_t i = samples.size() - 480; i < samples.size(); ++i)
      tail = std::max(tail, std::abs(samples[i]));
    EXPECT_LT(tail, 0.05f) << sound.name;
  }
}
