// The physics dice: a throw settles in a few seconds with a face
// from 1 to 6 on each die, the same seed throws the same, and the pull takes
// the container off the screen with the dice still in it
#include <CosmicHex/Dice/DiceTray.hpp>

#include <gtest/gtest.h>

using CosmicHex::Dice::DiceTray;

namespace {
constexpr double kStep = 1.0 / 60;
std::pair<int, int> throwAndSettle(DiceTray &tray, std::uint64_t seed) {
  tray.throwDice(seed);
  for (int i = 0; i < 60 * 15 && !tray.settled(); ++i)
    tray.step(kStep);
  EXPECT_TRUE(tray.settled()) << "seed " << seed;
  return tray.faces();
}
} // namespace

TEST(DiceTrayTest, AThrowSettlesOnFaces) {
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    DiceTray tray;
    const auto [a, b] = throwAndSettle(tray, seed);
    EXPECT_GE(a, 1);
    EXPECT_LE(a, 6);
    EXPECT_GE(b, 1);
    EXPECT_LE(b, 6);
    EXPECT_LT(tray.secondsToSettle(), 8) << "seed " << seed;
    // Both dice inside the glass, resting on the floor
    for (int d = 0; d < 2; ++d) {
      const auto at = tray.position(d);
      EXPECT_LT(std::abs(static_cast<double>(at.x)), DiceTray::kWidth / 2);
      EXPECT_LT(std::abs(static_cast<double>(at.z)), DiceTray::kDepth / 2);
      EXPECT_NEAR(static_cast<double>(at.y), DiceTray::kDieSize / 2, 0.1);
    }
  }
}

TEST(DiceTrayTest, TheSameSeedThrowsTheSame) {
  DiceTray one, two;
  EXPECT_EQ(throwAndSettle(one, 42), throwAndSettle(two, 42));
  EXPECT_EQ(one.secondsToSettle(), two.secondsToSettle());
}

TEST(DiceTrayTest, ThePullTakesItOffScreen) {
  DiceTray tray;
  throwAndSettle(tray, 7);
  tray.startPull();
  for (int i = 0; i < 60 * 5 && !tray.offScreen(); ++i)
    tray.step(kStep);
  EXPECT_TRUE(tray.offScreen());
  // The dice stayed in the container (its walls hold them as it goes)
  for (int d = 0; d < 2; ++d)
    EXPECT_LE(std::abs(static_cast<double>(tray.position(d).x)), DiceTray::kWidth / 2);
}
