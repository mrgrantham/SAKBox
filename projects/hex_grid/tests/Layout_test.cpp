// Hexes on screen: centres, corners, and which hex a point is in.
#include <HexGrid/Layout.hpp>

#include <gtest/gtest.h>

#include <cmath>

using HexGrid::Hex;
using HexGrid::Layout;
using HexGrid::Orientation;
using HexGrid::Point;

namespace {
float gap(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }
} // namespace

// Neighbours' centres are all the same distance apart (the hex's width
// across the flats: sqrt(3) x size), in both orientations.
TEST(LayoutTest, NeighboursAreEvenlySpaced) {
  for (Orientation orientation : {Orientation::PointyTop, Orientation::FlatTop}) {
    const Layout layout{orientation, {10, 10}, {100, 50}};
    EXPECT_FLOAT_EQ(layout.centre(Hex{}).x, 100);
    EXPECT_FLOAT_EQ(layout.centre(Hex{}).y, 50);
    for (Hex next : HexGrid::neighbours(Hex{2, -1}))
      EXPECT_NEAR(gap(layout.centre(next), layout.centre(Hex{2, -1})), 17.3205f, 1e-3f);
  }
}

TEST(LayoutTest, CornersAreAtTheSize) {
  const Layout layout{Orientation::PointyTop, {10, 10}, {0, 0}};
  for (Point corner : layout.corners(Hex{1, 1}))
    EXPECT_NEAR(gap(corner, layout.centre(Hex{1, 1})), 10, 1e-4f);
  // Pointy top: a corner straight above or below the centre
  bool vertical = false;
  for (Point corner : layout.corners(Hex{}))
    vertical =
        vertical || (std::abs(corner.x) < 1e-4f && std::abs(std::abs(corner.y) - 10) < 1e-4f);
  EXPECT_TRUE(vertical);
}

// A point at a hex's centre, or near it, is in that hex
TEST(LayoutTest, PointsFindTheirHex) {
  for (Orientation orientation : {Orientation::PointyTop, Orientation::FlatTop}) {
    const Layout layout{orientation, {12, 12}, {128, 96}};
    for (Hex hex : HexGrid::spiral(Hex{}, 4)) {
      const Point c = layout.centre(hex);
      EXPECT_EQ(layout.hexAt(c), hex);
      EXPECT_EQ(layout.hexAt({c.x + 4, c.y - 3}), hex);
    }
  }
}
