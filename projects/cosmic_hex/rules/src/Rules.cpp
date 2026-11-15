// Cosmic Hex's rules: setting up, what each action does, which actions are
// allowed, the dealer's draws, contracts, scoring, and the text the action
// log writes. See Rules.hpp and ../../design.md.
#include <CosmicHex/Rules.hpp>

#include <HexGrid/HexagonMap.hpp>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <deque>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace CosmicHex {

namespace {

constexpr int kBoardRadius = 4;
// The homes: slots on the outer ring, evenly spread (players 0 and 1 face
// each other, so a two-player game uses opposite homes)
constexpr std::array<Hex, kMaxPlayers> kHomes = {{{4, -2}, {-4, 2}, {2, 2}, {-2, -2}}};

constexpr std::array<std::string_view, kResources> kResourceNames = {"minerals", "food", "fuel",
                                                                     "water", "crystals"};
constexpr std::array<std::string_view, 6> kKindNames = {"asteroid_belt", "lush_planet", "gas_giant",
                                                        "ice_moon",      "nebula",      "home"};
constexpr std::array<std::string_view, kFinds> kFindNames = {"nothing", "salvage", "ruins",
                                                             "alien_station", "pirate_hideout"};
constexpr std::array<std::string_view, 11> kPhaseNames = {
    "home_income", "roll_draw", "blockade",      "steal_draw", "main", "planet_draw",
    "zone_draw",   "salvage",   "contract_draw", "keep",       "over"};

bool isSlot(Hex hex) { return ((hex.q - hex.r) % 3 + 3) % 3 == 0; }

bool canAfford(const Resources &have, const Resources &cost) {
  for (int i = 0; i < kResources; ++i) {
    if (have[static_cast<std::size_t>(i)] < cost[static_cast<std::size_t>(i)])
      return false;
  }
  return true;
}
void pay(Resources &have, const Resources &cost) {
  for (int i = 0; i < kResources; ++i)
    have[static_cast<std::size_t>(i)] -= cost[static_cast<std::size_t>(i)];
}
int total(const Resources &have) { return std::accumulate(have.begin(), have.end(), 0); }

Player &current(State &state) { return state.player[static_cast<std::size_t>(state.turn)]; }

int planetAt(const State &state, Hex hex) {
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    if (state.planets[i].hex == hex)
      return static_cast<int>(i);
  }
  return -1;
}
int siteAt(const State &state, Hex hex) {
  for (std::size_t i = 0; i < state.zone.size(); ++i) {
    if (state.zone[i].hex == hex)
      return static_cast<int>(i);
  }
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
// A planet a station can be built on: turned over, and not a home
bool settleable(const Planet &planet) {
  return planet.flipped && planet.homeOf < 0 && planet.kind != PlanetKind::Home;
}

int shipsOf(const State &state, int player) {
  return static_cast<int>(std::count_if(state.ships.begin(), state.ships.end(),
                                        [&](const Ship &s) { return s.owner == player; }));
}

int coreStationsOf(const State &state, int player) {
  int count = 0;
  for (const Planet &planet : state.planets)
    count += planet.core && dockedAt(planet, player);
  return count;
}

// Whether the zone's alien station is next to one of the player's stations
// or ships, so they can trade there
bool alienStationNear(const State &state, int player) {
  for (const ZoneSite &site : state.zone) {
    if (!site.flipped || site.find != Find::AlienStation)
      continue;
    for (const Planet &planet : state.planets) {
      if (dockedAt(planet, player) && HexGrid::distance(planet.hex, site.hex) <= 1)
        return true;
    }
    for (const Ship &ship : state.ships) {
      if (ship.owner == player && HexGrid::distance(ship.at, site.hex) <= 1)
        return true;
    }
  }
  return false;
}

int tradeRate(const State &state, int player) {
  return alienStationNear(state, player)
             ? std::min(state.settings.alienRate, state.settings.bankRate)
             : state.settings.bankRate;
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

void produce(State &state, int roll) {
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    const Planet &planet = state.planets[i];
    if (!planet.flipped || planet.number != roll || static_cast<int>(i) == state.blockade)
      continue;
    for (int d = 0; d < kDocks; ++d) {
      const int owner = planet.docks[static_cast<std::size_t>(d)];
      if (owner >= 0)
        state.player[static_cast<std::size_t>(owner)]
            .resources[static_cast<std::size_t>(planet.kind)] +=
            planet.colony[static_cast<std::size_t>(d)] ? state.settings.colonyPays
                                                       : state.settings.stationPays;
    }
  }
}

void updateCoreControl(State &state, int player) {
  const int mine = coreStationsOf(state, player);
  if (mine < state.settings.coreStationsForControl || state.coreHolder == player)
    return;
  if (state.coreHolder < 0 || mine > coreStationsOf(state, state.coreHolder))
    state.coreHolder = player;
}

void earnBadges(State &state, int count) {
  current(state).badges += count;
  current(state).badgesEarned += count;
}

std::vector<int> blockadeVictims(const State &state, int planet) {
  std::vector<int> out;
  if (planet < 0)
    return out;
  for (int d = 0; d < kDocks; ++d) {
    const int owner =
        state.planets[static_cast<std::size_t>(planet)].docks[static_cast<std::size_t>(d)];
    if (owner >= 0 && owner != state.turn &&
        total(state.player[static_cast<std::size_t>(owner)].resources) > 0)
      out.push_back(owner);
  }
  return out;
}

std::vector<int> blockadeTargets(const State &state) {
  std::vector<int> out;
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    if (settleable(state.planets[i]) && static_cast<int>(i) != state.blockade)
      out.push_back(static_cast<int>(i));
  }
  return out;
}

