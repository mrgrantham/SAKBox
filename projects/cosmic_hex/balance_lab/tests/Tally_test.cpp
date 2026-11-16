// The balance lab's tally: wins counted by turn position (from who went
// first) and by home, shares and the fair share's margin, rounds, and
// stations by system.
#include "../src/Tally.hpp"

#include <gtest/gtest.h>

using CosmicHex::Batch::GameResult;
using CosmicHex::Lab::Tally;

namespace {
GameResult won(int winner, int first, int rounds) {
  GameResult result;
  result.winner = winner;
  result.first = first;
  result.rounds = rounds;
  result.bySystem = {1, 2, 3};
  result.badgesAll = 10;
  return result;
}
} // namespace

TEST(TallyTest, CountsWinsByTurnAndHome) {
  Tally tally;
  tally.add(won(2, 2, 10)); // the first to play won: home 3
  tally.add(won(0, 3, 12)); // player 1 played 2nd (after player 4)
  tally.add(won(3, 0, 14)); // player 4 played 4th
  GameResult unfinished;
  unfinished.winner = -1;
  tally.add(unfinished);
  EXPECT_EQ(tally.games, 4);
  EXPECT_EQ(tally.finished, 3);
  EXPECT_EQ(tally.winsByTurn, (std::array<int, 4>{1, 1, 0, 1}));
  EXPECT_EQ(tally.winsByHome, (std::array<int, 4>{1, 0, 1, 1}));
  EXPECT_DOUBLE_EQ(tally.share(1), 100.0 / 3);
  EXPECT_DOUBLE_EQ(tally.averageRounds(), 12.0);
  EXPECT_EQ(tally.gamesByRounds[12], 1);
  EXPECT_DOUBLE_EQ(tally.stationsBySystem[2],
                   9.0); // (3 each from the finished games; the unfinished one had none)
  EXPECT_DOUBLE_EQ(tally.badges, 30.0);
}

TEST(TallyTest, AFairShareAndItsMargin) {
  Tally tally;
  EXPECT_DOUBLE_EQ(tally.fairShare(), 25.0);
  EXPECT_DOUBLE_EQ(tally.margin(), 0.0); // (nothing yet)
  for (int i = 0; i < 400; ++i)
    tally.add(won(i % 4, 0, 10));
  EXPECT_NEAR(tally.margin(), 4.2, 0.05); // 1.96 x sqrt(0.25 x 0.75 / 400)
}
