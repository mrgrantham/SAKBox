// LayoutSolver.hpp - boards made to order (rounds 56 to 60, the user's): a
// search for Cosmic Hex layouts that meet the constraints you set, ranked by
// how equal every home's access to planets is, to choose from and simulate.
//
// What's kept and how much of it (homes' places, how many singles, pairs and
// threes, symmetry, clusters never touching, how many finds) can each be
// fixed, or left free between a floor and a ceiling (Bound). The rest are a
// menu of constraint kinds (round 58: a menu first, a rule language much
// later). Distances are a ship's moves along edges from the home's corners,
// as the game is played.
//
//   SolverSpec spec;
//   spec.threes = Bound::exactly(2);
//   spec.findsAwayFromClustersOf = 3;          // "finds never touch a three"
//   spec.threesFartherThanSmaller = true;      // "threes farther from every home"
//   for (const SolvedLayout &layout : solve(spec)) ...   // the best first
#pragma once

#include <CosmicHex/Rules.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Solver {

// A number kept to a value, or free between a floor and a ceiling
struct Bound {
  int floor = 0, ceiling = 1000;
  static Bound exactly(int n) { return {n, n}; }
  static Bound atLeast(int n) { return {n, 1000}; }
  static Bound atMost(int n) { return {0, n}; }
  bool holds(int n) const { return n >= floor && n <= ceiling; }
};

// Where the four homes are: on the board's edge midway along four sides (the
// R layouts), or at four of its corners (the S layouts)
enum class HomePlaces : std::uint8_t { Edge, Corners };

struct SolverSpec {
  HomePlaces homes = HomePlaces::Edge;
  // How many clusters of each size (1, 2 and 3 planets), besides the core
  Bound singles = Bound{2, 6}, pairs = Bound{1, 4}, threes = Bound{1, 2};
  bool core = true;          // a planet at the centre
  bool symmetric = true;     // the same seen from opposite homes (turned half round)
  bool clustersApart = true; // clusters never touch each other, the core or a home
  Bound finds = Bound{6, 12};

  // The menu
  // Finds never touch a cluster of this many planets or more (3: no find
  // beside a three); none: anywhere empty
  std::optional<int> findsAwayFromClustersOf = 3;
  // From every home, the nearest three is farther than its nearest pair and
  // its nearest single
  bool threesFartherThanSmaller = false;
  // From every home, the nearest single, pair and three within these ship moves
  std::array<Bound, 3> nearestFromEveryHome{};
  // Every home has at least this many planets within this many ship moves
  int reachMoves = 4;
  Bound planetsInReach = Bound::atLeast(0);

  // The search
  int attempts = 20000;
  std::uint64_t seed = 1;
  int keep = 12; // the best this many, different from each other
};

struct SolvedLayout {
  std::vector<Hex> homes, planets, finds; // planets: the core included
  std::array<int, 3> clusters{};          // singles, pairs, threes
  // Each home's nearest single, pair and three, in ship moves
  std::vector<std::array<int, 3>> nearest;
  // Each home's access (planets weighted by nearness: 1 / (1 + moves)), and
  // the spread between the best and worst served home (the ranking)
  std::vector<double> access;
  double accessSpread = 0;
};

// The layouts found, the most equal first
std::vector<SolvedLayout> solve(const SolverSpec &spec);

// A layout as a line of a layouts file (layouts/layouts.txt), named `name`
std::string layoutLine(const SolvedLayout &layout, const std::string &name);

// Ship moves along edges from a home (any of its corners) to every corner
// of the board: by cornerNumber, -1 where it can't reach
std::vector<int> shipMovesFrom(Hex home);

} // namespace CosmicHex::Solver
