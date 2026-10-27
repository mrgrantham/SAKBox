// Sounds made in code: each the length it says, peaking at 0.9, starting
// without a click, and the same every time; loops running on from their end
// into their start without a click.
#include <ScratchPadd/Audio/MadeSounds.hpp>

#include <gtest/gtest.h>

#include <algorithm>
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
    {"chime", MadeSounds::chime, 0.8f},     {"knock", MadeSounds::knock, 0.3f},
    {"blast", MadeSounds::blast, 2.5f},     {"shot", MadeSounds::shot, 0.4f},
    {"zap", MadeSounds::zap, 0.3f},         {"whoosh", MadeSounds::whoosh, 0.8f},
    {"crackle", MadeSounds::crackle, 1.2f},
};

const std::vector<Made> kLoops = {
    {"engine", MadeSounds::engine, 1.0f},
    {"wind", MadeSounds::wind, 2.0f},
};

// The biggest step from one sample to the next
float biggestStep(const std::vector<float> &samples) {
  float biggest = 0;
  for (std::size_t i = 1; i < samples.size(); ++i)
    biggest = std::max(biggest, std::abs(samples[i] - samples[i - 1]));
  return biggest;
}

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

// A loop is its length, peaks at 0.9 and is the same every time, and its
// end runs into its start with no bigger a step than anywhere within it:
// played looping, there's no click
TEST(MadeSoundsTest, LoopsJoinWithoutAClick) {
  for (const Made &loop : kLoops) {
    const std::vector<float> samples = loop.make(48000);
    EXPECT_EQ(samples.size(), static_cast<std::size_t>(loop.seconds * 48000)) << loop.name;
    float peak = 0;
    for (float sample : samples)
      peak = std::max(peak, std::abs(sample));
    EXPECT_NEAR(peak, 0.9f, 1e-5) << loop.name;
    EXPECT_EQ(samples, loop.make(48000)) << loop.name;
    EXPECT_LE(std::abs(samples.front() - samples.back()), biggestStep(samples)) << loop.name;
  }
}