bool removeOne(std::vector<int> &from, int value) {
  const auto at = std::find(from.begin(), from.end(), value);
  if (at == from.end())
    return false;
  from.erase(at);
  return true;
}

void endTurn(State &state) {
  state.turn = (state.turn + 1) % state.players;
  if (state.turn == 0)
    ++state.round;
  for (Ship &ship : state.ships)
    ship.moved = false;
  state.phase = Phase::HomeIncome;
}

// ---- Text helpers

template <std::size_t N>
int indexOf(const std::array<std::string_view, N> &names, std::string_view name) {
  for (std::size_t i = 0; i < N; ++i) {
    if (names[i] == name)
      return static_cast<int>(i);
  }
  return -1;
}

std::optional<int> toInt(std::string_view text) {
  int value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size())
    return std::nullopt;
  return value;
}

std::vector<std::string_view> split(std::string_view text, char by) {
  std::vector<std::string_view> out;
  while (true) {
    const std::size_t at = text.find(by);
    out.push_back(text.substr(0, at));
    if (at == std::string_view::npos)
      return out;
    text.remove_prefix(at + 1);
  }
}

template <typename Container> std::string joined(const Container &values) {
  std::string out;
  for (const auto &value : values) {
    if (!out.empty())
      out += ',';
    out += std::to_string(value);
  }
  return out.empty() ? "-" : out;
}

std::optional<std::vector<int>> intList(std::string_view text) {
  std::vector<int> out;
  if (text == "-")
    return out;
  for (std::string_view part : split(text, ',')) {
    const auto value = toInt(part);
    if (!value)
      return std::nullopt;
    out.push_back(*value);
  }
  return out;
}

std::string hexText(Hex hex) { return std::to_string(hex.q) + "," + std::to_string(hex.r); }

std::string settingsText(const Settings &s) {
  std::ostringstream out;
  out << "settings win=" << s.winPoints << " ships=" << s.maxShips << " move=" << s.shipMove
      << " boost=" << s.boostMove << " core=" << s.coreRadius
      << " control=" << s.coreStationsForControl << "," << s.coreControlPoints
      << " badges=" << s.badgesPerPoint << " ruins=" << s.ruinsBadges
      << " salvage=" << s.salvageResources << " bank=" << s.bankRate << " alien=" << s.alienRate
      << " pays=" << s.stationPays << "," << s.colonyPays << " start=" << s.startResources
      << " bonus=" << joined(s.seatBonus) << " corners=" << (s.zoneCorners ? 1 : 0)
      << " tiles=" << joined(s.planetTiles) << " numbers=" << joined(s.numbers)
      << " finds=" << joined(s.finds) << " ship=" << joined(s.shipCost)
      << " station=" << joined(s.stationCost) << " colony=" << joined(s.colonyCost)
      << " contract=" << joined(s.contractCost) << " corehexes=";
  std::vector<int> core;
  for (Hex hex : s.coreHexes) {
    core.push_back(hex.q);
    core.push_back(hex.r);
  }
  out << joined(core);
  return out.str();
}

