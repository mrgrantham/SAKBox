// Hex coordinates: neighbours, distance, rotation and reflection, rings,
// spirals and lines.
#include <HexGrid/Hex.hpp>

#include <gtest/gtest.h>

#include <set>
#include <unordered_set>

using HexGrid::Hex;

TEST(HexTest, TheThirdCoordinateMakesZero) {
  constexpr Hex kHex{3, -5};
  static_assert(kHex.q + kHex.r + kHex.s() == 0);
  EXPECT_EQ(kHex.s(), 2);
}

TEST(HexTest, NeighboursAreOneStepAwayInDirectionOrder) {
  const Hex centre{2, -1};
  const auto around = HexGrid::neighbours(centre);
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(HexGrid::distance(centre, around[i]), 1);
    EXPECT_EQ(around[i], centre + HexGrid::kDirections[i]);
    // Each is next to the one before it: they go round in order
    EXPECT_EQ(HexGrid::distance(around[i], around[(i + 1) % 6]), 1);
  }
  EXPECT_EQ(HexGrid::direction(6), HexGrid::direction(0));
  EXPECT_EQ(HexGrid::direction(-1), HexGrid::direction(5));
}

TEST(HexTest, Distance) {
  static_assert(HexGrid::distance(Hex{}, Hex{3, -1}) == 3);
  EXPECT_EQ(HexGrid::distance(Hex{-2, 0}, Hex{2, 0}), 4);
  EXPECT_EQ(HexGrid::distance(Hex{0, -3}, Hex{0, 3}), 6);
  EXPECT_EQ(HexGrid::distance(Hex{1, 1}, Hex{-1, 2}), 2);
  EXPECT_EQ(HexGrid::distance(Hex{5, -2}, Hex{5, -2}), 0);
}

// Six turns of 60 degrees come back round; each turn keeps the distance to
// the centre; turning a direction gives the next direction.
TEST(HexTest, Rotation) {
  const Hex hex{3, -1}, centre{1, 1};
  for (int turns = 0; turns < 6; ++turns) {
    EXPECT_EQ(HexGrid::distance(HexGrid::rotated(hex, turns, centre), centre),
              HexGrid::distance(hex, centre));
    EXPECT_EQ(HexGrid::rotated(HexGrid::direction(turns), 1), HexGrid::direction(turns + 1));
  }
  EXPECT_EQ(HexGrid::rotated(hex, 6, centre), hex);
  EXPECT_EQ(HexGrid::rotated(hex, -1, centre), HexGrid::rotated(hex, 5, centre));
}

TEST(HexTest, Reflection) {
  const Hex hex{3, -1};
  EXPECT_EQ(HexGrid::reflected(HexGrid::reflected(hex)), hex);
  EXPECT_EQ(HexGrid::reflected(hex).r, hex.r);
  EXPECT_EQ(HexGrid::reflected(Hex{2, 0}), (Hex{-2, 0})); // across the q axis line
  EXPECT_EQ(HexGrid::length(HexGrid::reflected(hex)), HexGrid::length(hex));
}

TEST(HexTest, ARingIsEveryHexAtThatDistanceGoingRound) {
  const Hex centre{1, -2};
  for (int radius = 1; radius <= 4; ++radius) {
    const std::vector<Hex> ring = HexGrid::ring(centre, radius);
    ASSERT_EQ(ring.size(), static_cast<std::size_t>(6 * radius));
    std::unordered_set<Hex> unique(ring.begin(), ring.end());
    EXPECT_EQ(unique.size(), ring.size());
    for (std::size_t i = 0; i < ring.size(); ++i) {
      EXPECT_EQ(HexGrid::distance(ring[i], centre), radius);
      EXPECT_EQ(HexGrid::distance(ring[i], ring[(i + 1) % ring.size()]), 1); // round in order
    }
  }
  EXPECT_EQ(HexGrid::ring(centre, 0), std::vector<Hex>{centre});
}

TEST(HexTest, ASpiralIsEveryHexWithinTheRadiusRingByRing) {
  const std::vector<Hex> spiral = HexGrid::spiral(Hex{}, 5);
  ASSERT_EQ(spiral.size(), 91u); // 1 + 3 x 5 x 6
  EXPECT_EQ(spiral.front(), Hex{});
  std::unordered_set<Hex> unique(spiral.begin(), spiral.end());
  EXPECT_EQ(unique.size(), spiral.size());
  for (std::size_t i = 1; i < spiral.size(); ++i)
    EXPECT_LE(HexGrid::length(spiral[i - 1]), HexGrid::length(spiral[i]));
}

// A line goes step by step from one end to the other, and is exactly as
// long as the distance; reversing it gives the same hexes, back to front,
// except where it runs along an edge.
TEST(HexTest, LinesGoStepByStep) {
  const Hex a{-3, 1}, b{4, -2};
  const std::vector<Hex> line = HexGrid::line(a, b);
  ASSERT_EQ(line.size(), static_cast<std::size_t>(HexGrid::distance(a, b) + 1));
  EXPECT_EQ(line.front(), a);
  EXPECT_EQ(line.back(), b);
  for (std::size_t i = 1; i < line.size(); ++i)
    EXPECT_EQ(HexGrid::distance(line[i - 1], line[i]), 1);
  EXPECT_EQ(HexGrid::line(a, a), std::vector<Hex>{a});
}

// Along a straight row it's exactly the row.
TEST(HexTest, AStraightLineIsTheRow) {
  const std::vector<Hex> line = HexGrid::line(Hex{0, 0}, Hex{0, 3});
  EXPECT_EQ(line, (std::vector<Hex>{{0, 0}, {0, 1}, {0, 2}, {0, 3}}));
}

// A line that runs along edges (here every second hex is a tie) always
// takes the same side, so it's the same every time.
TEST(HexTest, LinesAlongEdgesAreRepeatable) {
  const std::vector<Hex> line = HexGrid::line(Hex{0, 0}, Hex{2, -4});
  ASSERT_EQ(line.size(), 5u);
  EXPECT_EQ(line, HexGrid::line(Hex{0, 0}, Hex{2, -4}));
  for (std::size_t i = 1; i < line.size(); ++i)
    EXPECT_EQ(HexGrid::distance(line[i - 1], line[i]), 1);
}

TEST(HexTest, HexesCanBeSortedAndHashed) {
  std::set<Hex> sorted{{1, 0}, {0, 1}, {0, 0}};
  EXPECT_EQ(*sorted.begin(), (Hex{0, 0}));
  std::unordered_set<Hex> hashed{{1, 0}, {0, 1}, {1, 0}};
  EXPECT_EQ(hashed.size(), 2u);
}
