// The sounds compiled in are the sounds made in code, rendered at kRate
#include <ScratchPadd/Audio/MadeSounds.hpp>
#include <ScratchPadd/Audio/MadeSoundsPcm16.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

using namespace ScratchPadd::Audio;

TEST(MadeSoundsPcm16Test, SameAsMadeInCode) {
  const struct {
    const char *name;
    std::vector<std::int16_t> rendered;
    std::vector<float> made;
  } sounds[] = {
      {"chime", MadeSoundsPcm16::chime(), MadeSounds::chime(MadeSoundsPcm16::kRate)},
      {"knock", MadeSoundsPcm16::knock(), MadeSounds::knock(MadeSoundsPcm16::kRate)},
      {"blast", MadeSoundsPcm16::blast(), MadeSounds::blast(MadeSoundsPcm16::kRate)},
      {"note", MadeSoundsPcm16::note(), MadeSounds::note(MadeSoundsPcm16::kRate)},
  };
  for (const auto &sound : sounds) {
    SCOPED_TRACE(sound.name);
    ASSERT_EQ(sound.rendered.size(), sound.made.size());
    ASSERT_GT(sound.rendered.size(), 1000u);
    for (std::size_t i = 0; i < sound.made.size(); ++i) {
      ASSERT_LE(std::abs(sound.rendered[i] - std::lround(sound.made[i] * 32767)), 1)
          << "sample " << i;
    }
  }
}
