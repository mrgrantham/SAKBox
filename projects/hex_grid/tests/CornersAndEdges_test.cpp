// Corners (where three hexes meet) and edges (where two meet): every one
// has a single name, touches the right hexes, and joins up with the others
// the way a board's corners and edges do.
#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Layout.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using HexGrid::Corner;
using HexGrid::Edge;
using HexGrid::Hex;

namespace {

template <typename List> bool holds(const List &list, Hex hex) {
  return std::find(list.begin(), list.end(), hex) != list.end();
}

// Every hex within 3 of the centre: enough to cover corners in every
// direction, and the board's edge
const std::vector<Hex> kBoard = HexGrid::spiral(Hex{}, 3);

} // namespace

// Corner i of a hex touches that hex and its neighbours in directions i and
// i + 1.
TEST(CornersTest, ACornerTouchesItsThreeHexes) {
  for (Hex hex : kBoard) {
    for (int i = 0; i < 6; ++i) {
      const auto touching = HexGrid::hexesTouching(HexGrid::corner(hex, i));
      EXPECT_TRUE(holds(touching, hex));
      EXPECT_TRUE(holds(touching, HexGrid::neighbour(hex, i)));
      EXPECT_TRUE(holds(touching, HexGrid::neighbour(hex, i + 1)));
    }
  }
}

// The three hexes at a corner all call it by the same name.
TEST(CornersTest, ACornerHasOneName) {
  for (Hex hex : kBoard) {
    for (Corner c : HexGrid::corners(hex)) {
      EXPECT_TRUE(c.index == 0 || c.index == 1);
      for (Hex other : HexGrid::hexesTouching(c)) {
        const auto theirs = HexGrid::corners(other);
        EXPECT_NE(std::find(theirs.begin(), theirs.end(), c), theirs.end());
      }
    }
  }
  // Going round: corner 6 is corner 0, corner -1 is corner 5
  EXPECT_EQ(HexGrid::corner(Hex{2, -1}, 6), HexGrid::corner(Hex{2, -1}, 0));
  EXPECT_EQ(HexGrid::corner(Hex{2, -1}, -1), HexGrid::corner(Hex{2, -1}, 5));
}

// An edge sits between a hex and its neighbour in that direction, and the
// neighbour calls it by the same name from the other side.
TEST(EdgesTest, AnEdgeIsBetweenTwoNeighbours) {
  for (Hex hex : kBoard) {
    for (int i = 0; i < 6; ++i) {
      const Edge e = HexGrid::edge(hex, i);
      EXPECT_TRUE(e.index >= 0 && e.index <= 2);
      const auto touching = HexGrid::hexesTouching(e);
      EXPECT_TRUE(holds(touching, hex));
      EXPECT_TRUE(holds(touching, HexGrid::neighbour(hex, i)));
      EXPECT_EQ(e, HexGrid::edge(HexGrid::neighbour(hex, i), i + 3));
    }
  }
}

// An edge's two ends are different corners, and both touch both its hexes.
TEST(EdgesTest, AnEdgeEndsAtTwoCorners) {
  for (Edge e : HexGrid::everyEdge(kBoard)) {
    const auto [a, b] = HexGrid::endpoints(e);
    EXPECT_NE(a, b);
    for (Hex side : HexGrid::hexesTouching(e)) {
      EXPECT_TRUE(holds(HexGrid::hexesTouching(a), side));
      EXPECT_TRUE(holds(HexGrid::hexesTouching(b), side));
    }
  }
}

// Three edges meet at each corner, and lead to its three neighbours: each
// shares two hexes with it, and the edge between them is found.
TEST(CornersTest, ACornerJoinsThreeOthers) {
  for (Corner c : HexGrid::everyCorner(kBoard)) {
    for (Edge e : HexGrid::edgesMeeting(c)) {
      const auto ends = HexGrid::endpoints(e);
      EXPECT_TRUE(ends[0] == c || ends[1] == c);
    }
    const auto next = HexGrid::neighbours(c);
    EXPECT_NE(next[0], next[1]);
    EXPECT_NE(next[1], next[2]);
    EXPECT_NE(next[0], next[2]);
    for (Corner n : next) {
      int shared = 0;
      for (Hex hex : HexGrid::hexesTouching(n))
        shared += holds(HexGrid::hexesTouching(c), hex) ? 1 : 0;
      EXPECT_EQ(shared, 2);
      const std::optional<Edge> between = HexGrid::edgeBetween(c, n);
      if (!between) {
        ADD_FAILURE() << "no edge found between neighbouring corners";
        continue;
      }
      const auto ends = HexGrid::endpoints(*between);
      EXPECT_TRUE((ends[0] == c && ends[1] == n) || (ends[0] == n && ends[1] == c));
    }
  }
  // Two corners apart: no edge between
  const Corner c = HexGrid::corner(Hex{}, 0);
  EXPECT_FALSE(HexGrid::edgeBetween(c, HexGrid::corner(Hex{}, 2)).has_value());
}

// A hexagon-shaped board of radius R has 6 x (R + 1)^2 corners, and
// (corners + hexes - 1) edges (Euler's formula for a flat map).
TEST(CornersTest, ABoardHasTheRightCount) {
  for (int radius = 0; radius <= 4; ++radius) {
    const std::vector<Hex> board = HexGrid::spiral(Hex{}, radius);
    const std::size_t corners = HexGrid::everyCorner(board).size();
    EXPECT_EQ(corners, static_cast<std::size_t>(6 * (radius + 1) * (radius + 1)));
    EXPECT_EQ(HexGrid::everyEdge(board).size(), corners + board.size() - 1);
  }
}

// On screen, a corner is at one of each of its hexes' drawn corners, with
// pointy and flat tops alike.
TEST(CornersTest, OnScreenAtTheHexesCorners) {
  for (auto orientation : {HexGrid::Orientation::PointyTop, HexGrid::Orientation::FlatTop}) {
    const HexGrid::Layout layout{orientation, {10, 10}, {50, 40}};
    for (Hex hex : HexGrid::spiral(Hex{}, 1)) {
      for (Corner c : HexGrid::corners(hex)) {
        const HexGrid::Point p = layout.point(c);
        for (Hex touching : HexGrid::hexesTouching(c)) {
          bool found = false;
          for (HexGrid::Point drawn : layout.corners(touching))
            found = found || (std::abs(drawn.x - p.x) < 0.01f && std::abs(drawn.y - p.y) < 0.01f);
          EXPECT_TRUE(found);
        }
      }
    }
  }
}
