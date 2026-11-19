// LayoutSolver.hpp - boards made to order (the user's): a
// search for Cosmic Hex layouts that meet the constraints you set, ranked by
// how equal every home's access to planets is, to choose from and simulate.
//
// What's kept and how much of it (homes' places, how many singles, pairs and
// threes, symmetry, clusters never touching, how many finds) can each be
// fixed, or left free between a floor and a ceiling (Bound). The rest are a
// menu of constraint kinds (a menu first, a rule language much
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

// Where the homes are: four on the board's edge midway along four sides
// (the R layouts), four at its corners (the S layouts), or three on the
// edge a third of a turn apart (for three players, as the rules' own)
enum class HomePlaces : std::uint8_t { Edge, Corners, Three };

struct SolverSpec {
  HomePlaces homes = HomePlaces::Edge;
  // How many clusters of each size (1, 2, 3 and 4 planets), besides the
  // core; a four is a compact diamond (the user's: to try)
  Bound singles = Bound{2, 6}, pairs = Bound{1, 4}, threes = Bound{1, 2}, fours = Bound{0, 0};
  // A lone planet at the centre. Off (the user's): the centre is
  // like any other hex, free for any cluster to cover, or to stay empty
  bool core = false;
  // The same seen from every home: turned half round with four homes, a
  // third of a turn with three
  bool symmetric = true;
  bool clustersApart = true; // clusters never touch each other, the core or a home
  Bound finds = Bound{6, 12};

  // The menu
  // Finds never touch a cluster of this many planets or more (3: no find
  // beside a three); none: anywhere empty
  std::optional<int> findsAwayFromClustersOf = 3;
  // From every home, the nearest three (and four) is farther than its
  // nearest pair and its nearest single
  bool threesFartherThanSmaller = false;
  // From every home, the nearest single, pair, three and four within these
  // ship moves
  std::array<Bound, 4> nearestFromEveryHome{};
  // Every home has at least this many planets within this many ship moves
  int reachMoves = 4;
  Bound planetsInReach = Bound::atLeast(0);
  // Every home has the same number of planets within each distance up to
  // this many ship moves (0: off). What the simulations found: of the solver's
  // boards, the one with equal access overall but a planet fewer within 2
  // moves for two homes was the most unfair (those homes won 20% to the
  // others' 29%); every fair board had equal counts near every home
  int sameNearPlanetsWithin = 0;

  // The search
  int attempts = 20000;
  std::uint64_t seed = 1;
  int keep = 12; // the best this many, different from each other
};

struct SolvedLayout {
  std::vector<Hex> homes, planets, finds; // planets: the core included
  std::array<int, 4> clusters{};          // singles, pairs, threes, fours
  // Each home's nearest single, pair, three and four, in ship moves
  std::vector<std::array<int, 4>> nearest;
  // Each home's access (planets weighted by nearness: 1 / (1 + moves)), and
  // the spread between the best and worst served home (the ranking)
  std::vector<double> access;
  double accessSpread = 0;
  // (sameNearPlanetsWithin) each home's planets within 1, 2, ... moves
  std::vector<std::vector<int>> planetsNear; // (not "near": a macro in Windows' headers)
};

// The layouts found, the most equal first
std::vector<SolvedLayout> solve(const SolverSpec &spec);

// A layout as a line of a layouts file (layouts/layouts.txt), named `name`
std::string layoutLine(const SolvedLayout &layout, const std::string &name);

// Ship moves along edges from a home (any of its corners) to every corner
// of the board: by cornerNumber, -1 where it can't reach
std::vector<int> shipMovesFrom(Hex home);

} // namespace CosmicHex::Solver
