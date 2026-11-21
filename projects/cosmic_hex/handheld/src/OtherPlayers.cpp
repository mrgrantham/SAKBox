#include <CosmicHex/Handheld/CardsHeld.hpp>
#include <CosmicHex/Handheld/OtherPlayers.hpp>

#include <HexGrid/CornersAndEdges.hpp>

#include <algorithm>
#include <numeric>

namespace CosmicHex::Handheld {

PlayerView viewOf(const State &state, int player) {
  PlayerView view;
  view.player = player;
  view.points = points(state, player);
  const Player &p = state.player[static_cast<std::size_t>(player)];
  view.resourceCards = std::accumulate(p.resources.begin(), p.resources.end(), 0);
  view.actionCards = static_cast<int>(p.contracts.size());
  view.badges = p.badges; // (held: every 3 become a point at once, counted in points)
  view.ships = static_cast<int>(std::count_if(state.ships.begin(), state.ships.end(),
                                              [&](const Ship &s) { return s.owner == player; }));
  std::vector<int> seen; // (planets already listed, by index)
  for (const Station &station : state.stations) {
    if (station.owner != player)
      continue;
    ++(station.colony ? view.colonies : view.stations);
    for (const Hex hex : HexGrid::hexesTouching(station.at))
      for (std::size_t i = 0; i < state.planets.size(); ++i) {
        const Planet &planet = state.planets[i];
        if (planet.hex != hex || !planet.flipped || planet.number <= 0)
          continue;
        const auto index = static_cast<int>(i);
        const auto listed = std::find(seen.begin(), seen.end(), index);
        if (listed != seen.end()) {
          // (a colony on it as well as a station: shown as the colony's)
          if (station.colony)
            view.planets[static_cast<std::size_t>(listed - seen.begin())].colony = true;
          continue;
        }
        seen.push_back(index);
        view.planets.push_back({planet.kind, planet.number, station.colony});
      }
  }
  for (const int id : p.revealed)
    if (const auto card = cardOf(id))
      view.charters.push_back(*card);
  return view;
}

std::vector<int> othersOf(const State &state, int you) {
  std::vector<int> out;
  for (int i = 1; i < state.players; ++i)
    out.push_back((you + i) % state.players);
  return out;
}

} // namespace CosmicHex::Handheld