std::optional<Settings> readSettings(std::string_view line) {
  if (!line.starts_with("settings "))
    return std::nullopt;
  Settings s;
  const auto five = [](const std::vector<int> &v, Resources &into) {
    if (v.size() != kResources)
      return false;
    std::copy(v.begin(), v.end(), into.begin());
    return true;
  };
  for (std::string_view field : split(line.substr(9), ' ')) {
    const std::size_t eq = field.find('=');
    if (eq == std::string_view::npos)
      return std::nullopt;
    const std::string_view key = field.substr(0, eq);
    const auto values = intList(field.substr(eq + 1));
    if (values && key == "corehexes" && values->size() % 2 == 0) {
      s.coreHexes.clear();
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.coreHexes.push_back(Hex{(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (!values || values->empty())
      return std::nullopt;
    const std::vector<int> &v = *values;
    bool ok = true;
    if (key == "win")
      s.winPoints = v[0];
    else if (key == "ships")
      s.maxShips = v[0];
    else if (key == "move")
      s.shipMove = v[0];
    else if (key == "boost")
      s.boostMove = v[0];
    else if (key == "core")
      s.coreRadius = v[0];
    else if (key == "control" && v.size() == 2) {
      s.coreStationsForControl = v[0];
      s.coreControlPoints = v[1];
    } else if (key == "badges")
      s.badgesPerPoint = v[0];
    else if (key == "ruins")
      s.ruinsBadges = v[0];
    else if (key == "salvage")
      s.salvageResources = v[0];
    else if (key == "bank")
      s.bankRate = v[0];
    else if (key == "alien")
      s.alienRate = v[0];
    else if (key == "pays" && v.size() == 2) {
      s.stationPays = v[0];
      s.colonyPays = v[1];
    } else if (key == "bonus" && v.size() == kMaxPlayers) {
      std::copy(v.begin(), v.end(), s.seatBonus.begin());
    } else if (key == "start")
      s.startResources = v[0];
    else if (key == "corners")
      s.zoneCorners = v[0] != 0;
    else if (key == "tiles")
      ok = five(v, s.planetTiles);
    else if (key == "numbers")
      s.numbers = v;
    else if (key == "finds" && v.size() == kFinds)
      std::copy(v.begin(), v.end(), s.finds.begin());
    else if (key == "ship")
      ok = five(v, s.shipCost);
    else if (key == "station")
      ok = five(v, s.stationCost);
    else if (key == "colony")
      ok = five(v, s.colonyCost);
    else if (key == "contract")
      ok = five(v, s.contractCost);
    else
      ok = false;
    if (!ok)
      return std::nullopt;
  }
  return s;
}

} // namespace

// ---------------------------------------------------------------------------
// Setting up

State newGame(int players, const Settings &settings, int first) {
  assert(players >= 2 && players <= kMaxPlayers);
  State state;
  state.settings = settings;
  state.players = players;
  assert(first >= 0 && first < players);
  state.turn = first;
  for (Hex hex : HexGrid::spiral(Hex{}, kBoardRadius)) {
    const int length = HexGrid::length(hex);
    if (isSlot(hex)) {
      Planet planet;
      planet.hex = hex;
      planet.core = settings.coreHexes.empty()
                        ? length <= settings.coreRadius
                        : std::find(settings.coreHexes.begin(), settings.coreHexes.end(), hex) !=
                              settings.coreHexes.end();
      for (int p = 0; p < kMaxPlayers; ++p) {
        if (kHomes[static_cast<std::size_t>(p)] == hex) {
          planet.flipped = true;
          planet.kind = PlanetKind::Home;
          if (p < players) {
            planet.homeOf = p;
            planet.docks[0] = p;
          }
        }
      }
      state.planets.push_back(planet);
      continue;
    }
    const bool corner = length == kBoardRadius && (hex.q == 0 || hex.r == 0 || hex.s() == 0);
    const bool inZone = settings.zoneCorners ? (length == 2 || corner) : length <= 2;
    if (inZone)
      state.zone.push_back(ZoneSite{hex, false, Find::Nothing});
  }
  // Unused homes (fewer than 4 players) stay empty home planets: nobody's,
  // producing nothing, and not settleable
  for (int p = 0; p < players; ++p) {
    state.ships.push_back(Ship{p, kHomes[static_cast<std::size_t>(p)], false});
    state.player[static_cast<std::size_t>(p)].resources.fill(settings.startResources);
  }
  for (int order = 0; order < players; ++order) {
    Resources &resources =
        state.player[static_cast<std::size_t>((first + order) % players)].resources;
    for (int i = 0; i < settings.seatBonus[static_cast<std::size_t>(order)]; ++i)
      ++resources[static_cast<std::size_t>(i % kResources)];
  }
  state.kindsLeft = settings.planetTiles;
  state.numbersLeft = settings.numbers;
  state.findsLeft = settings.finds;
  state.deck.resize(contracts().size());
  std::iota(state.deck.begin(), state.deck.end(), 0);
  return state;
}

// ---------------------------------------------------------------------------
// Contracts

namespace {
// The kinds of planet a player has stations on (homes not counted)
std::array<int, kResources> stationKinds(const State &state, int player) {
  std::array<int, kResources> count{};
  for (const Planet &planet : state.planets) {
    if (planet.homeOf < 0 && planet.kind != PlanetKind::Home && dockedAt(planet, player))
      ++count[static_cast<std::size_t>(planet.kind)];
  }
  return count;
}
int kindsCovered(const State &state, int player) {
  const auto kinds = stationKinds(state, player);
  return static_cast<int>(std::count_if(kinds.begin(), kinds.end(), [](int n) { return n > 0; }));
}
int colonies(const State &state, int player) {
  int count = 0;
  for (const Planet &planet : state.planets) {
    for (int d = 0; d < kDocks; ++d)
      count += planet.docks[static_cast<std::size_t>(d)] == player &&
               planet.colony[static_cast<std::size_t>(d)];
  }
  return count;
}
template <PlanetKind kKind> bool twoOf(const State &state, int player) {
  return stationKinds(state, player)[static_cast<std::size_t>(kKind)] >= 2;
}
} // namespace

const std::vector<Contract> &contracts() {
  static const std::vector<Contract> kContracts = {
      {"Stations on 3 kinds of planet", 2,
       [](const State &s, int p) { return kindsCovered(s, p) >= 3; }},
      {"Stations on 2 gas giants", 1, twoOf<PlanetKind::GasGiant>},
      {"Two colonies", 2, [](const State &s, int p) { return colonies(s, p) >= 2; }},
      {"A station on a core planet", 1,
       [](const State &s, int p) { return coreStationsOf(s, p) >= 1; }},
      {"5 explorer badges earned", 2,
       [](const State &s, int p) {
         return s.player[static_cast<std::size_t>(p)].badgesEarned >= 5;
       }},
      {"A find kept from the zone", 1,
       [](const State &s, int p) { return s.player[static_cast<std::size_t>(p)].findsKept >= 1; }},
      {"Stations on opposite sides of the board", 3,
       [](const State &s, int p) {
         for (const Planet &a : s.planets) {
           for (const Planet &b : s.planets) {
             if (dockedAt(a, p) && dockedAt(b, p) && HexGrid::distance(a.hex, b.hex) >= 6)
               return true;
           }
         }
         return false;
       }},
      {"Sharing 2 planets with other players", 2,
       [](const State &s, int p) {
         int shared = 0;
         for (const Planet &planet : s.planets)
           shared += dockedAt(planet, p) && planet.docks[0] >= 0 && planet.docks[1] >= 0;
         return shared >= 2;
       }},
      {"A station next to the alien station", 1,
       [](const State &s, int p) {
         for (const ZoneSite &site : s.zone) {
           if (!site.flipped || site.find != Find::AlienStation)
             continue;
           for (const Planet &planet : s.planets) {
             if (dockedAt(planet, p) && HexGrid::distance(planet.hex, site.hex) <= 1)
               return true;
           }
         }
         return false;
       }},
      {"6 resources in hand", 1,
       [](const State &s, int p) {
         return total(s.player[static_cast<std::size_t>(p)].resources) >= 6;
       }},
      {"A colony on a planet numbered 6 or 8", 2,
       [](const State &s, int p) {
         for (const Planet &planet : s.planets) {
           for (int d = 0; d < kDocks; ++d) {
             if (planet.docks[static_cast<std::size_t>(d)] == p &&
                 planet.colony[static_cast<std::size_t>(d)] &&
                 (planet.number == 6 || planet.number == 8))
               return true;
           }
         }
         return false;
       }},
      {"Stations on 2 ice moons", 1, twoOf<PlanetKind::IceMoon>},
      {"Stations on 2 nebulae", 1, twoOf<PlanetKind::Nebula>},
      {"Three ships in play", 1, [](const State &s, int p) { return shipsOf(s, p) >= 3; }},
      {"Stations on all 5 kinds of planet", 3,
       [](const State &s, int p) { return kindsCovered(s, p) >= 5; }},
      {"Stations on 2 asteroid belts", 1, twoOf<PlanetKind::AsteroidBelt>},
      {"Stations on 2 lush planets", 1, twoOf<PlanetKind::LushPlanet>},
  };
  return kContracts;
}

// ---------------------------------------------------------------------------
// Scoring

int stationsOf(const State &state, int player) {
  int count = 0;
  for (const Planet &planet : state.planets)
    count += dockedAt(planet, player);
  return count;
}

int points(const State &state, int player) {
  const Player &p = state.player[static_cast<std::size_t>(player)];
  int total = stationsOf(state, player) + colonies(state, player); // a colony is 2 in all
  if (state.coreHolder == player)
    total += state.settings.coreControlPoints;
  total += p.badgePoints;
  for (int id : p.revealed)
    total += contracts()[static_cast<std::size_t>(id)].points;
  return total;
}

// ---------------------------------------------------------------------------
// The rules

int Game::currentPlayer(const State &state) {
  switch (state.phase) {
  case Phase::RollDraw:
  case Phase::StealDraw:
  case Phase::PlanetDraw:
  case Phase::ZoneDraw:
  case Phase::ContractDraw:
    return GameBots::kDealer;
  default:
    return state.turn;
  }
}

namespace {
bool applyMain(State &state, const Action &action) {
  using Kind = Action::Kind;
  Player &me = current(state);
  const Settings &settings = state.settings;
  switch (action.kind) {
  case Kind::Trade: {
    if (action.a < 0 || action.a >= kResources || action.b < 0 || action.b >= kResources ||
        action.a == action.b)
      return false;
    const int rate = tradeRate(state, state.turn);
    if (me.resources[static_cast<std::size_t>(action.a)] < rate)
      return false;
    me.resources[static_cast<std::size_t>(action.a)] -= rate;
    ++me.resources[static_cast<std::size_t>(action.b)];
    return true;
  }
  case Kind::BuildShip: {
    if (action.a < 0 || action.a >= static_cast<int>(state.planets.size()))
      return false;
    const Planet &planet = state.planets[static_cast<std::size_t>(action.a)];
    if (!dockedAt(planet, state.turn) || shipsOf(state, state.turn) >= settings.maxShips ||
        !canAfford(me.resources, settings.shipCost))
      return false;
    pay(me.resources, settings.shipCost);
    state.ships.push_back(Ship{state.turn, planet.hex, true}); // it moves from next turn
    return true;
  }
  case Kind::Move: {
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
    if (found->second > settings.shipMove)
      --me.resources[static_cast<std::size_t>(Resource::Fuel)]; // the boost
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
  case Kind::Settle: {
    if (action.a < 0 || action.a >= static_cast<int>(state.ships.size()))
      return false;
    const Ship ship = state.ships[static_cast<std::size_t>(action.a)];
    const int at = planetAt(state, ship.at);
    if (ship.owner != state.turn || at < 0)
      return false;
    Planet &planet = state.planets[static_cast<std::size_t>(at)];
    const int dock = freeDock(planet);
    if (!settleable(planet) || dockedAt(planet, state.turn) || dock < 0 ||
        !canAfford(me.resources, settings.stationCost))
      return false;
    pay(me.resources, settings.stationCost);
    state.ships.erase(state.ships.begin() + action.a);
    planet.docks[static_cast<std::size_t>(dock)] = state.turn;
    updateCoreControl(state, state.turn);
    return true;
  }
  case Kind::BuildColony: {
    if (action.a < 0 || action.a >= static_cast<int>(state.planets.size()))
      return false;
    Planet &planet = state.planets[static_cast<std::size_t>(action.a)];
    if (planet.homeOf >= 0 || planet.kind == PlanetKind::Home ||
        !canAfford(me.resources, settings.colonyCost))
      return false;
    for (int d = 0; d < kDocks; ++d) {
      if (planet.docks[static_cast<std::size_t>(d)] == state.turn &&
          !planet.colony[static_cast<std::size_t>(d)]) {
        pay(me.resources, settings.colonyCost);
        planet.colony[static_cast<std::size_t>(d)] = true;
        return true;
      }
    }
    return false;
  }
  case Kind::BuyContract:
    if (state.deck.empty() || !canAfford(me.resources, settings.contractCost))
      return false;
    pay(me.resources, settings.contractCost);
    state.phase = Phase::ContractDraw;
    return true;
  case Kind::RevealContract: {
    const auto held = std::find(me.contracts.begin(), me.contracts.end(), action.a);
    if (held == me.contracts.end() ||
        !contracts()[static_cast<std::size_t>(action.a)].met(state, state.turn))
      return false;
    me.contracts.erase(held);
    me.revealed.push_back(action.a);
    return true;
  }
  case Kind::CashBadges:
    if (me.badges < settings.badgesPerPoint)
      return false;
    me.badges -= settings.badgesPerPoint;
    ++me.badgePoints;
    return true;
  case Kind::EndTurn:
    endTurn(state);
    return true;
  default:
    return false;
  }
}
} // namespace

bool Game::apply(State &state, const Action &action) {
  using Kind = Action::Kind;
  if (state.winner >= 0)
    return false;
  Player &me = current(state);
  bool done = false;
  switch (state.phase) {
  case Phase::HomeIncome:
    if (action.kind == Kind::ChooseHome && action.a >= 0 && action.a < kResources) {
      ++me.resources[static_cast<std::size_t>(action.a)];
      state.phase = Phase::RollDraw;
      done = true;
    }
    break;
  case Phase::RollDraw:
    if (action.kind == Kind::Roll && action.a >= 1 && action.a <= 6 && action.b >= 1 &&
        action.b <= 6) {
      state.lastRoll = action.a + action.b;
      if (state.lastRoll == 7) {
        state.phase = Phase::Blockade;
      } else {
        produce(state, state.lastRoll);
        state.phase = Phase::Main;
      }
      done = true;
    }
    break;
  case Phase::Blockade: {
    if (action.kind != Kind::MoveBlockade)
      break;
    const std::vector<int> targets = blockadeTargets(state);
    if (targets.empty() ? action.a != -1
                        : std::find(targets.begin(), targets.end(), action.a) == targets.end())
      break;
    const std::vector<int> victims = blockadeVictims(state, action.a);
    if (victims.empty() ? action.b != -1
                        : std::find(victims.begin(), victims.end(), action.b) == victims.end())
      break;
    if (action.a >= 0)
      state.blockade = action.a;
    if (action.b >= 0) {
      state.victims = {action.b};
      state.phase = Phase::StealDraw;
    } else {
      state.phase = Phase::Main;
    }
    done = true;
    break;
  }
  case Phase::StealDraw: {
    if (action.kind != Kind::Steal || state.victims.empty() || action.a < 0 ||
        action.a >= kResources)
      break;
    Player &victim = state.player[static_cast<std::size_t>(state.victims.front())];
    if (victim.resources[static_cast<std::size_t>(action.a)] <= 0)
      break;
    --victim.resources[static_cast<std::size_t>(action.a)];
    ++me.resources[static_cast<std::size_t>(action.a)];
    state.victims.erase(state.victims.begin());
    if (state.victims.empty())
      state.phase = Phase::Main;
    done = true;
    break;
  }
  case Phase::Main:
    done = applyMain(state, action);
    break;
  case Phase::PlanetDraw: {
    if (action.kind != Kind::RevealPlanet || action.a < 0 || action.a >= kResources ||
        state.kindsLeft[static_cast<std::size_t>(action.a)] <= 0 ||
        !removeOne(state.numbersLeft, action.b))
      break;
    --state.kindsLeft[static_cast<std::size_t>(action.a)];
    Planet &planet = state.planets[static_cast<std::size_t>(state.pendingPlanet)];
    planet.flipped = true;
    planet.kind = static_cast<PlanetKind>(action.a);
    planet.number = action.b;
    state.pendingPlanet = -1;
    earnBadges(state, 1);
    state.phase = Phase::Main;
    done = true;
    break;
  }
  case Phase::ZoneDraw: {
    if (action.kind != Kind::RevealFind || action.a < 0 || action.a >= kFinds ||
        state.findsLeft[static_cast<std::size_t>(action.a)] <= 0)
      break;
    --state.findsLeft[static_cast<std::size_t>(action.a)];
    ZoneSite &site = state.zone[static_cast<std::size_t>(state.pendingSite)];
    site.flipped = true;
    site.find = static_cast<Find>(action.a);
    state.pendingSite = -1;
    earnBadges(state, 1);
    state.phase = Phase::Main;
    switch (site.find) {
    case Find::Salvage:
      ++me.findsKept;
      state.salvageLeft = state.settings.salvageResources;
      state.phase = Phase::Salvage;
      break;
    case Find::Ruins:
      ++me.findsKept;
      earnBadges(state, state.settings.ruinsBadges);
      break;
    case Find::PirateHideout:
      ++me.findsKept;
      for (int i = 1; i < state.players; ++i) {
        const int other = (state.turn + i) % state.players;
        if (total(state.player[static_cast<std::size_t>(other)].resources) > 0)
          state.victims.push_back(other);
      }
      if (!state.victims.empty())
        state.phase = Phase::StealDraw;
      break;
    default:
      break;
    }
    done = true;
    break;
  }
  case Phase::Salvage:
    if (action.kind == Kind::TakeSalvage && action.a >= 0 && action.a < kResources) {
      ++me.resources[static_cast<std::size_t>(action.a)];
      if (--state.salvageLeft == 0)
        state.phase = Phase::Main;
      done = true;
    }
    break;
  case Phase::ContractDraw: {
    if (action.kind != Kind::DealContracts || action.a == action.b)
      break;
    std::vector<int> deck = state.deck;
    if (!removeOne(deck, action.a))
      break;
    if (action.b < 0 ? !deck.empty() : !removeOne(deck, action.b))
      break; // two must be dealt while two are left
    state.deck = std::move(deck);
    state.offered = {action.a, action.b};
    state.phase = Phase::KeepContract;
    done = true;
    break;
  }
  case Phase::KeepContract:
    if (action.kind == Kind::KeepContract && action.a >= 0 &&
        (action.a == state.offered[0] || action.a == state.offered[1])) {
      me.contracts.push_back(action.a);
      const int other = action.a == state.offered[0] ? state.offered[1] : state.offered[0];
      if (other >= 0)
        state.deck.push_back(other); // back under the deck
      state.offered = {-1, -1};
      state.phase = Phase::Main;
      done = true;
    }
    break;
  case Phase::Over:
    break;
  }
  if (done && state.phase != Phase::HomeIncome &&
      points(state, state.turn) >= state.settings.winPoints) {
    state.winner = state.turn;
    state.phase = Phase::Over;
  }
  return done;
}

std::vector<Action> Game::legalActions(const State &state) {
  using Kind = Action::Kind;
  std::vector<Action> out;
  if (state.winner >= 0)
    return out;
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  const Settings &settings = state.settings;
  switch (state.phase) {
  case Phase::HomeIncome:
  case Phase::Salvage:
    for (int r = 0; r < kResources; ++r)
      out.push_back({state.phase == Phase::HomeIncome ? Kind::ChooseHome : Kind::TakeSalvage, r});
    break;
  case Phase::Blockade: {
    const std::vector<int> targets = blockadeTargets(state);
    if (targets.empty())
      out.push_back({Kind::MoveBlockade, -1, -1});
    for (int planet : targets) {
      const std::vector<int> victims = blockadeVictims(state, planet);
      if (victims.empty())
        out.push_back({Kind::MoveBlockade, planet, -1});
      for (int victim : victims)
        out.push_back({Kind::MoveBlockade, planet, victim});
    }
    break;
  }
  case Phase::KeepContract:
    for (int id : state.offered) {
      if (id >= 0)
        out.push_back({Kind::KeepContract, id});
    }
    break;
  case Phase::Main: {
    out.push_back({Kind::EndTurn});
    const int rate = tradeRate(state, state.turn);
    for (int give = 0; give < kResources; ++give) {
      if (me.resources[static_cast<std::size_t>(give)] < rate)
        continue;
      for (int get = 0; get < kResources; ++get) {
        if (get != give)
          out.push_back({Kind::Trade, give, get});
      }
    }
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
    if (!state.deck.empty() && canAfford(me.resources, settings.contractCost))
      out.push_back({Kind::BuyContract});
    for (int id : me.contracts) {
      if (contracts()[static_cast<std::size_t>(id)].met(state, state.turn))
        out.push_back({Kind::RevealContract, id});
    }
    if (me.badges >= settings.badgesPerPoint)
      out.push_back({Kind::CashBadges});
    break;
  }
  default: // the dealer's turn, or over
    break;
  }
  return out;
}

Action Game::dealerAction(const State &state, SeededRandom::Generator &random) {
  using Kind = Action::Kind;
  // Draws one of several things, each as likely as its count
  const auto weighted = [&](const auto &counts) {
    int sum = 0;
    for (int c : counts)
      sum += c;
    assert(sum > 0);
    int pick = static_cast<int>(random.below(static_cast<std::uint32_t>(sum)));
    for (std::size_t i = 0; i < counts.size(); ++i) {
      if (pick < counts[i])
        return static_cast<int>(i);
      pick -= counts[i];
    }
    return static_cast<int>(counts.size()) - 1;
  };
  switch (state.phase) {
  case Phase::RollDraw:
    return {Kind::Roll, random.between(1, 6), random.between(1, 6)};
  case Phase::StealDraw:
    return {Kind::Steal,
            weighted(state.player[static_cast<std::size_t>(state.victims.front())].resources)};
  case Phase::PlanetDraw: {
    const int kind = weighted(state.kindsLeft);
    const int number =
        state.numbersLeft[random.below(static_cast<std::uint32_t>(state.numbersLeft.size()))];
    return {Kind::RevealPlanet, kind, number};
  }
  case Phase::ZoneDraw:
    return {Kind::RevealFind, weighted(state.findsLeft)};
  case Phase::ContractDraw: {
    std::vector<int> deck = state.deck;
    random.shuffle(deck.begin(), deck.end());
    return {Kind::DealContracts, deck[0], deck.size() > 1 ? deck[1] : -1};
  }
  default:
    assert(false && "not the dealer's turn");
    return {};
  }
}

State Game::determinize(const State &state, int viewer, SeededRandom::Generator &random) {
  // The only hidden things: the contracts in the other players' hands, and
  // so which are left in the deck. Shuffle the ones the viewer can't place
  // and deal the others' hands again from them.
  State guess = state;
  std::vector<int> unseen = guess.deck;
  for (int p = 0; p < guess.players; ++p) {
    if (p != viewer) {
      const auto &held = guess.player[static_cast<std::size_t>(p)].contracts;
      unseen.insert(unseen.end(), held.begin(), held.end());
    }
  }
  random.shuffle(unseen.begin(), unseen.end());
  std::size_t next = 0;
  for (int p = 0; p < guess.players; ++p) {
    if (p == viewer)
      continue;
    for (int &id : guess.player[static_cast<std::size_t>(p)].contracts)
      id = unseen[next++];
  }
  guess.deck.assign(unseen.begin() + static_cast<std::ptrdiff_t>(next), unseen.end());
  return guess;
}

double Game::estimate(const State &state, int player) {
  int best = 0;
  for (int p = 0; p < state.players; ++p) {
    if (p != player)
      best = std::max(best, points(state, p));
  }
  const double lead = points(state, player) - best;
  return std::clamp(0.5 + lead / (2.0 * state.settings.winPoints), 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Text

std::string Game::writeState(const State &state) {
  std::ostringstream out;
  out << "players " << state.players << " turn " << state.turn << " round " << state.round
      << " phase " << kPhaseNames[static_cast<std::size_t>(state.phase)] << "\n";
  out << settingsText(state.settings) << "\n";
  for (const Planet &p : state.planets) {
    out << "planet " << hexText(p.hex) << " "
        << (p.flipped ? kKindNames[static_cast<std::size_t>(p.kind)] : "?") << " " << p.number
        << (p.core ? " core" : "") << " home=" << p.homeOf << " docks=" << p.docks[0] << ","
        << p.docks[1] << " colony=" << p.colony[0] << "," << p.colony[1] << "\n";
  }
  for (const ZoneSite &z : state.zone)
    out << "zone " << hexText(z.hex) << " "
        << (z.flipped ? kFindNames[static_cast<std::size_t>(z.find)] : "?") << "\n";
  for (const Ship &s : state.ships)
    out << "ship " << s.owner << " " << hexText(s.at) << (s.moved ? " moved" : "") << "\n";
  for (int i = 0; i < state.players; ++i) {
    const Player &p = state.player[static_cast<std::size_t>(i)];
    out << "player " << i << " resources=" << joined(p.resources) << " badges=" << p.badges << ","
        << p.badgesEarned << "," << p.badgePoints << " finds=" << p.findsKept
        << " contracts=" << joined(p.contracts) << " revealed=" << joined(p.revealed) << "\n";
  }
  out << "left kinds=" << joined(state.kindsLeft) << " numbers=" << joined(state.numbersLeft)
      << " finds=" << joined(state.findsLeft) << " deck=" << joined(state.deck) << "\n";
  out << "blockade=" << state.blockade << " core=" << state.coreHolder << " winner=" << state.winner
      << " roll=" << state.lastRoll << " pending=" << state.pendingPlanet << ","
      << state.pendingSite << " victims=" << joined(state.victims)
      << " salvage=" << state.salvageLeft << " offered=" << state.offered[0] << ","
      << state.offered[1] << "\n";
  return out.str();
}

std::optional<State> Game::readState(std::string_view text) {
  // A starting state is newGame's, so it's rebuilt from its player count and
  // settings, and must then write out exactly as the text says
  const std::size_t firstEnd = text.find('\n');
  if (firstEnd == std::string_view::npos)
    return std::nullopt;
  const std::vector<std::string_view> first = split(text.substr(0, firstEnd), ' ');
  if (first.size() < 2 || first[0] != "players")
    return std::nullopt;
  const auto players = toInt(first[1]);
  const auto turn =
      first.size() >= 4 && first[2] == "turn" ? toInt(first[3]) : std::optional<int>(0);
  const std::size_t secondEnd = text.find('\n', firstEnd + 1);
  if (!players || *players < 2 || *players > kMaxPlayers || !turn || *turn < 0 ||
      *turn >= *players || secondEnd == std::string_view::npos)
    return std::nullopt;
  const auto settings = readSettings(text.substr(firstEnd + 1, secondEnd - firstEnd - 1));
  if (!settings)
    return std::nullopt;
  State state = newGame(*players, *settings, *turn);
  if (writeState(state) != text)
    return std::nullopt;
  return state;
}

std::string Game::writeAction(const Action &action) {
  using Kind = Action::Kind;
  const auto res = [](int r) { return std::string(kResourceNames[static_cast<std::size_t>(r)]); };
  const auto n = [](int v) { return std::to_string(v); };
  switch (action.kind) {
  case Kind::ChooseHome:
    return "home " + res(action.a);
  case Kind::Roll:
    return "roll " + n(action.a) + " " + n(action.b);
  case Kind::MoveBlockade:
    return "blockade " + n(action.a) + " " + n(action.b);
  case Kind::Steal:
    return "steal " + res(action.a);
  case Kind::Trade:
    return "trade " + res(action.a) + " " + res(action.b);
  case Kind::BuildShip:
    return "ship " + n(action.a);
  case Kind::Move:
    return "move " + n(action.a) + " " + n(action.b) + "," + n(action.c);
  case Kind::RevealPlanet:
    return "planet " + std::string(kKindNames[static_cast<std::size_t>(action.a)]) + " " +
           n(action.b);
  case Kind::RevealFind:
    return "find " + std::string(kFindNames[static_cast<std::size_t>(action.a)]);
  case Kind::TakeSalvage:
    return "salvage " + res(action.a);
  case Kind::Settle:
    return "settle " + n(action.a);
  case Kind::BuildColony:
    return "colony " + n(action.a);
  case Kind::BuyContract:
    return "buy";
  case Kind::DealContracts:
    return "deal " + n(action.a) + " " + n(action.b);
  case Kind::KeepContract:
    return "keep " + n(action.a);
  case Kind::RevealContract:
    return "reveal " + n(action.a);
  case Kind::CashBadges:
    return "badges";
  case Kind::EndTurn:
    return "end";
  }
  return "?";
}

std::optional<Action> Game::readAction(std::string_view text) {
  using Kind = Action::Kind;
  const std::vector<std::string_view> w = split(text, ' ');
  const auto res = [](std::string_view name) { return indexOf(kResourceNames, name); };
  const auto num = [](std::string_view v) { return toInt(v); };
  const auto make = [](Kind kind, int a = 0, int b = 0, int c = 0) {
    return std::optional<Action>(Action{kind, a, b, c});
  };
  const std::string_view verb = w[0];
  if (w.size() == 1) {
    if (verb == "buy")
      return make(Kind::BuyContract);
    if (verb == "badges")
      return make(Kind::CashBadges);
    if (verb == "end")
      return make(Kind::EndTurn);
    return std::nullopt;
  }
  if (w.size() == 2) {
    const int r = res(w[1]);
    if (verb == "home" && r >= 0)
      return make(Kind::ChooseHome, r);
    if (verb == "steal" && r >= 0)
      return make(Kind::Steal, r);
    if (verb == "salvage" && r >= 0)
      return make(Kind::TakeSalvage, r);
    if (verb == "find") {
      const int f = indexOf(kFindNames, w[1]);
      return f >= 0 ? make(Kind::RevealFind, f) : std::nullopt;
    }
    const auto v = num(w[1]);
    if (!v)
      return std::nullopt;
    if (verb == "ship")
      return make(Kind::BuildShip, *v);
    if (verb == "settle")
      return make(Kind::Settle, *v);
    if (verb == "colony")
      return make(Kind::BuildColony, *v);
    if (verb == "keep")
      return make(Kind::KeepContract, *v);
    if (verb == "reveal")
      return make(Kind::RevealContract, *v);
    return std::nullopt;
  }
  if (w.size() == 3) {
    if (verb == "trade") {
      const int give = res(w[1]), get = res(w[2]);
      return give >= 0 && get >= 0 ? make(Kind::Trade, give, get) : std::nullopt;
    }
    if (verb == "planet") {
      const int kind = indexOf(kKindNames, w[1]);
      const auto number = num(w[2]);
      return kind >= 0 && kind < kResources && number ? make(Kind::RevealPlanet, kind, *number)
                                                      : std::nullopt;
    }
    if (verb == "move") {
      const auto ship = num(w[1]);
      const std::vector<std::string_view> qr = split(w[2], ',');
      if (!ship || qr.size() != 2)
        return std::nullopt;
      const auto q = num(qr[0]), r = num(qr[1]);
      return q && r ? make(Kind::Move, *ship, *q, *r) : std::nullopt;
    }
    const auto a = num(w[1]), b = num(w[2]);
    if (!a || !b)
      return std::nullopt;
    if (verb == "roll")
      return make(Kind::Roll, *a, *b);
    if (verb == "blockade")
      return make(Kind::MoveBlockade, *a, *b);
    if (verb == "deal")
      return make(Kind::DealContracts, *a, *b);
  }
  return std::nullopt;
}

} // namespace CosmicHex
