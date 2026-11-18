// The consoles' profiles: each one's settings within what its hardware and
// the measurements allow (round 92), and this build's profile one of them
#include <CosmicHex/Handheld/PlatformProfile.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex::Handheld;

namespace {
void expectSound(const PlatformProfile &p) {
  SCOPED_TRACE(p.name);
  // The overlay's text fits the top screen (6-pixel glyphs and a margin)
  EXPECT_LE(static_cast<int>(p.charsPerLine) * 6 + 8, p.topWidth + 8);
  EXPECT_GT(p.botIterations, 0);
  // A simulated game's memory: about 75 KB at most on a 64-bit Mac, less
  // with 32-bit pointers; the arena falls back to the heap past it
  EXPECT_GE(p.frameArenaBytes, 32u * 1024);
  // The dice's step: one of those dice_fairness proved fair (20,000
  // throws each, round 92), and a frame may catch up at least one
  EXPECT_TRUE(p.diceStep == 1.0 / 30 || p.diceStep == 1.0 / 60);
  EXPECT_GE(p.mostDiceSteps, 1);
  EXPECT_GT(p.faceSize, 0);
}
} // namespace

TEST(PlatformProfileTest, EachConsolesSettingsAreSound) {
  expectSound(kDsProfile);
  expectSound(kThreeDsProfile);
}

TEST(PlatformProfileTest, TheDsIsTunedForItsHardware) {
  // No floating-point hardware: the bots score in fixed point; its 3D
  // hardware shows the last frame by itself: still frames are held
  EXPECT_EQ(kDsProfile.botArithmetic, TableSettings::Arithmetic::FixedPoint);
  EXPECT_TRUE(kDsProfile.holdStillFrames);
  // The 3DS sends every frame
  EXPECT_FALSE(kThreeDsProfile.holdStillFrames);
  // (a desktop build, as these tests are, takes the 3DS's)
  EXPECT_EQ(&kThisPlatform, &kThreeDsProfile);
}
