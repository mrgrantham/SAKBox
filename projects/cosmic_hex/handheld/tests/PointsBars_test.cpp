// The points bars (round 96): a row of dashes a player, a dash a point in
// their colour and grey for the rest of the target, centred; the turn's row
// outlined in a colour no player has, inside the gaps between rows
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>

#include <gtest/gtest.h>

#include <algorithm>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

namespace {
State fourPlayers(int turn) {
  State state;
  state.players = 4;
  state.turn = turn;
  return state;
}
} // namespace

TEST(PointsBarsTest, ADashAPointCentred) {
  const State state = fourPlayers(1);
  const auto bars = PointsBars::of(state, 256);
  const int target = state.settings.winPoints;
  ASSERT_EQ(bars.size(), 4u + 4u * static_cast<std::size_t>(target));
  // The dashes: centred, every row the same, grey where no points are yet
  float left = 1e9f, right = 0;
  for (std::size_t i = 4; i < bars.size(); ++i) {
    left = std::min(left, bars[i].x);
    right = std::max(right, bars[i].x + bars[i].width);
    EXPECT_EQ(bars[i].height, PointsBars::kRowHeight);
  }
  EXPECT_FLOAT_EQ(left, 256 - right);
  for (int p = 0; p < 4; ++p) {
    const int points = CosmicHex::points(state, p);
    for (int k = 0; k < target; ++k) {
      const auto &dash = bars[4 + static_cast<std::size_t>(p * target + k)];
      EXPECT_EQ(dash.colour, k < points ? BoardColours::ofPlayer(p) : PointsBars::kStillToWin)
          << "player " << p << ", dash " << k;
    }
  }
}

TEST(PointsBarsTest, TheTurnsRowIsOutlinedApart) {
  const State state = fourPlayers(2);
  const auto bars = PointsBars::of(state, 400);
  const int target = state.settings.winPoints;
  // The outline: its four sides round the turn's row, clear of the rows
  // above and below
  const auto row = [&](int p) { return bars[4 + static_cast<std::size_t>(p * target)]; };
  float top = 1e9f, bottom = 0;
  for (int i = 0; i < 4; ++i) {
    top = std::min(top, bars[static_cast<std::size_t>(i)].y);
    bottom = std::max(bottom, bars[static_cast<std::size_t>(i)].y +
                                  bars[static_cast<std::size_t>(i)].height);
    EXPECT_EQ(bars[static_cast<std::size_t>(i)].colour, PointsBars::kOutlineColour);
  }
  EXPECT_LT(top, row(2).y);
  EXPECT_GT(bottom, row(2).y + row(2).height);
  EXPECT_GE(top, row(1).y + row(1).height);
  EXPECT_LE(bottom, row(3).y);
  // A colour none of the players have
  for (int p = 0; p < kMaxPlayers; ++p) {
    EXPECT_NE(PointsBars::kOutlineColour, BoardColours::ofPlayer(p));
  }
  EXPECT_GE(PointsBars::bottom(4), row(3).y + row(3).height);
}
