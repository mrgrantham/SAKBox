// The winner's ships: at least three (the rest fading in at colonies),
// taking off from where they are into a ring above the board, going round
// nose first with their flames burning, until it's over
#include <CosmicHex/Handheld/WinCelebration.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace CosmicHex::Handheld;
using ScratchPadd::Scene::Vector3;

TEST(WinCelebrationTest, TheShipsTakeOffAndCircle) {
  const WinCelebration party({{1, 0, 1}}, {{-2, 0, 0}}, 10);
  ASSERT_EQ(party.ships(), 3u);
  EXPECT_FLOAT_EQ(party.fadedIn(0, 10), 1);
  EXPECT_FLOAT_EQ(party.fadedIn(1, 10), 0);
  EXPECT_FLOAT_EQ(party.fadedIn(2, 10 + WinCelebration::kSecondsFadeIn), 1);
  EXPECT_NEAR(party.pose(0, 10).at.x, 1, 1e-4);
  EXPECT_NEAR(party.pose(1, 10).at.x, -2, 1e-4); // (at its colony)
  for (std::size_t i = 0; i < 3; ++i) {
    const ShipPose up = party.pose(i, 10 + WinCelebration::kSecondsUp + 1);
    EXPECT_NEAR(up.at.y, WinCelebration::kHeight, 1e-4);
    EXPECT_NEAR(std::hypot(up.at.x, up.at.z), WinCelebration::kRadius, 1e-3);
    EXPECT_GT(up.flame, 0);
  }
  EXPECT_FALSE(party.over(15));
  EXPECT_TRUE(party.over(10 + WinCelebration::kSeconds));
}

TEST(WinCelebrationTest, AWinnerWithManyShipsFliesThemAll) {
  const WinCelebration party({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}}, {}, 0);
  EXPECT_EQ(party.ships(), 4u);
  EXPECT_FLOAT_EQ(party.fadedIn(3, 0), 1);
}
