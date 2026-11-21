// Board.cpp - the board's lookups, shared by every part of the turn: the
// hex index, corners and how far a ship can reach from each, and whose
// stations and ships are where.
#include "RulesInternal.hpp"

#include <HexGrid/HexagonMap.hpp>
#include <HexGrid/Paths.hpp>

namespace CosmicHex {

namespace Internal {

namespace {
// Ships pass anything, so which corners one can reach depends only on the
// board's shape: the table holds every corner within kReachTabled edges,
// nearest first (then by number) (the search it replaces, with its hash
// map, was the bots' largest cost)
constexpr int kReachTabled = 5; // (a move and its boost: 3 + 2; a starting ship's 3)
} // namespace

bool removeOne(std::vector<int> &from, int value) {
  const auto at = std::find(from.begin(), from.end(), value);
  if (at == from.end())
    return false;
  from.erase(at);
  return true;
}

// Fills State's hex index from its planets and zone tiles
void indexHexes(State &state) {
  state.planetOnHex.fill(-1);
  state.siteOnHex.fill(-1);
  for (std::size_t i = 0; i < state.planets.size(); ++i)
    if (const int cell = hexCell(state.planets[i].hex); cell >= 0)
      state.planetOnHex[static_cast<std::size_t>(cell)] = static_cast<std::int8_t>(i);
  for (std::size_t i = 0; i < state.zone.size(); ++i)
    if (const int cell = hexCell(state.zone[i].hex); cell >= 0)
      state.siteOnHex[static_cast<std::size_t>(cell)] = static_cast<std::int8_t>(i);
  state.hexesIndexed = true;
}

bool onBoard(Corner corner) {
  for (Hex hex : HexGrid::hexesTouching(corner))
    if (HexGrid::length(hex) <= kBoardRadius)
      return true;
  return false;
}

const std::vector<std::vector<CornerReach>> &cornerReachTable() {
  static const std::vector<std::vector<CornerReach>> kTable = [] {
    const std::vector<Corner> &all = boardCorners();
    std::vector<std::vector<CornerReach>> table(all.size());
    for (std::size_t i = 0; i < all.size(); ++i) {
      for (const HexGrid::Reach<Corner> &reach :
           HexGrid::reachable(all[i], kReachTabled, [](Corner c) { return onBoard(c); }))
        if (reach.place != all[i])
          table[i].push_back({static_cast<std::int16_t>(cornerNumber(reach.place)),
                              static_cast<std::int16_t>(reach.steps)});
      std::sort(table[i].begin(), table[i].end(), [](CornerReach a, CornerReach b) {
        return a.steps != b.steps ? a.steps < b.steps : a.corner < b.corner;
      });
    }
    return table;
  }();
  return kTable;
}

// From the table, or searched for beyond its reach
std::vector<CornerReach> cornersWithin(Corner from, int steps) {
  if (steps <= kReachTabled) {
    const std::vector<CornerReach> &all =
        cornerReachTable()[static_cast<std::size_t>(cornerNumber(from))];
    const auto end = std::partition_point(all.begin(), all.end(),
                                          [steps](CornerReach r) { return r.steps <= steps; });
    return {all.begin(), end};
  }
  std::vector<CornerReach> out;
  for (const HexGrid::Reach<Corner> &reach :
       HexGrid::reachable(from, steps, [](Corner c) { return onBoard(c); }))
    if (reach.place != from)
      out.push_back({static_cast<std::int16_t>(cornerNumber(reach.place)),
                     static_cast<std::int16_t>(reach.steps)});
  return out;
}

// The player's home colony (the first station they placed), or -1
int homeColonyOf(const State &state, int player) {
  for (std::size_t i = 0; i < state.stations.size(); ++i)
    if (state.stations[i].owner == player && state.stations[i].colony)
      return static_cast<int>(i);
  return -1;
}

bool dockedAt(const Planet &planet, int player) {
  return planet.docks[0] == player || planet.docks[1] == player;
}
int freeDock(const Planet &planet) {
  for (int i = 0; i < kDocks; ++i) {
    if (planet.docks[static_cast<std::size_t>(i)] < 0)
      return i;
  }
  return -1;
}

// Whether the player has a station on the planet (docked, or on a corner
// touching it), or a colony
bool hasStationOn(const State &state, const Planet &planet, int player) {
  if (!state.settings.corners)
    return dockedAt(planet, player);
  for (const Station &station : state.stations)
    if (station.owner == player && touches(station.at, planet.hex))
      return true;
  return false;
}
bool hasColonyOn(const State &state, const Planet &planet, int player) {
  if (!state.settings.corners) {
    for (int d = 0; d < kDocks; ++d)
      if (planet.docks[static_cast<std::size_t>(d)] == player &&
          planet.colony[static_cast<std::size_t>(d)])
        return true;
    return false;
  }
  for (const Station &station : state.stations)
    if (station.owner == player && station.colony && touches(station.at, planet.hex))
      return true;
  return false;
}
// How many players have stations on the planet (without listing them)
int ownerCount(const State &state, const Planet &planet) {
  int count = 0;
  for (int p = 0; p < state.players; ++p)
    count += hasStationOn(state, planet, p);
  return count;
}

int shipsOf(const State &state, int player) {
  return static_cast<int>(std::count_if(state.ships.begin(), state.ships.end(),
                                        [&](const Ship &s) { return s.owner == player; }));
}

// Stations on core planets (with corners: stations touching one)
int coreStationsOf(const State &state, int player) {
  int count = 0;
  if (!state.settings.corners) {
    for (const Planet &planet : state.planets)
      count += planet.core && dockedAt(planet, player);
    return count;
  }
  for (const Station &station : state.stations) {
    if (station.owner != player)
      continue;
    const bool onCore =
        std::any_of(state.planets.begin(), state.planets.end(),
                    [&](const Planet &p) { return p.core && touches(station.at, p.hex); });
    count += onCore;
  }
  return count;
}

} // namespace Internal

const std::vector<Corner> &boardCorners() {
  static const std::vector<Corner> kCorners =
      HexGrid::everyCorner(HexGrid::spiral(Hex{}, Internal::kBoardRadius));
  return kCorners;
}

[[gnu::hot]] int cornerNumber(Corner corner) {
  const std::vector<Corner> &all = boardCorners();
  const auto found = std::lower_bound(all.begin(), all.end(), corner);
  return found != all.end() && *found == corner ? static_cast<int>(found - all.begin()) : -1;
}

} // namespace CosmicHex
