// BuildingAndMoving.cpp - the main part of a turn's pieces: building ships,
// moving them (a ship's stop turns over what it lands by: Exploring.cpp),
// settling a ship as a station, and upgrading a station to a colony. Each
// under both rules: on corners and edges (Settings::corners), or on hexes
// with two docks a planet.
#include "RulesInternal.hpp"

#include <HexGrid/HexagonMap.hpp>

#include <deque>
#include <unordered_map>

namespace CosmicHex {

using namespace Internal;

namespace {

// Whether a planet a station can be built by (turned over, not a home) has
// a corner here
[[gnu::hot]] bool byASettleablePlanet(const State &state, Corner corner) {
  for (Hex hex : HexGrid::hexesTouching(corner))
    if (const int planet = planetAt(state, hex);
        planet >= 0 && settleable(state.planets[static_cast<std::size_t>(planet)]))
      return true;
  return false;
}

// Whether the player may build a Deep-space relay: holding its Charter
// (revealed or not) and without one yet
bool mayBuildRelay(const State &state, int player) {
  const Player &me = state.player[static_cast<std::size_t>(player)];
  const auto in = [](const std::vector<int> &cards) {
    return std::find(cards.begin(), cards.end(), kDeepSpaceRelay) != cards.end();
  };
  const bool holds = in(me.contracts) || in(me.revealed); // (revealed or not)
  const bool hasOne =
      std::any_of(state.stations.begin(), state.stations.end(),
                  [&](const Station &s) { return s.owner == player && isRelaySpot(state, s.at); });
  return holds && !hasOne;
}

// Where a ship can become a station: by a planet, an outpost, or for a
// relay's holder anywhere
[[gnu::hot]] bool canStandAt(const State &state, int player, Corner corner) {
  return byASettleablePlanet(state, corner) || byAnOutpost(state, corner) ||
         (isRelaySpot(state, corner) && mayBuildRelay(state, player));
}

// Where a ship can move this turn (corner numbers, with the edges to each):
// shipMove edges, and boostMove more for 1 fuel. Ships pass anything; only
// where a move ends matters (discovery). It can't end on a corner taken by
// a station, a colony or another ship (the user's)
std::vector<CornerReach> cornerMovesFor(const State &state, const Ship &ship) {
  const Settings &settings = state.settings;
  const bool canBoost = state.player[static_cast<std::size_t>(ship.owner)]
                                .resources[static_cast<std::size_t>(Resource::Fuel)] >= 1 &&
                        (settings.firstRoundBoost || state.round > 1);
  std::vector<CornerReach> moves =
      cornersWithin(ship.corner, settings.shipMove + (canBoost ? settings.boostMove : 0));
  // The taken corners, by number (a few dozen at most)
  std::vector<std::int16_t> taken;
  taken.reserve(state.stations.size() + state.ships.size());
  for (const Station &station : state.stations)
    taken.push_back(static_cast<std::int16_t>(cornerNumber(station.at)));
  for (const Ship &other : state.ships)
    if (&other != &ship)
      taken.push_back(static_cast<std::int16_t>(cornerNumber(other.corner)));
  std::erase_if(moves, [&](CornerReach reach) {
    return std::find(taken.begin(), taken.end(), reach.corner) != taken.end();
  });
  return moves;
}

// Where a ship can move this turn, with the fewest hexes to each: through
// open space, turned-over tiles and other ships, but not through a
// face-down tile (moving onto one turns it over and stops the ship) or the
// blockade ship's planet. Beyond shipMove hexes needs 1 fuel.
std::unordered_map<Hex, int> movesFor(const State &state, const Ship &ship) {
  const Settings &settings = state.settings;
  const bool canBoost = state.player[static_cast<std::size_t>(ship.owner)]
                            .resources[static_cast<std::size_t>(Resource::Fuel)] >= 1;
  const int reach = settings.shipMove + (canBoost ? settings.boostMove : 0);
  const Hex blockaded = state.blockade >= 0
                            ? state.planets[static_cast<std::size_t>(state.blockade)].hex
                            : Hex{1000, 1000};
  const auto faceDown = [&](Hex hex) {
    const int planet = planetAt(state, hex);
    if (planet >= 0)
      return !state.planets[static_cast<std::size_t>(planet)].flipped;
    const int site = siteAt(state, hex);
    return site >= 0 && !state.zone[static_cast<std::size_t>(site)].flipped;
  };
  std::unordered_map<Hex, int> steps{{ship.at, 0}};
  std::deque<Hex> frontier{ship.at};
  while (!frontier.empty()) {
    const Hex here = frontier.front();
    frontier.pop_front();
    const int at = steps[here];
    if (at == reach || (here != ship.at && faceDown(here)))
      continue; // out of moves, or stopped by turning a tile over
    for (Hex next : HexGrid::neighbours(here)) {
      if (HexGrid::length(next) > kBoardRadius || next == blockaded || steps.contains(next))
        continue;
      steps.emplace(next, at + 1);
      frontier.push_back(next);
    }
  }
  steps.erase(ship.at);
  return steps;
}

// ---- On corners and edges (Settings::corners)

// The player's own station by its index, or null
Station *ownStation(State &state, int index) {
  if (index < 0 || index >= static_cast<int>(state.stations.size()))
    return nullptr;
  Station &found = state.stations[static_cast<std::size_t>(index)];
  return found.owner == state.turn ? &found : nullptr;
}

// A ship at a colony; with Shipyard Surplus a second one free, so room for two
bool buildShipsAtColony(State &state, const Action &action) {
  Player &me = current(state);
  const Settings &settings = state.settings;
  const bool surplus = action.kind == Kind::PlayShipyard;
  const int building = surplus ? 2 : 1;
  const Station *colony = ownStation(state, action.a);
  if (!colony || !colony->colony || shipsOf(state, state.turn) + building > settings.maxShips ||
      !canAfford(me.resources, settings.shipCost) ||
      (surplus && !canPlayCard(state, kShipyardCard)))
    return false;
  if (surplus)
    playCard(state, kShipyardCard);
  pay(me.resources, settings.shipCost);
  // (it moves from next turn; or, when new ships leave, this one: by the
  // usual move, before the turn can end)
  const bool movesNow = settings.newShipsLeave;
  for (int i = 0; i < building; ++i)
    state.ships.push_back(Ship{state.turn, Hex{}, !movesNow, colony->at});
  return true;
}

bool moveAlongEdges(State &state, const Action &action) {
  if (action.a < 0 || action.a >= static_cast<int>(state.ships.size()))
    return false;
  Ship &ship = state.ships[static_cast<std::size_t>(action.a)];
  if (ship.owner != state.turn || ship.moved)
    return false;
  const auto moves = cornerMovesFor(state, ship);
  const auto found =
      std::find_if(moves.begin(), moves.end(), [&](CornerReach r) { return r.corner == action.b; });
  if (found == moves.end())
    return false;
  if (found->steps > state.settings.shipMove)
    --current(state).resources[static_cast<std::size_t>(Resource::Fuel)]; // the boost
  ship.corner = boardCorners()[static_cast<std::size_t>(action.b)];
  ship.moved = true;
  // Where it stops, what it touches turns over
  for (Hex hex : HexGrid::hexesTouching(ship.corner))
    state.turning.push_back(hex);
  turnNextOver(state);
  return true;
}

bool settleOnCorner(State &state, const Action &action) {
  if (action.a < 0 || action.a >= static_cast<int>(state.ships.size()))
    return false;
  Player &me = current(state);
  const Ship ship = state.ships[static_cast<std::size_t>(action.a)];
  if (ship.owner != state.turn || !cornerFree(state, ship.corner) ||
      !canAfford(me.resources, state.settings.stationCost))
    return false;
  // A station must touch a planet it can be built by (turned over, not a
  // home), or an alien outpost
  if (!canStandAt(state, state.turn, ship.corner))
    return false;
  pay(me.resources, state.settings.stationCost);
  state.ships.erase(state.ships.begin() + action.a);
  state.stations.push_back(Station{ship.corner, state.turn, false});
  updateCoreControl(state, state.turn);
  return true;
}

bool buildColonyOnCorner(State &state, const Action &action) {
  Player &me = current(state);
  Station *mine = ownStation(state, action.a);
  // (a station only by an outpost can't become a colony: a colony pays
  // double from its planets, and it has none)
  if (!mine || mine->colony || !canAfford(me.resources, state.settings.colonyCost) ||
      !byASettleablePlanet(state, mine->at))
    return false;
  pay(me.resources, state.settings.colonyCost);
  mine->colony = true;
  updateDominion(state);
  return true;
}

bool applyOnCorners(State &state, const Action &action) {
  switch (action.kind) {
  case Kind::BuildShip:
  case Kind::PlayShipyard:
    return buildShipsAtColony(state, action);
  case Kind::Move:
    return moveAlongEdges(state, action);
  case Kind::Settle:
    return settleOnCorner(state, action);
  case Kind::BuildColony:
    return buildColonyOnCorner(state, action);
  default:
    return false;
  }
}

// (corners) The main part of a turn's moves for ships and stations: a ship
// at each colony, each ship's moves along edges and settling, each station's
// upgrade to a colony
void cornerActions(const State &state, std::vector<Action> &out) {
  using Kind = Action::Kind;
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  const Settings &settings = state.settings;
  if (shipsOf(state, state.turn) < settings.maxShips &&
      canAfford(me.resources, settings.shipCost)) {
    const bool surplus =
        shipsOf(state, state.turn) + 2 <= settings.maxShips && canPlayCard(state, kShipyardCard);
    for (std::size_t i = 0; i < state.stations.size(); ++i)
      if (state.stations[i].owner == state.turn && state.stations[i].colony) {
        out.push_back({Kind::BuildShip, static_cast<int>(i)});
        if (surplus)
          out.push_back({Kind::PlayShipyard, static_cast<int>(i)});
      }
  }
  for (std::size_t i = 0; i < state.ships.size(); ++i) {
    const Ship &ship = state.ships[i];
    if (ship.owner != state.turn)
      continue;
    if (!ship.moved) {
      const auto reachable = cornerMovesFor(state, ship);
      std::vector<int> corners;
      corners.reserve(reachable.size());
      for (const CornerReach &reach : reachable)
        corners.push_back(reach.corner);
      std::sort(corners.begin(), corners.end()); // (by number, as the moves are listed)
      for (int corner : corners)
        out.push_back({Kind::Move, static_cast<int>(i), corner});
    }
    // (as the rules check: by a planet or an alien outpost)
    const bool canStand = canStandAt(state, state.turn, ship.corner);
    if (canStand && cornerFree(state, ship.corner) && canAfford(me.resources, settings.stationCost))
      out.push_back({Kind::Settle, static_cast<int>(i)});
  }
  if (canAfford(me.resources, settings.colonyCost)) {
    for (std::size_t i = 0; i < state.stations.size(); ++i)
      if (state.stations[i].owner == state.turn && !state.stations[i].colony &&
          byASettleablePlanet(state, state.stations[i].at))
        out.push_back({Kind::BuildColony, static_cast<int>(i)});
  }
}

// ---- On hexes, with docks

bool buildShipAtDock(State &state, const Action &action) {
  if (action.a < 0 || action.a >= static_cast<int>(state.planets.size()))
    return false;
  Player &me = current(state);
  const Settings &settings = state.settings;
  const Planet &planet = state.planets[static_cast<std::size_t>(action.a)];
  if (!dockedAt(planet, state.turn) || shipsOf(state, state.turn) >= settings.maxShips ||
      !canAfford(me.resources, settings.shipCost))
    return false;
  pay(me.resources, settings.shipCost);
  state.ships.push_back(Ship{state.turn, planet.hex, true}); // it moves from next turn
  return true;
}

// A move onto a face-down planet or zone tile turns it over (the dealer
// draws it)
bool moveAcrossHexes(State &state, const Action &action) {
  if (action.a < 0 || action.a >= static_cast<int>(state.ships.size()))
    return false;
  Ship &ship = state.ships[static_cast<std::size_t>(action.a)];
  if (ship.owner != state.turn || ship.moved)
    return false;
  const Hex to{action.b, action.c};
  const auto moves = movesFor(state, ship);
  const auto found = moves.find(to);
  if (found == moves.end())
    return false;
  if (found->second > state.settings.shipMove)
    --current(state).resources[static_cast<std::size_t>(Resource::Fuel)]; // the boost
  ship.at = to;
  ship.moved = true;
  const int planet = planetAt(state, to);
  if (planet >= 0 && !state.planets[static_cast<std::size_t>(planet)].flipped) {
    state.pendingPlanet = planet;
    state.phase = Phase::PlanetDraw;
  }
  const int site = siteAt(state, to);
  if (site >= 0 && !state.zone[static_cast<std::size_t>(site)].flipped) {
    state.pendingSite = site;
    state.phase = Phase::ZoneDraw;
  }
  return true;
}

bool settleAtDock(State &state, const Action &action) {
  if (action.a < 0 || action.a >= static_cast<int>(state.ships.size()))
    return false;
  Player &me = current(state);
  const Ship ship = state.ships[static_cast<std::size_t>(action.a)];
  const int at = planetAt(state, ship.at);
  if (ship.owner != state.turn || at < 0)
    return false;
  Planet &planet = state.planets[static_cast<std::size_t>(at)];
  const int dock = freeDock(planet);
  if (!settleable(planet) || dockedAt(planet, state.turn) || dock < 0 ||
      !canAfford(me.resources, state.settings.stationCost))
    return false;
  pay(me.resources, state.settings.stationCost);
  state.ships.erase(state.ships.begin() + action.a);
  planet.docks[static_cast<std::size_t>(dock)] = state.turn;
  updateCoreControl(state, state.turn);
  return true;
}

bool buildColonyAtDock(State &state, const Action &action) {
  if (action.a < 0 || action.a >= static_cast<int>(state.planets.size()))
    return false;
  Player &me = current(state);
  Planet &planet = state.planets[static_cast<std::size_t>(action.a)];
  if (planet.homeOf >= 0 || planet.kind == PlanetKind::Home ||
      !canAfford(me.resources, state.settings.colonyCost))
    return false;
  for (int d = 0; d < kDocks; ++d) {
    if (planet.docks[static_cast<std::size_t>(d)] == state.turn &&
        !planet.colony[static_cast<std::size_t>(d)]) {
      pay(me.resources, state.settings.colonyCost);
      planet.colony[static_cast<std::size_t>(d)] = true;
      return true;
    }
  }
  return false;
}

bool applyOnHexes(State &state, const Action &action) {
  switch (action.kind) {
  case Kind::BuildShip:
    return buildShipAtDock(state, action);
  case Kind::Move:
    return moveAcrossHexes(state, action);
  case Kind::Settle:
    return settleAtDock(state, action);
  case Kind::BuildColony:
    return buildColonyAtDock(state, action);
  default:
    return false;
  }
}

// The main part of a turn's moves for ships and docks: a ship at each dock,
// each ship's moves and settling, each dock's upgrade to a colony
void hexActions(const State &state, std::vector<Action> &out) {
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  const Settings &settings = state.settings;
  if (shipsOf(state, state.turn) < settings.maxShips &&
      canAfford(me.resources, settings.shipCost)) {
    for (std::size_t i = 0; i < state.planets.size(); ++i) {
      if (dockedAt(state.planets[i], state.turn))
        out.push_back({Kind::BuildShip, static_cast<int>(i)});
    }
  }
  for (std::size_t i = 0; i < state.ships.size(); ++i) {
    const Ship &ship = state.ships[i];
    if (ship.owner != state.turn)
      continue;
    if (!ship.moved) {
      const auto reachable = movesFor(state, ship);
      std::vector<std::pair<Hex, int>> moves(reachable.begin(), reachable.end());
      std::sort(moves.begin(), moves.end()); // a fixed order, whatever the hash map's
      for (const auto &[to, steps] : moves) {
        (void)steps;
        out.push_back({Kind::Move, static_cast<int>(i), to.q, to.r});
      }
    }
    const int at = planetAt(state, ship.at);
    if (at >= 0) {
      const Planet &planet = state.planets[static_cast<std::size_t>(at)];
      if (settleable(planet) && !dockedAt(planet, state.turn) && freeDock(planet) >= 0 &&
          canAfford(me.resources, settings.stationCost))
        out.push_back({Kind::Settle, static_cast<int>(i)});
    }
  }
  if (canAfford(me.resources, settings.colonyCost)) {
    for (std::size_t i = 0; i < state.planets.size(); ++i) {
      const Planet &planet = state.planets[i];
      if (planet.homeOf >= 0 || planet.kind == PlanetKind::Home)
        continue;
      for (int d = 0; d < kDocks; ++d) {
        if (planet.docks[static_cast<std::size_t>(d)] == state.turn &&
            !planet.colony[static_cast<std::size_t>(d)])
          out.push_back({Kind::BuildColony, static_cast<int>(i)});
      }
    }
  }
}

} // namespace

namespace Internal {

[[gnu::hot]] bool byAnOutpost(const State &state, Corner corner) {
  for (Hex hex : HexGrid::hexesTouching(corner))
    if (const int site = siteAt(state, hex); site >= 0) {
      const ZoneSite &find = state.zone[static_cast<std::size_t>(site)];
      if (find.flipped && find.find == Find::AlienStation)
        return true;
    }
  return false;
}

[[gnu::hot]] bool isRelaySpot(const State &state, Corner corner) {
  // (what's on the corner's three hexes, looked up)
  for (Hex hex : HexGrid::hexesTouching(corner)) {
    if (planetAt(state, hex) >= 0)
      return false; // by a planet
    if (const int site = siteAt(state, hex); site >= 0) {
      const ZoneSite &find = state.zone[static_cast<std::size_t>(site)];
      if (!find.flipped || find.find == Find::AlienStation)
        return false; // by a find that isn't allowed
    }
  }
  return true;
}

bool applyBuildOrMove(State &state, const Action &action) {
  return state.settings.corners ? applyOnCorners(state, action) : applyOnHexes(state, action);
}

bool aNewShipMustLeave(const State &state) {
  if (!state.settings.newShipsLeave)
    return false;
  return std::any_of(state.ships.begin(), state.ships.end(), [&](const Ship &ship) {
    return ship.owner == state.turn && !ship.moved && stationAt(state, ship.corner) >= 0 &&
           !cornerMovesFor(state, ship).empty();
  });
}

void buildAndMoveActions(const State &state, std::vector<Action> &out) {
  if (state.settings.corners)
    cornerActions(state, out);
  else
    hexActions(state, out);
}

} // namespace Internal

} // namespace CosmicHex
