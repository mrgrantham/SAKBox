// Moving across a board: how far a piece can get, and the shortest way
// round what's in the way.
#include <HexGrid/HexagonMap.hpp>
#include <HexGrid/Paths.hpp>

#include <gtest/gtest.h>

#include <unordered_set>

using HexGrid::Hex;

// On an open board, three steps reach the 37 hexes within three, each with
// its distance as its steps.
TEST(PathsTest, ReachableOnAnOpenBoard) {
  const HexGrid::HexagonMap<bool> board(5);
  const auto open = [&](Hex hex) { return board.contains(hex); };
  const std::vector<HexGrid::Reach<Hex>> reach = HexGrid::reachable(Hex{}, 3, open);
  EXPECT_EQ(reach.size(), 37u);
  EXPECT_EQ(reach.front(), (HexGrid::Reach<Hex>{Hex{}, 0}));
  for (const HexGrid::Reach<Hex> &r : reach)
    EXPECT_EQ(r.steps, HexGrid::distance(Hex{}, r.place));
  for (std::size_t i = 1; i < reach.size(); ++i)
    EXPECT_LE(reach[i - 1].steps, reach[i].steps);
}

// A wall in the way: the far side takes the long way round, or is out of
// reach.
TEST(PathsTest, WallsAreWalkedAround) {
  // A wall along q = 1, from r = -2 to r = 1, with gaps beyond its ends
  const std::unordered_set<Hex> wall{{1, -2}, {1, -1}, {1, 0}, {1, 1}};
  const auto open = [&](Hex hex) { return HexGrid::length(hex) <= 4 && !wall.contains(hex); };
  const std::vector<Hex> way = HexGrid::shortestPath(Hex{0, 0}, Hex{2, 0}, open);
  ASSERT_FALSE(way.empty());
  EXPECT_EQ(way.front(), (Hex{0, 0}));
  EXPECT_EQ(way.back(), (Hex{2, 0}));
  EXPECT_GT(way.size(), 3u); // longer than the straight line (2 steps)
  for (std::size_t i = 1; i < way.size(); ++i) {
    EXPECT_EQ(HexGrid::distance(way[i - 1], way[i]), 1);
    EXPECT_FALSE(wall.contains(way[i]));
  }
  // Three steps aren't enough to get round
  bool farSide = false;
  for (const HexGrid::Reach<Hex> &r : HexGrid::reachable(Hex{0, 0}, 2, open))
    farSide = farSide || r.place == Hex{2, 0};
  EXPECT_FALSE(farSide);
}

TEST(PathsTest, NoWayGivesNothing) {
  // The goal is closed in on every side
  const auto open = [](Hex hex) {
    return HexGrid::length(hex) <= 4 && HexGrid::distance(hex, Hex{3, 0}) != 1;
  };
  EXPECT_TRUE(HexGrid::shortestPath(Hex{}, Hex{3, 0}, open).empty());
  // Or too far for the steps allowed
  const auto all = [](Hex hex) { return HexGrid::length(hex) <= 4; };
  EXPECT_TRUE(HexGrid::shortestPath(Hex{}, Hex{4, 0}, all, 3).empty());
  EXPECT_EQ(HexGrid::shortestPath(Hex{}, Hex{4, 0}, all, 4).size(), 5u);
}

TEST(PathsTest, AStraightRunIsTheShortest) {
  const auto all = [](Hex hex) { return HexGrid::length(hex) <= 5; };
  const std::vector<Hex> way = HexGrid::shortestPath(Hex{-2, 0}, Hex{2, 0}, all);
  EXPECT_EQ(way.size(), 5u);
  EXPECT_EQ(HexGrid::shortestPath(Hex{1, 1}, Hex{1, 1}, all), (std::vector<Hex>{Hex{1, 1}}));
}

// The same board gives the same way every time
TEST(PathsTest, Repeatable) {
  const auto all = [](Hex hex) { return HexGrid::length(hex) <= 5; };
  EXPECT_EQ(HexGrid::shortestPath(Hex{-3, 1}, Hex{3, -2}, all),
            HexGrid::shortestPath(Hex{-3, 1}, Hex{3, -2}, all));
}

// Moving along edges, corner to corner: halfway round one hex is 3 edges,
// and one step reaches the three corners next to the start.
TEST(PathsTest, AlongEdgesCornerToCorner) {
  const std::vector<Hex> board = HexGrid::spiral(Hex{}, 2);
  const auto onBoard = [&](HexGrid::Corner c) {
    for (Hex hex : HexGrid::hexesTouching(c))
      if (HexGrid::length(hex) <= 2)
        return true;
    return false;
  };
  const HexGrid::Corner from = HexGrid::corner(Hex{}, 0), to = HexGrid::corner(Hex{}, 3);
  const std::vector<HexGrid::Corner> way = HexGrid::shortestPath(from, to, onBoard);
  ASSERT_EQ(way.size(), 4u);
  for (std::size_t i = 1; i < way.size(); ++i) {
    EXPECT_TRUE(HexGrid::edgeBetween(way[i - 1], way[i]).has_value());
  }
  const auto reach = HexGrid::reachable(from, 1, onBoard);
  EXPECT_EQ(reach.size(), 4u);
  EXPECT_EQ(reach.front(), (HexGrid::Reach<HexGrid::Corner>{from, 0}));
}
