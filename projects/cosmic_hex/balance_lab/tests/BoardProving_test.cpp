// Proving the solver's boards (round 87): chi-squares from wins, each board
// screened, the fair ones played deep, the rest left
#include "../src/BoardProving.hpp"

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Lab;

namespace {
// `n` finished games: the winner given by `winnerOf(i)`, first player i % 4
std::vector<Batch::GameResult> games(int n, int (*winnerOf)(int)) {
  std::vector<Batch::GameResult> out(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    out[static_cast<std::size_t>(i)].first = i % 4;
    out[static_cast<std::size_t>(i)].winner = winnerOf(i);
  }
  return out;
}
} // namespace

TEST(BoardProvingTest, ChiSquaresFromWins) {
  // Winners spread evenly by home and by turn position: both 0
  const auto even = games(400, [](int i) { return (i / 4) % 4; });
  const Fairness fair = fairnessOf(even, 4);
  EXPECT_EQ(fair.games, 400);
  EXPECT_NEAR(fair.homeChi, 0, 1e-9);
  EXPECT_NEAR(fair.turnChi, 0, 1e-9);
  EXPECT_TRUE(fair.fair(4));
  // The first player always wins: even by home, all turn position 0
  const auto first = games(400, [](int i) { return i % 4; });
  const Fairness leaning = fairnessOf(first, 4);
  EXPECT_NEAR(leaning.homeChi, 0, 1e-9);
  EXPECT_NEAR(leaning.turnChi, 1200, 1e-6); // (3 x 100 for each empty position, 300 for the full)
  EXPECT_FALSE(leaning.fair(4));
  // Unfinished games don't count
  auto some = games(10, [](int) { return -1; });
  EXPECT_EQ(fairnessOf(some, 4).games, 0);
  EXPECT_FALSE(fairnessOf(some, 4).fair(4));
}

TEST(BoardProvingTest, FairBoardsGoDeepTheOthersStop) {
  BoardProving proving;
  proving.start(3, 4);
  EXPECT_TRUE(proving.busy());
  // Board 0 screens fair, 1 not, 2 fair
  const Fairness fair{300, 2, 2}, unfair{300, 15, 2};
  std::vector<std::size_t> order;
  while (auto run = proving.next()) {
    EXPECT_FALSE(proving.next().has_value()); // (one at a time)
    order.push_back(run->board);
    EXPECT_EQ(run->games, run->deep ? BoardProving::kDeepGames : BoardProving::kScreenGames);
    proving.finished(*run, run->board == 1 ? unfair : fair);
  }
  EXPECT_EQ(order, (std::vector<std::size_t>{0, 1, 2, 0, 2})); // (screens first, then deep)
  EXPECT_FALSE(proving.busy());
  const auto &results = proving.results();
  EXPECT_TRUE(results[0].deep.has_value());
  EXPECT_FALSE(results[1].deep.has_value());
  EXPECT_TRUE(results[2].deep.has_value());
  EXPECT_EQ(fairLine(3), 5.99);
}
