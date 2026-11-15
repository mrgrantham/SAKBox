// Paths.hpp - moving across a hex grid: where a piece can get to in a number
// of steps, and the shortest way between two hexes, around hexes it can't
// enter. Each step costs the same (a breadth-first search), and neighbours
// are always tried in direction order, so the same board gives the same
// answer every time.
//
//   auto open = [&](HexGrid::Hex h) { return board.contains(h) && !blockaded(h); };
//   for (const HexGrid::Reach &reach : HexGrid::reachable(ship, 3, open)) ...
//   std::vector<HexGrid::Hex> way = HexGrid::shortestPath(ship, planet, open);
#pragma once

#include <HexGrid/Hex.hpp>

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <vector>

namespace HexGrid {

struct Reach {
  Hex hex;
  int steps = 0;
  bool operator==(const Reach &) const = default;
};

// Every hex `from` can get to in at most `maxSteps` steps, entering only
// hexes `canEnter` allows, with the fewest steps to each: `from` first (0
// steps), then by steps, and in search order within a step.
template <typename CanEnter>
std::vector<Reach> reachable(Hex from, int maxSteps, CanEnter &&canEnter) {
  std::vector<Reach> out{{from, 0}};
  std::unordered_map<Hex, int> seen{{from, 0}};
  for (std::size_t i = 0; i < out.size(); ++i) {
    const Reach here = out[i];
    if (here.steps == maxSteps)
      continue;
    for (Hex next : neighbours(here.hex)) {
      if (seen.contains(next) || !canEnter(next))
        continue;
      seen.emplace(next, here.steps + 1);
      out.push_back({next, here.steps + 1});
    }
  }
  return out;
}

// The shortest way from `from` to `to`, entering only hexes `canEnter`
// allows (the goal too): every hex on it, both ends included. Empty if
// there's no way, or none within `maxSteps`.
template <typename CanEnter>
std::vector<Hex> shortestPath(Hex from, Hex to, CanEnter &&canEnter, int maxSteps = 1 << 20) {
  if (from == to)
    return {from};
  std::unordered_map<Hex, Hex> cameFrom{{from, from}};
  std::deque<std::pair<Hex, int>> frontier{{from, 0}};
  while (!frontier.empty()) {
    const auto [here, steps] = frontier.front();
    frontier.pop_front();
    if (steps == maxSteps)
      continue;
    for (Hex next : neighbours(here)) {
      if (cameFrom.contains(next) || !canEnter(next))
        continue;
      cameFrom.emplace(next, here);
      if (next == to) {
        std::vector<Hex> way{to};
        for (Hex back = here; back != from; back = cameFrom.at(back))
          way.push_back(back);
        way.push_back(from);
        std::reverse(way.begin(), way.end());
        return way;
      }
      frontier.emplace_back(next, steps + 1);
    }
  }
  return {};
}

} // namespace HexGrid
