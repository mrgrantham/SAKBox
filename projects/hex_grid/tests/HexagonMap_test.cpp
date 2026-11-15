// A hexagon-shaped board: one value for every hex, and nothing else.
#include <HexGrid/HexagonMap.hpp>

#include <gtest/gtest.h>

#include <set>

using HexGrid::Hex;
using HexGrid::HexagonMap;

TEST(HexagonMapTest, HoldsEveryHexWithinTheRadius) {
  for (int radius = 0; radius <= 6; ++radius) {
    HexagonMap<int> board(radius);
    EXPECT_EQ(board.size(), static_cast<std::size_t>(HexagonMap<int>::count(radius)));
    EXPECT_EQ(board.hexes().size(), board.size());
    for (Hex hex : HexGrid::spiral(Hex{}, radius + 1))
      EXPECT_EQ(board.contains(hex), HexGrid::length(hex) <= radius);
  }
}

// Every hex has its own place: writing each a different value and reading
// them all back gives each its own.
TEST(HexagonMapTest, EveryHexHasItsOwnValue) {
  HexagonMap<int> board(5, -1);
  int next = 0;
  for (Hex hex : board.hexes())
    board[hex] = next++;
  next = 0;
  for (Hex hex : board.hexes())
    EXPECT_EQ(board[hex], next++) << hex.q << "," << hex.r;
}

TEST(HexagonMapTest, StartsFilled) {
  const HexagonMap<char> board(3, '.');
  for (Hex hex : board.hexes())
    EXPECT_EQ(board[hex], '.');
}

TEST(HexagonMapTest, NeighboursOnTheBoard) {
  const HexagonMap<int> board(2);
  EXPECT_EQ(board.neighboursOn(Hex{}).size(), 6u);
  EXPECT_EQ(board.neighboursOn(Hex{2, 0}).size(), 3u); // a corner
  EXPECT_EQ(board.neighboursOn(Hex{1, 1}).size(), 4u); // an edge
}

// A copy is a separate board, and equal until one changes
TEST(HexagonMapTest, CopiesAreSeparate) {
  HexagonMap<int> board(2);
  HexagonMap<int> copy = board;
  EXPECT_EQ(copy, board);
  copy[{1, -1}] = 7;
  EXPECT_NE(copy, board);
  EXPECT_EQ((board[{1, -1}]), 0);
}
