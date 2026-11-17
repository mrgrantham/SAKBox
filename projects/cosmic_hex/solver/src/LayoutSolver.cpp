#include <CosmicHex/Solver/LayoutSolver.hpp>

#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Hex.hpp>
#include <SeededRandom/Generator.hpp>

#include <algorithm>
#include <deque>
#include <set>
#include <sstream>

namespace CosmicHex::Solver {

namespace {

constexpr int kRadius = 4; // (the rules' board)

bool onBoard(Hex hex) { return HexGrid::length(hex) <= kRadius; }

std::vector<Hex> homesOf(HomePlaces places) {
  switch (places) {
  case HomePlaces::Edge:
    return {{4, -2}, {-4, 2}, {2, 2}, {-2, -2}};
  case HomePlaces::Corners:
    return {{4, -4}, {-4, 4}, {4, 0}, {-4, 0}};
  case HomePlaces::Three:
    return {{4, -2}, {-2, -2}, {-2, 4}};
  }
  return {};
}

// How many times the board repeats round the centre: twice (turned half
// round) for four homes, three times for three; once without symmetry
int symmetryOrder(const SolverSpec &spec) {
  if (!spec.symmetric)
    return 1;
  return spec.homes == HomePlaces::Three ? 3 : 2;
}

// A hex's copies round the board (itself not included)
std::vector<Hex> copiesOf(Hex hex, int order) {
  std::vector<Hex> out;
  for (int k = 1; k < order; ++k)
    out.push_back(HexGrid::rotated(hex, k * 6 / order));
  return out;
}

bool contains(const std::vector<Hex> &hexes, Hex hex) {
  return std::find(hexes.begin(), hexes.end(), hex) != hexes.end();
}

bool touchesAny(Hex hex, const std::vector<Hex> &hexes) {
  for (Hex n : HexGrid::neighbours(hex))
    if (contains(hexes, n))
      return true;
  return false;
}

// A cluster of `size` planets at `at`: one hex, two side by side, or three
// round a corner (each touching the other two), turned by `turn`
std::vector<Hex> clusterShape(Hex at, int size, int turn) {
  std::vector<Hex> out{at};
  if (size >= 2)
    out.push_back(HexGrid::neighbour(at, turn));
  if (size >= 3)
    out.push_back(HexGrid::neighbour(at, turn + 1));
  return out;
}

// One attempt's board as it's built: what's where
struct Board {
  std::vector<Hex> homes, planets, finds;
  std::vector<std::vector<Hex>> clusters; // (the core not among them)
  std::vector<Hex> taken() const {
    std::vector<Hex> all = homes;
    all.insert(all.end(), planets.begin(), planets.end());
    return all;
  }
};

// Whether a cluster can go there: on the board, on nothing, and (if kept
// apart) touching no planet or home
bool fits(const Board &board, const std::vector<Hex> &cluster, bool apart) {
  const std::vector<Hex> taken = board.taken();
  for (Hex hex : cluster) {
    if (!onBoard(hex) || contains(taken, hex))
      return false;
    if (apart && touchesAny(hex, taken))
      return false;
  }
  return true;
}

// The size of the cluster a planet hex belongs to (the core: 1)
int clusterSizeAt(const Board &board, Hex hex) {
  for (const auto &cluster : board.clusters)
    if (contains(cluster, hex))
      return static_cast<int>(cluster.size());
  return 1;
}

// Each home's nearest single, pair and three, in ship moves (a cluster's
// distance: its nearest planet's)
std::array<int, 3> nearestClusters(const Board &board, const std::vector<int> &moves) {
  std::array<int, 3> nearest{1000, 1000, 1000};
  for (const auto &cluster : board.clusters) {
    int d = 1000;
    for (Hex hex : cluster)
      for (int i = 0; i < 6; ++i) {
        const int n = cornerNumber(HexGrid::corner(hex, i));
        if (n >= 0 && moves[static_cast<std::size_t>(n)] >= 0)
          d = std::min(d, moves[static_cast<std::size_t>(n)]);
      }
    const auto size = static_cast<std::size_t>(std::min<std::size_t>(cluster.size(), 3) - 1);
    nearest[size] = std::min(nearest[size], d);
  }
  return nearest;
}

int planetMoves(Hex planet, const std::vector<int> &moves) {
  int d = 1000;
  for (int i = 0; i < 6; ++i) {
    const int n = cornerNumber(HexGrid::corner(planet, i));
    if (n >= 0 && moves[static_cast<std::size_t>(n)] >= 0)
      d = std::min(d, moves[static_cast<std::size_t>(n)]);
  }
  return d;
}

// Tries to place `count` clusters of `size`, each with its copies round the
// board when symmetric; false if there's no room
bool placeClusters(Board &board, int size, int count, const SolverSpec &spec,
                   SeededRandom::Generator &random) {
  const std::vector<Hex> hexes = HexGrid::spiral(Hex{}, kRadius);
  for (int placed = 0; placed < count;) {
    bool done = false;
    for (int tries = 0; tries < 60 && !done; ++tries) {
      const Hex at = hexes[random.below(static_cast<std::uint32_t>(hexes.size()))];
      const std::vector<Hex> cluster = clusterShape(at, size, static_cast<int>(random.below(6)));
      if (!fits(board, cluster, spec.clustersApart))
        continue;
      // The cluster and its copies round the board, each fitting beside
      // the ones placed before it
      const int order = symmetryOrder(spec);
      Board trial = board;
      trial.planets.insert(trial.planets.end(), cluster.begin(), cluster.end());
      trial.clusters.push_back(cluster);
      bool allFit = true;
      for (int k = 1; k < order && allFit; ++k) {
        std::vector<Hex> copy;
        copy.reserve(cluster.size());
        for (Hex hex : cluster)
          copy.push_back(HexGrid::rotated(hex, k * 6 / order));
        allFit = fits(trial, copy, spec.clustersApart);
        trial.planets.insert(trial.planets.end(), copy.begin(), copy.end());
        trial.clusters.push_back(copy);
      }
      if (!allFit)
        continue;
      board = trial;
      placed += order;
      done = true;
    }
    if (!done)
      return false;
  }
  return true;
}

// A count within a bound (a multiple of the symmetry's copies), at random,
// kept to what a board could hold
int countWithin(const Bound &bound, int order, int most, SeededRandom::Generator &random) {
  const int floor = std::max(0, bound.floor), ceiling = std::min(bound.ceiling, most);
  if (ceiling < floor)
    return -1;
  std::vector<int> choices;
  for (int n = floor; n <= ceiling; ++n)
    if (n % order == 0)
      choices.push_back(n);
  if (choices.empty())
    return -1;
  return choices[random.below(static_cast<std::uint32_t>(choices.size()))];
}

// One attempt: a board meeting the spec's counts and menu, or nothing
std::optional<SolvedLayout> attempt(const SolverSpec &spec, SeededRandom::Generator &random) {
  Board board;
  board.homes = homesOf(spec.homes);
  if (spec.core)
    board.planets.push_back(Hex{0, 0});
  const int order = symmetryOrder(spec);
  const int threes = countWithin(spec.threes, order, 6, random);
  const int pairs = countWithin(spec.pairs, order, 9, random);
  const int singles = countWithin(spec.singles, order, 12, random);
  if (threes < 0 || pairs < 0 || singles < 0)
    return std::nullopt;
  // Bigger clusters first, while there's room
  if (!placeClusters(board, 3, threes, spec, random) ||
      !placeClusters(board, 2, pairs, spec, random) ||
      !placeClusters(board, 1, singles, spec, random))
    return std::nullopt;

  // The finds: empty hexes not beside a home (nor beside clusters of the
  // menu's size and up), each with its copies round the board when symmetric
  std::vector<Hex> spots;
  for (Hex hex : HexGrid::spiral(Hex{}, kRadius)) {
    if (contains(board.taken(), hex) || touchesAny(hex, board.homes))
      continue;
    bool nearBig = false;
    if (spec.findsAwayFromClustersOf)
      for (Hex n : HexGrid::neighbours(hex))
        if (contains(board.planets, n) && clusterSizeAt(board, n) >= *spec.findsAwayFromClustersOf)
          nearBig = true;
    if (!nearBig)
      spots.push_back(hex);
  }
  const int finds = countWithin(spec.finds, order, static_cast<int>(spots.size()), random);
  if (finds < 0)
    return std::nullopt;
  for (int tries = 0; static_cast<int>(board.finds.size()) < finds; ++tries) {
    if (tries > 2000)
      return std::nullopt; // (no room for that many, as the copies need)
    const Hex hex = spots[random.below(static_cast<std::uint32_t>(spots.size()))];
    if (contains(board.finds, hex))
      continue;
    const std::vector<Hex> copies = copiesOf(hex, order);
    const bool copiesFree = std::all_of(copies.begin(), copies.end(), [&](Hex c) {
      return c != hex && contains(spots, c) && !contains(board.finds, c);
    });
    if (!copiesFree)
      continue;
    board.finds.push_back(hex);
    board.finds.insert(board.finds.end(), copies.begin(), copies.end());
  }

  // From each home: the nearest clusters, the planets in reach, the access
  SolvedLayout out;
  out.homes = board.homes;
  out.planets = board.planets;
  out.finds = board.finds;
  for (const auto &cluster : board.clusters)
    ++out.clusters[std::min<std::size_t>(cluster.size(), 3) - 1];
  for (Hex home : board.homes) {
    const std::vector<int> moves = shipMovesFrom(home);
    const std::array<int, 3> nearest = nearestClusters(board, moves);
    for (std::size_t i = 0; i < 3; ++i)
      if (out.clusters[i] > 0 && !spec.nearestFromEveryHome[i].holds(nearest[i]))
        return std::nullopt;
    if (spec.threesFartherThanSmaller && out.clusters[2] > 0 &&
        (nearest[2] <= nearest[0] || nearest[2] <= nearest[1]))
      return std::nullopt;
    double access = 0;
    int inReach = 0;
    std::vector<int> near(static_cast<std::size_t>(std::max(spec.sameNearPlanetsWithin, 0)) + 1, 0);
    for (Hex planet : board.planets) {
      const int d = planetMoves(planet, moves);
      access += 1.0 / (1.0 + d);
      inReach += d <= spec.reachMoves;
      for (int within = 1; within <= spec.sameNearPlanetsWithin; ++within)
        near[static_cast<std::size_t>(within)] += d <= within;
    }
    if (spec.sameNearPlanetsWithin > 0) {
      if (!out.near.empty() && out.near.front() != near)
        return std::nullopt; // (a home with more or fewer planets near it)
      out.near.push_back(near);
    }
    if (!spec.planetsInReach.holds(inReach))
      return std::nullopt;
    out.nearest.push_back(nearest);
    out.access.push_back(access);
  }
  const auto [low, high] = std::minmax_element(out.access.begin(), out.access.end());
  out.accessSpread = *high - *low;
  return out;
}

} // namespace

std::vector<int> shipMovesFrom(Hex home) {
  const std::vector<Corner> &corners = boardCorners();
  std::vector<int> moves(corners.size(), -1);
  std::deque<Corner> next;
  for (int i = 0; i < 6; ++i) {
    const Corner c = HexGrid::corner(home, i);
    const int n = cornerNumber(c);
    if (n >= 0 && moves[static_cast<std::size_t>(n)] < 0) {
      moves[static_cast<std::size_t>(n)] = 0;
      next.push_back(c);
    }
  }
  while (!next.empty()) {
    const Corner c = next.front();
    next.pop_front();
    const int here = moves[static_cast<std::size_t>(cornerNumber(c))];
    for (const Corner n : HexGrid::neighbours(c)) {
      const int number = cornerNumber(n);
      if (number >= 0 && moves[static_cast<std::size_t>(number)] < 0) {
        moves[static_cast<std::size_t>(number)] = here + 1;
        next.push_back(n);
      }
    }
  }
  return moves;
}

std::vector<SolvedLayout> solve(const SolverSpec &spec) {
  SeededRandom::Generator random(spec.seed);
  std::vector<SolvedLayout> found;
  std::set<std::vector<std::pair<int, int>>> seen; // (each board once)
  for (int i = 0; i < spec.attempts; ++i) {
    auto layout = attempt(spec, random);
    if (!layout)
      continue;
    std::vector<std::pair<int, int>> key;
    for (Hex hex : layout->planets)
      key.emplace_back(hex.q, hex.r);
    std::sort(key.begin(), key.end());
    if (!seen.insert(key).second)
      continue;
    found.push_back(std::move(*layout));
  }
  std::sort(found.begin(), found.end(), [](const SolvedLayout &a, const SolvedLayout &b) {
    return a.accessSpread < b.accessSpread;
  });
  if (static_cast<int>(found.size()) > spec.keep)
    found.resize(static_cast<std::size_t>(spec.keep));
  return found;
}

std::string layoutLine(const SolvedLayout &layout, const std::string &name) {
  const auto list = [](const std::vector<Hex> &hexes) {
    std::ostringstream out;
    for (std::size_t i = 0; i < hexes.size(); ++i)
      out << (i ? "," : "") << hexes[i].q << "," << hexes[i].r;
    return out.str();
  };
  std::ostringstream line;
  const std::array<int, 3> reach =
      layout.nearest.empty() ? std::array<int, 3>{} : layout.nearest[0];
  line << name << " reach=" << reach[0] << "," << reach[1] << "," << reach[2]
       << " homes=" << list(layout.homes) << " planets=" << list(layout.planets);
  if (!layout.finds.empty())
    line << " small_finds=" << list(layout.finds);
  return line.str();
}

} // namespace CosmicHex::Solver
