// Points and badges pop as they're won: not at the first look, each new
// one growing past its size and settling, several one after another
#include <CosmicHex/Handheld/ScorePops.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex::Handheld;

TEST(ScorePopsTest, NewPointsPopInTurn) {
  ScorePops pops;
  pops.saw({3, 1}, {0, 0}, 0);
  EXPECT_FLOAT_EQ(pops.dash(0, 2, 0.1), 1); // (there already)
  EXPECT_FALSE(pops.popping(0.1));
  pops.saw({5, 1}, {1, 0}, 10);
  EXPECT_FLOAT_EQ(pops.dash(0, 3, 10), 0);                       // (just won: not in yet)
  EXPECT_GT(pops.dash(0, 3, 10 + ScorePops::kSeconds * 0.7), 1); // (past its size)
  EXPECT_FLOAT_EQ(pops.dash(0, 4, 10.05), 0);                    // (the second after the first)
  EXPECT_FLOAT_EQ(pops.dash(0, 3, 12), 1);
  EXPECT_FLOAT_EQ(pops.dash(1, 0, 10.2), 1); // (another player's: unchanged)
  EXPECT_LT(pops.badge(0, 0, 10.1), 1);
  EXPECT_TRUE(pops.popping(10.2));
  EXPECT_FALSE(pops.popping(12));
  // (fewer, as at a new game: nothing pops)
  pops.saw({0, 0}, {0, 0}, 20);
  EXPECT_FALSE(pops.popping(20.1));
}
