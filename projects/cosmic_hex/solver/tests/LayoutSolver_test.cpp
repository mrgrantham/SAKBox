// The solver: every layout it gives meets the spec (counts, clusters apart,
// symmetry, finds away from threes, threes farther), and they come ranked
#include <CosmicHex/Solver/LayoutSolver.hpp>

#include <HexGrid/Hex.hpp>

#include <gtest/gtest.h>

#include <algorithm>

using namespace CosmicHex;
using namespace CosmicHex::Solver;

namespace {
bool has(const std::vector<Hex> &hexes, Hex hex) {
  return std::find(hexes.begin(), hexes.end(), hex) != hexes.end();
}
} // namespace

TEST(LayoutSolverTest, ShipMovesCountEdges) {
  // From a home, its own corners are 0, and a corner's neighbours 1 more
  const auto moves = shipMovesFrom(Hex{4, -2});
  EXPECT_EQ(moves[static_cast<std::size_t>(cornerNumber(HexGrid::corner(Hex{4, -2}, 0)))], 0);
  const auto far = moves[static_cast<std::size_t>(cornerNumber(HexGrid::corner(Hex{-4, 2}, 0)))];
  EXPECT_GT(far, 8); // (across the board)
}

TEST(LayoutSolverTest, LayoutsMeetTheirSpec) {
  SolverSpec spec;
  spec.threes = Bound::exactly(2);
  spec.pairs = Bound{2, 4};
  spec.singles = Bound{2, 6};
  spec.finds = Bound{6, 10};
  spec.findsAwayFromClustersOf = 3;
  spec.threesFartherThanSmaller = true;
  spec.attempts = 3000;
  const auto layouts = solve(spec);
  ASSERT_FALSE(layouts.empty());
  for (std::size_t i = 1; i < layouts.size(); ++i)
    EXPECT_LE(layouts[i - 1].accessSpread, layouts[i].accessSpread); // (ranked)
  for (const SolvedLayout &l : layouts) {
    EXPECT_EQ(l.clusters[2], 2);
    EXPECT_TRUE(spec.pairs.holds(l.clusters[1]));
    EXPECT_TRUE(spec.finds.holds(static_cast<int>(l.finds.size())));
    // Symmetric: each planet's mirror is a planet
    for (Hex p : l.planets)
      EXPECT_TRUE(has(l.planets, Hex{-p.q, -p.r}));
    // No planet beside a home
    for (Hex home : l.homes)
      for (Hex n : HexGrid::neighbours(home))
        EXPECT_FALSE(has(l.planets, n));
    // Threes farther than the rest, from every home
    for (const auto &nearest : l.nearest) {
      EXPECT_GT(nearest[2], nearest[0]);
      EXPECT_GT(nearest[2], nearest[1]);
    }
    EXPECT_FALSE(layoutLine(l, "T1").empty());
  }
}

// Three homes (three players): the board repeats every third of a turn
TEST(LayoutSolverTest, ThreeHomesRepeatByThirds) {
  SolverSpec spec;
  spec.homes = HomePlaces::Three;
  spec.threes = Bound{0, 3};
  spec.pairs = Bound{0, 3};
  spec.singles = Bound{3, 9};
  spec.finds = Bound{3, 9};
  spec.attempts = 3000;
  const auto layouts = solve(spec);
  ASSERT_FALSE(layouts.empty());
  for (const SolvedLayout &l : layouts) {
    EXPECT_EQ(l.homes.size(), 3u);
    for (Hex p : l.planets)
      EXPECT_TRUE(has(l.planets, HexGrid::rotated(p, 2)));
    for (Hex f : l.finds)
      EXPECT_TRUE(has(l.finds, HexGrid::rotated(f, 2)));
    EXPECT_EQ(l.clusters[0] % 3, 0);
  }
}

// Round 83: every home with the same number of planets within each
// distance up to 3 moves
TEST(LayoutSolverTest, SameNearPlanetsFromEveryHome) {
  SolverSpec spec;
  spec.homes = HomePlaces::Corners;
  spec.sameNearPlanetsWithin = 3;
  spec.attempts = 4000;
  const auto layouts = solve(spec);
  ASSERT_FALSE(layouts.empty());
  for (const SolvedLayout &l : layouts) {
    ASSERT_EQ(l.near.size(), l.homes.size());
    for (const auto &counts : l.near)
      EXPECT_EQ(counts, l.near.front());
  }
}
