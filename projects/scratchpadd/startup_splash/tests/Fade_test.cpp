#include <ScratchPadd/StartupSplash/Fade.hpp>

#include <gtest/gtest.h>

using ScratchPadd::brightnessAt;

// The fade's brightness: in, from black (-16) up to as drawn (0); out, back
// down; never past either end, and never back the wrong way
TEST(FadeTest, TheFadeGoesFromBlackToShownAndBack) {
  constexpr int kFrames = 30;
  EXPECT_EQ(brightnessAt(0, kFrames, true), -16);
  EXPECT_EQ(brightnessAt(kFrames, kFrames, true), 0);
  EXPECT_EQ(brightnessAt(0, kFrames, false), 0);
  EXPECT_EQ(brightnessAt(kFrames, kFrames, false), -16);
  EXPECT_EQ(brightnessAt(kFrames * 2, kFrames, true), 0);
  for (int frame = 1; frame <= kFrames; ++frame) {
    EXPECT_GE(brightnessAt(frame, kFrames, true), brightnessAt(frame - 1, kFrames, true));
    EXPECT_LE(brightnessAt(frame, kFrames, false), brightnessAt(frame - 1, kFrames, false));
  }
}
