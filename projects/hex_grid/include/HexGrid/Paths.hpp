// Paths.hpp - moving across a hex grid: where a piece can get to in a number
// of steps, and the shortest way between two places, around places it can't
// enter. A place is a hex (moving hex to hex) or a corner (moving corner to
// corner along edges, from CornersAndEdges.hpp); the searches are the same
// for both, stepping to a place's `neighbours`. Each step costs the same (a
// breadth-first search), and neighbours are always tried in the same order,
// so the same board gives the same answer every time.
//
//   auto open = [&](HexGrid::Hex h) { return board.contains(h) && !blockaded(h); };
//   for (const HexGrid::Reach<HexGrid::Hex> &reach : HexGrid::reachable(ship, 3, open)) ...
//   std::vector<HexGrid::Hex> way = HexGrid::shortestPath(ship, planet, open);
//
//   auto onBoard = [&](HexGrid::Corner c) { return touchesBoard(c); };
//   std::vector<HexGrid::Corner> lane = HexGrid::shortestPath(from, to, onBoard);
#pragma once

#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Hex.hpp>

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <vector>

namespace HexGrid {

// A place a piece can get to, and the fewest steps it takes.
template <typename Place> struct Reach {
  Place place;
  int steps = 0;
  bool operator==(const Reach &) const = default;
};

// Every place `from` can get to in at most `maxSteps` steps, entering only
// places `canEnter` allows, with the fewest steps to each: `from` first (0
// steps), then by steps, and in search order within a step.
//
// `canEnter` says where a piece may pass through. Where it may stop can be
// stricter (in Starfarers a ship passes occupied corners but can't stop on
// one): filter the result for that.
template <typename Place, typename CanEnter>
std::vector<Reach<Place>> reachable(Place from, int maxSteps, CanEnter &&canEnter) {
  std::vector<Reach<Place>> out{{from, 0}};
  std::unordered_map<Place, int> seen{{from, 0}};
  for (std::size_t i = 0; i < out.size(); ++i) {
    const Reach<Place> here = out[i];
    if (here.steps == maxSteps)
      continue;
    for (Place next : neighbours(here.place)) {
      if (seen.contains(next) || !canEnter(next))
        continue;
      seen.emplace(next, here.steps + 1);
      out.push_back({next, here.steps + 1});
    }
  }
  return out;
}

// The shortest way from `from` to `to`, entering only places `canEnter`
// allows (the goal too): every place on it, both ends included. Empty if
// there's no way, or none within `maxSteps`.
template <typename Place, typename CanEnter>
std::vector<Place> shortestPath(Place from, Place to, CanEnter &&canEnter, int maxSteps = 1 << 20) {
  if (from == to)
    return {from};
  std::unordered_map<Place, Place> cameFrom{{from, from}};
  std::deque<std::pair<Place, int>> frontier{{from, 0}};
  while (!frontier.empty()) {
    const auto [here, steps] = frontier.front();
    frontier.pop_front();
    if (steps == maxSteps)
      continue;
    for (Place next : neighbours(here)) {
      if (cameFrom.contains(next) || !canEnter(next))
        continue;
      cameFrom.emplace(next, here);
      if (next == to) {
        std::vector<Place> way{to};
        for (Place back = here; back != from; back = cameFrom.at(back))
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
