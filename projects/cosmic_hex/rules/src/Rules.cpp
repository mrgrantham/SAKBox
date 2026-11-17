// Cosmic Hex's rules: setting up, what each action does, which actions are
// allowed, the dealer's draws, contracts, scoring, and the text the action
// log writes. See Rules.hpp and ../../design.md.
#include <CosmicHex/Rules.hpp>

#include <HexGrid/Paths.hpp>

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
constexpr std::array<std::string_view, 7> kKindNames = {
    "asteroid_belt", "lush_planet", "gas_giant", "ice_moon", "nebula", "home", "moon"};
constexpr std::array<std::string_view, kFinds> kFindNames = {"nothing", "salvage", "ruins",
                                                             "alien_station", "pirate_hideout"};
// A phase's name in the state's text (a switch, so a new phase without a
// name is a compiler warning rather than a read past a table's end)
std::string_view phaseName(Phase phase) {
  switch (phase) {
  case Phase::HomeIncome:
    return "home_income";
  case Phase::HomeDieDraw:
    return "home_die_draw";
  case Phase::RollDraw:
    return "roll_draw";
  case Phase::Blockade:
    return "blockade";
  case Phase::StealDraw:
    return "steal_draw";
  case Phase::Main:
    return "main";
  case Phase::PlanetDraw:
    return "planet_draw";
  case Phase::ZoneDraw:
    return "zone_draw";
  case Phase::Salvage:
    return "salvage";
  case Phase::ContractDraw:
    return "contract_draw";
  case Phase::KeepContract:
    return "keep";
  case Phase::Over:
    return "over";
  case Phase::PlaceColony:
    return "place_colony";
  case Phase::PlaceShip:
    return "place_ship";
  case Phase::HomeNumberDraw:
    return "home_number_draw";
  case Phase::Trading:
    return "trading";
  case Phase::TradeReply:
    return "trade_reply";
  case Phase::TradeCounter:
    return "trade_counter";
  }
  return "?";
}

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

// ---- Corners and edges (Settings::corners)

bool onBoard(Corner corner) {
  for (Hex hex : HexGrid::hexesTouching(corner))
    if (HexGrid::length(hex) <= kBoardRadius)
      return true;
  return false;
}

bool touches(Corner corner, Hex hex) {
  const auto touching = HexGrid::hexesTouching(corner);
  return std::find(touching.begin(), touching.end(), hex) != touching.end();
}

// The station on `corner`, or -1
int stationAt(const State &state, Corner corner) {
  for (std::size_t i = 0; i < state.stations.size(); ++i)
    if (state.stations[i].at == corner)
      return static_cast<int>(i);
  return -1;
}

// The distance rule: no station on the corner, nor on any corner an edge away
bool cornerFree(const State &state, Corner corner) {
  if (stationAt(state, corner) >= 0)
    return false;
  for (Corner next : HexGrid::neighbours(corner))
    if (stationAt(state, next) >= 0)
      return false;
  return true;
}

// Every corner within `steps` edges of `from`, with the fewest edges to each
std::unordered_map<int, int> cornersWithin(Corner from, int steps) {
  std::unordered_map<int, int> out;
  for (const HexGrid::Reach<Corner> &reach :
       HexGrid::reachable(from, steps, [](Corner c) { return onBoard(c); })) {
    if (reach.place != from)
      out.emplace(cornerNumber(reach.place), reach.steps);
  }
  return out;
}

// Where a ship can move this turn (corner numbers, with the edges to each):
// shipMove edges, and boostMove more for 1 fuel. Ships pass anything; only
// where a move ends matters (discovery).
std::unordered_map<int, int> cornerMovesFor(const State &state, const Ship &ship) {
  const Settings &settings = state.settings;
  const bool canBoost = state.player[static_cast<std::size_t>(ship.owner)]
                            .resources[static_cast<std::size_t>(Resource::Fuel)] >= 1;
  return cornersWithin(ship.corner, settings.shipMove + (canBoost ? settings.boostMove : 0));
}

// ---- The piles of planets' kinds and numbers (Settings::fitPiles)

// The numbers for `count` planets in the genre's mix: each of 3 to 11 (not
// 7) twice as often as 2 and 12 (Catan's 18: one each of 2 and 12, two of
// the rest), shared out by largest remainder, the middle numbers first on
// a tie
std::vector<int> numbersFor(int count) {
  constexpr std::array<int, 10> kOrder{6, 8, 5, 9, 4, 10, 3, 11, 2, 12}; // (ties: earlier first)
  const auto weight = [](int number) { return number == 2 || number == 12 ? 1 : 2; };
  constexpr int kTotalWeight = 18;
  std::array<int, 10> counts{};
  std::array<double, 10> remainders{};
  int dealt = 0;
  for (std::size_t i = 0; i < kOrder.size(); ++i) {
    const double share = static_cast<double>(count) * weight(kOrder[i]) / kTotalWeight;
    counts[i] = static_cast<int>(share);
    remainders[i] = share - counts[i];
    dealt += counts[i];
  }
  while (dealt < count) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < kOrder.size(); ++i)
      if (remainders[i] > remainders[best] + 1e-9)
        best = i;
    ++counts[best];
    remainders[best] = -1;
    ++dealt;
  }
  std::vector<int> out;
  for (std::size_t i = 0; i < kOrder.size(); ++i)
    out.insert(out.end(), static_cast<std::size_t>(counts[i]), kOrder[i]);
  std::sort(out.begin(), out.end());
  return out;
}

// The kinds for `count` planets: as even as can be, the extras one each to
// the kinds in order
// The kinds for `count` planets, shared by `weights` (largest remainder,
// earlier kinds first on a tie): even weights share them as evenly as can be
Resources kindsFor(int count, const Resources &weights) {
  const int total = std::accumulate(weights.begin(), weights.end(), 0);
  Resources kinds{};
  if (total <= 0)
    return kinds;
  std::array<double, kResources> remainders{};
  int dealt = 0;
  for (std::size_t i = 0; i < kinds.size(); ++i) {
    const double share = static_cast<double>(count) * weights[i] / total;
    kinds[i] = static_cast<int>(share);
    remainders[i] = share - kinds[i];
    dealt += kinds[i];
  }
  while (dealt < count) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < kinds.size(); ++i)
      if (remainders[i] > remainders[best] + 1e-9)
        best = i;
    ++kinds[best];
    remainders[best] = -1;
    ++dealt;
  }
  return kinds;
}

// Whether a face-up planet next to this one has a 6 or an 8 (the red-number
// rule: Settings::redNumbersApart)
bool redNumberNextTo(const State &state, int planet) {
  const Hex hex = state.planets[static_cast<std::size_t>(planet)].hex;
  for (const Planet &other : state.planets)
    if (other.flipped && HexGrid::distance(other.hex, hex) == 1 &&
        (other.number == 6 || other.number == 8))
      return true;
  return false;
}
bool isRed(int number) { return number == 6 || number == 8; }
bool pileHasOtherThanRed(const State &state) {
  return std::any_of(state.numbersLeft.begin(), state.numbersLeft.end(),
                     [](int n) { return !isRed(n); });
}

// The player's home colony (the first station they placed), or -1
int homeColonyOf(const State &state, int player) {
  for (std::size_t i = 0; i < state.stations.size(); ++i)
    if (state.stations[i].owner == player && state.stations[i].colony)
      return static_cast<int>(i);
  return -1;
}

// The player whose setup placement is next: colonies in turn order from the
// first player, then ships in reverse (snake order)
int setupPlayer(const State &state) {
  const int n = state.players, step = state.setupStep;
  return step < n ? (state.first + step) % n : (state.first + 2 * n - 1 - step) % n;
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
// The players with stations on the planet, each once
std::vector<int> ownersOn(const State &state, const Planet &planet) {
  std::vector<int> out;
  for (int p = 0; p < state.players; ++p)
    if (hasStationOn(state, planet, p))
      out.push_back(p);
  return out;
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

// Whether a turned-over alien outpost (the zone's alien station) has a
// corner here
bool byAnOutpost(const State &state, Corner corner) {
  return std::any_of(state.zone.begin(), state.zone.end(), [&](const ZoneSite &site) {
    return site.flipped && site.find == Find::AlienStation && touches(corner, site.hex);
  });
}

// Whether a planet a station can be built by (turned over, not a home) has
// a corner here
bool byASettleablePlanet(const State &state, Corner corner) {
  return std::any_of(state.planets.begin(), state.planets.end(),
                     [&](const Planet &p) { return settleable(p) && touches(corner, p.hex); });
}

// A Deep-space relay's place: touching no planet (homes included) and no
// alien outpost
bool isRelaySpot(const State &state, Corner corner) {
  const bool byAnyPlanet = std::any_of(state.planets.begin(), state.planets.end(),
                                       [&](const Planet &p) { return touches(corner, p.hex); });
  return !byAnyPlanet && !byAnOutpost(state, corner);
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
bool canStandAt(const State &state, int player, Corner corner) {
  return byASettleablePlanet(state, corner) || byAnOutpost(state, corner) ||
         (isRelaySpot(state, corner) && mayBuildRelay(state, player));
}

// (corners) The best alien rate the player has: alienStationRate with a
// station by an outpost, alienRate with a ship, or 0 with neither
int alienRateOf(const State &state, int player) {
  int best = 0;
  for (const Station &station : state.stations)
    if (station.owner == player && byAnOutpost(state, station.at))
      best = state.settings.alienStationRate;
  if (best == 0)
    for (const Ship &ship : state.ships)
      if (ship.owner == player && byAnOutpost(state, ship.corner))
        best = state.settings.alienRate;
  return best;
}

// Whether the zone's alien station is next to one of the player's stations
// or ships, so they can trade there
bool alienStationNear(const State &state, int player) {
  for (const ZoneSite &site : state.zone) {
    if (!site.flipped || site.find != Find::AlienStation)
      continue;
    if (state.settings.corners) {
      // a station or ship on one of its corners
      for (const Station &station : state.stations)
        if (station.owner == player && touches(station.at, site.hex))
          return true;
      for (const Ship &ship : state.ships)
        if (ship.owner == player && touches(ship.corner, site.hex))
          return true;
      continue;
    }
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
  if (state.settings.corners) {
    const int alien = alienRateOf(state, player);
    return alien > 0 ? std::min(alien, state.settings.bankRate) : state.settings.bankRate;
  }
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
  if (state.settings.corners) {
    // Each station from every planet it touches
    for (const Station &station : state.stations) {
      for (Hex hex : HexGrid::hexesTouching(station.at)) {
        const int at = planetAt(state, hex);
        if (at < 0 || at == state.blockade)
          continue;
        const Planet &planet = state.planets[static_cast<std::size_t>(at)];
        if (!planet.flipped || !givesResources(planet.kind) || planet.number != roll)
          continue;
        state.player[static_cast<std::size_t>(station.owner)]
            .resources[static_cast<std::size_t>(planet.kind)] +=
            station.colony ? state.settings.colonyPays : state.settings.stationPays;
      }
    }
    return;
  }
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    const Planet &planet = state.planets[i];
    if (!planet.flipped || !givesResources(planet.kind) || planet.number != roll ||
        static_cast<int>(i) == state.blockade)
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

// The hand limit (Settings::discardOver): on a 7, each player holding more
// than the limit gives back half, one at a time from whichever resource they
// hold most of (the lowest-numbered of a tie)
void discardOverLimit(State &state) {
  const int limit = state.settings.discardOver;
  if (limit <= 0)
    return;
  for (Player &player : state.player) {
    const int held = total(player.resources);
    if (held <= limit)
      continue;
    for (int give = held / 2; give > 0; --give) {
      const auto most = std::max_element(player.resources.begin(), player.resources.end());
      --*most;
    }
  }
}

std::vector<int> blockadeVictims(const State &state, int planet) {
  std::vector<int> out;
  if (planet < 0)
    return out;
  if (state.settings.corners) {
    for (int owner : ownersOn(state, state.planets[static_cast<std::size_t>(planet)]))
      if (owner != state.turn && total(state.player[static_cast<std::size_t>(owner)].resources) > 0)
        out.push_back(owner);
    return out;
  }
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

// How many resources a player takes from their home at the start of their
// turn: 1, and 1 more if they're behind (Settings::catchUpBelow, catchUpGap)
int homeIncomeFor(const State &state, int player);

// The home's income starts (or goes on): rolled on the home die, or chosen;
// none left, the roll
void nextHomeIncome(State &state) {
  state.phase = state.homeIncomeLeft <= 0 ? Phase::RollDraw
                : state.settings.homeDie  ? Phase::HomeDieDraw
                                          : Phase::HomeIncome;
}

int homeIncomeFor(const State &state, int player) {
  const Settings &settings = state.settings;
  // An exhausted home gives nothing of its own (it pays by the dice)
  const bool hasHome = std::any_of(state.planets.begin(), state.planets.end(),
                                   [&](const Planet &p) { return p.homeOf == player; });
  if (!hasHome)
    return 0;
  const int mine = points(state, player);
  if (!settings.homeIncomeSteps.empty()) {
    int income = 1;
    for (const auto &[from, amount] : settings.homeIncomeSteps)
      if (mine >= from)
        income = amount;
    return income;
  }
  int leader = 0;
  for (int p = 0; p < state.players; ++p) {
    if (p != player)
      leader = std::max(leader, points(state, p));
  }
  const bool behind = (settings.catchUpBelow > 0 && mine < settings.catchUpBelow) ||
                      (settings.catchUpGap > 0 && leader - mine >= settings.catchUpGap);
  return behind ? 2 : 1;
}

// ---- Trading between players (Settings::playerTrading)

// After the roll's dealt with: trading first, if it's on and not done
void afterTheRoll(State &state) {
  state.phase = state.settings.playerTrading && !state.tradingDone ? Phase::Trading : Phase::Main;
}

bool holds(const Resources &have, const Resources &needed) {
  for (std::size_t i = 0; i < have.size(); ++i)
    if (have[i] < needed[i])
      return false;
  return true;
}

bool anything(const Resources &resources) {
  return std::any_of(resources.begin(), resources.end(), [](int n) { return n > 0; });
}

// `from` gives `give` to `to`, and gets `get` back
void swapResources(State &state, int from, int to, const Resources &give, const Resources &get) {
  Resources &a = state.player[static_cast<std::size_t>(from)].resources;
  Resources &b = state.player[static_cast<std::size_t>(to)].resources;
  for (std::size_t i = 0; i < a.size(); ++i) {
    a[i] += get[i] - give[i];
    b[i] += give[i] - get[i];
  }
}

// The next player to answer an offer to anyone after `after`, or -1
int nextReplier(const State &state, int after) {
  if (state.offer.to >= 0)
    return -1;
  for (int step = 1; step < state.players; ++step) {
    const int p = (after + step) % state.players;
    if (p == state.turn)
      return -1; // (round to the offerer: everyone's answered)
    return p;
  }
  return -1;
}

// The offer's answered (taken or not): back to trading
void offerDone(State &state) {
  state.offer = TradeOffer{};
  state.phase = Phase::Trading;
}

bool applyTrading(State &state, const Action &action) {
  using Kind = Action::Kind;
  switch (state.phase) {
  case Phase::Trading:
    if (action.kind == Kind::EndTrading) {
      state.tradingDone = true;
      state.phase = Phase::Main;
      return true;
    }
    if (action.kind == Kind::OfferTrade) {
      const Resources give = resourcesOf(action.a), get = resourcesOf(action.b);
      const bool toSomeone = action.c >= 0 && action.c < state.players && action.c != state.turn;
      if (!anything(give) || !anything(get) || (action.c != -1 && !toSomeone) ||
          !holds(state.player[static_cast<std::size_t>(state.turn)].resources, give))
        return false;
      state.offer = TradeOffer{action.a, action.b, action.c, -1, 0, 0};
      state.offer.replier = toSomeone ? action.c : (state.turn + 1) % state.players;
      state.offersMade.push_back({action.a, action.b});
      state.phase = Phase::TradeReply;
      return true;
    }
    return false;
  case Phase::TradeReply: {
    const int replier = state.offer.replier;
    const Resources give = resourcesOf(state.offer.give), get = resourcesOf(state.offer.get);
    if (action.kind == Kind::AcceptTrade) {
      if (!holds(state.player[static_cast<std::size_t>(replier)].resources, get))
        return false;
      swapResources(state, state.turn, replier, give, get);
      offerDone(state);
      return true;
    }
    if (action.kind == Kind::DeclineTrade) {
      const int next = nextReplier(state, replier);
      if (next < 0)
        offerDone(state);
      else
        state.offer.replier = next;
      return true;
    }
    if (action.kind == Kind::CounterTrade) {
      const Resources theyGive = resourcesOf(action.a), theyGet = resourcesOf(action.b);
      if (!anything(theyGive) || !anything(theyGet) ||
          !holds(state.player[static_cast<std::size_t>(replier)].resources, theyGive))
        return false;
      state.offer.counterGive = action.a;
      state.offer.counterGet = action.b;
      state.phase = Phase::TradeCounter;
      return true;
    }
    return false;
  }
  case Phase::TradeCounter: {
    const int replier = state.offer.replier;
    if (action.kind == Kind::AcceptTrade) {
      const Resources theyGive = resourcesOf(state.offer.counterGive),
                      theyGet = resourcesOf(state.offer.counterGet);
      if (!holds(state.player[static_cast<std::size_t>(state.turn)].resources, theyGet))
        return false;
      swapResources(state, state.turn, replier, theyGet, theyGive);
      offerDone(state);
      return true;
    }
    if (action.kind == Kind::DeclineTrade) {
      // (on to the next answerer, as a declined offer)
      state.offer.counterGive = state.offer.counterGet = 0;
      const int next = nextReplier(state, replier);
      if (next < 0) {
        offerDone(state);
      } else {
        state.offer.replier = next;
        state.phase = Phase::TradeReply;
      }
      return true;
    }
    return false;
  }
  default:
    return false;
  }
}

// Whether the player whose turn it is may play an action card: one card a
// turn, in the main part, and not one bought this turn
bool canPlayCard(const State &state, int card) {
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  const auto held = std::count(me.contracts.begin(), me.contracts.end(), card);
  const auto bought = std::count(me.boughtThisTurn.begin(), me.boughtThisTurn.end(), card);
  return state.phase == Phase::Main && !state.cardPlayed && held > bought;
}

// The card's gone from the hand, and the turn's card is played
void playCard(State &state, int card) {
  Player &me = state.player[static_cast<std::size_t>(state.turn)];
  me.contracts.erase(std::find(me.contracts.begin(), me.contracts.end(), card));
  state.cardPlayed = true;
}

// The Galactic Embargo: to whoever has played the most Enforcers, at least
// embargoEnforcers; a tie keeps it with its holder
void updateEmbargo(State &state, int player) {
  const auto played = [&](int p) {
    return state.player[static_cast<std::size_t>(p)].enforcersPlayed;
  };
  if (played(player) < state.settings.embargoEnforcers || state.embargoHolder == player)
    return;
  if (state.embargoHolder < 0 || played(player) > played(state.embargoHolder))
    state.embargoHolder = player;
}

void endTurn(State &state) {
  state.player[static_cast<std::size_t>(state.turn)].boughtThisTurn.clear();
  state.cardPlayed = false;
  state.tradingDone = false;
  state.offersMade.clear();
  state.turn = (state.turn + 1) % state.players;
  if (state.turn == 0)
    ++state.round;
  // (finishRound) the round played out: the most points wins, a tie going to
  // the later in turn order
  if (state.finalRound && state.turn == state.first) {
    int best = -1;
    for (int step = 0; step < state.players; ++step) {
      const int p = (state.first + step) % state.players;
      if (best < 0 || points(state, p) >= points(state, best))
        best = p;
    }
    state.winner = best;
    state.phase = Phase::Over;
    return;
  }
  for (Ship &ship : state.ships)
    ship.moved = false;
  state.homeIncomeLeft = homeIncomeFor(state, state.turn);
  nextHomeIncome(state);
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

// The home's income steps as one list (from, amount, from, amount...), and back
std::vector<int> flatSteps(const std::vector<std::array<int, 2>> &steps) {
  std::vector<int> out;
  for (const auto &[from, amount] : steps) {
    out.push_back(from);
    out.push_back(amount);
  }
  return out;
}

std::string settingsText(const Settings &s) {
  std::ostringstream out;
  out << "settings win=" << s.winPoints << " ships=" << s.maxShips << " move=" << s.shipMove
      << " boost=" << s.boostMove << " core=" << s.coreRadius
      << " control=" << s.coreStationsForControl << "," << s.coreControlPoints
      << " badges=" << s.badgesPerPoint << " ruins=" << s.ruinsBadges
      << " salvage=" << s.salvageResources << " bank=" << s.bankRate << " alien=" << s.alienRate
      << "," << s.alienStationRate << " pays=" << s.stationPays << "," << s.colonyPays
      << " start=" << s.startResources << " bonus=" << joined(s.seatBonus)
      << " corners=" << (s.zoneCorners ? 1 : 0) << " tiles=" << joined(s.planetTiles)
      << " numbers=" << joined(s.numbers) << " finds=" << joined(s.finds)
      << " ship=" << joined(s.shipCost) << " station=" << joined(s.stationCost)
      << " kinds=" << joined(s.kindWeights) << " exhaust=" << s.exhaustHomesAt << ","
      << (s.exhaustedKindRandom ? 1 : 0) << " colony=" << joined(s.colonyCost)
      << " contract=" << joined(s.contractCost) << " corehexes=";
  std::vector<int> core;
  for (Hex hex : s.coreHexes) {
    core.push_back(hex.q);
    core.push_back(hex.r);
  }
  out << joined(core) << " homes=";
  std::vector<int> homeList;
  for (Hex hex : s.homeHexes) {
    homeList.push_back(hex.q);
    homeList.push_back(hex.r);
  }
  out << joined(homeList) << " extratiles=" << joined(s.extraTiles)
      << " extranumbers=" << joined(s.extraNumbers) << " catchup=" << s.catchUpBelow << ","
      << s.catchUpGap << " homesteps=" << joined(flatSteps(s.homeIncomeSteps))
      << " skipfirst=" << (s.firstSkipsIncome ? 1 : 0) << " discardover=" << s.discardOver
      << " charters=" << joined(s.charters) << " enforcers=" << s.enforcers << ","
      << s.embargoEnforcers << "," << s.embargoPoints << " cartels=" << s.cartels << ","
      << s.cartelTake << " finishround=" << (s.finishRound ? 1 : 0)
      << " homedie=" << (s.homeDie ? 1 : 0) << " shipyards=" << s.shipyards
      << " supply=" << s.supplyDrops << " moons=" << s.moons << " planets=";
  std::vector<int> planetList;
  for (Hex hex : s.planetHexes) {
    planetList.push_back(hex.q);
    planetList.push_back(hex.r);
  }
  out << joined(planetList) << " zone=";
  std::vector<int> zoneList;
  for (Hex hex : s.zoneHexes) {
    zoneList.push_back(hex.q);
    zoneList.push_back(hex.r);
  }
  out << joined(zoneList) << " edges=" << (s.corners ? 1 : 0) << "," << s.startShipReach << ","
      << (s.deepSpaceStart ? 1 : 0) << " piles=" << (s.fitPiles ? 1 : 0) << ","
      << (s.redNumbersApart ? 1 : 0);
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
    if (values && (key == "extratiles" || key == "extranumbers")) {
      (key == "extratiles" ? s.extraTiles : s.extraNumbers) = *values;
      continue;
    }
    if (values && key == "homesteps" && values->size() % 2 == 0) {
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.homeIncomeSteps.push_back({(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (values && key == "catchup" && values->size() == 2) {
      s.catchUpBelow = (*values)[0];
      s.catchUpGap = (*values)[1];
      continue;
    }
    if (values && key == "skipfirst" && values->size() == 1) {
      s.firstSkipsIncome = (*values)[0] != 0;
      continue;
    }
    if (values && key == "charters") {
      s.charters = *values;
      continue;
    }
    if (values && key == "enforcers" && values->size() == 3) {
      s.enforcers = (*values)[0];
      s.embargoEnforcers = (*values)[1];
      s.embargoPoints = (*values)[2];
      continue;
    }
    if (values && key == "cartels" && values->size() == 2) {
      s.cartels = (*values)[0];
      s.cartelTake = (*values)[1];
      continue;
    }
    if (values && key == "finishround" && values->size() == 1) {
      s.finishRound = (*values)[0] != 0;
      continue;
    }
    if (values && key == "homedie" && values->size() == 1) {
      s.homeDie = (*values)[0] != 0;
      continue;
    }
    if (values && key == "shipyards" && values->size() == 1) {
      s.shipyards = (*values)[0];
      continue;
    }
    if (values && key == "supply" && values->size() == 1) {
      s.supplyDrops = (*values)[0];
      continue;
    }
    if (values && key == "discardover" && values->size() == 1) {
      s.discardOver = (*values)[0];
      continue;
    }
    if (values && key == "homes" && values->size() % 2 == 0) {
      s.homeHexes.clear();
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.homeHexes.push_back(Hex{(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (values && key == "planets" && values->size() % 2 == 0) {
      s.planetHexes.clear();
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.planetHexes.push_back(Hex{(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (values && key == "piles" && values->size() == 2) {
      s.fitPiles = (*values)[0] != 0;
      s.redNumbersApart = (*values)[1] != 0;
      continue;
    }
    if (values && key == "edges" && values->size() == 3) {
      s.corners = (*values)[0] != 0;
      s.startShipReach = (*values)[1];
      s.deepSpaceStart = (*values)[2] != 0;
      continue;
    }
    if (values && key == "zone" && values->size() % 2 == 0) {
      s.zoneHexes.clear();
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.zoneHexes.push_back(Hex{(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (values && key == "moons" && values->size() == 1) {
      s.moons = (*values)[0];
      continue;
    }
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
    else if (key == "alien") {
      s.alienRate = v[0];
      if (v.size() >= 2)
        s.alienStationRate = v[1];
    } else if (key == "pays" && v.size() == 2) {
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
    else if (key == "kinds")
      ok = five(v, s.kindWeights);
    else if (key == "exhaust") {
      s.exhaustHomesAt = v[0];
      s.exhaustedKindRandom = v.size() >= 2 && v[1] != 0;
    } else if (key == "colony")
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

Board boardR3() {
  return {
      .homes = {{4, -2}, {-4, 2}, {2, 2}, {-2, -2}},
      .planets = {{0, 0},
                  {-4, 0},
                  {-4, 4},
                  {-2, 0},
                  {-2, 3},
                  {-2, 4},
                  {-1, -1},
                  {-1, 3},
                  {0, -4},
                  {0, 4},
                  {1, -3},
                  {1, 1},
                  {2, -4},
                  {2, -3},
                  {2, 0},
                  {4, -4},
                  {4, 0}},
      .zone = {{-3, 0}, {3, 0}, {-2, 1}, {2, -1}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}},
  };
}

Settings agreedRules(const Board &board) {
  Settings s;
  s.homeHexes = board.homes;
  s.planetHexes = board.planets;
  s.zoneHexes = board.zone;
  // Corners and edges, ships moving 2 edges and 1 more for a fuel, and the
  // first deep-space mission (rounds 39 to 52)
  s.corners = true;
  s.deepSpaceStart = true;
  s.shipMove = 2;
  s.boostMove = 1;
  // The hybrid home income (rounds 60 to 69): 2 a turn below 5 points, on
  // the home die; at 5 the home flips to a planet
  s.catchUpBelow = 5;
  s.homeDie = true;
  s.exhaustHomesAt = 5;
  s.discardOver = 7; // the Imperial Tithe (round 69)
  // Free stations (round 72): a ship becomes a station at no cost; the ship
  // costs minerals, fuel and water (round 74: costs down, the quickest fair
  // game measured, about 13 rounds)
  s.stationCost = {0, 0, 0, 0, 0};
  s.shipCost = {1, 0, 1, 1, 0};
  s.playerTrading = true; // (round 57)
  return s;
}

State newGame(int players, const Settings &settings, int first) {
  assert(players >= 2 && players <= kMaxPlayers);
  State state;
  state.settings = settings;
  state.players = players;
  // Spread evenly for the number of players, unless the settings name them
  static const std::array<std::vector<Hex>, kMaxPlayers + 1> kEvenHomes = {{
      {},
      {},
      {{4, -2}, {-4, 2}},
      {{4, -2}, {-2, -2}, {-2, 4}},
      {kHomes.begin(), kHomes.end()},
  }};
  const std::vector<Hex> homes = settings.homeHexes.empty()
                                     ? kEvenHomes[static_cast<std::size_t>(players)]
                                     : settings.homeHexes;
  assert(static_cast<int>(homes.size()) >= players);
  assert(first >= 0 && first < players);
  state.turn = first;
  state.first = first;
  // The planets: the layout's, or one every third hex; and the homes
  const auto isPlanet = [&](Hex hex) {
    const auto named = [hex](const std::vector<Hex> &list) {
      return std::find(list.begin(), list.end(), hex) != list.end();
    };
    return named(homes) ||
           (settings.planetHexes.empty() ? isSlot(hex) : named(settings.planetHexes));
  };
  for (Hex hex : HexGrid::spiral(Hex{}, kBoardRadius)) {
    const int length = HexGrid::length(hex);
    if (isPlanet(hex)) {
      Planet planet;
      planet.hex = hex;
      planet.core = settings.coreHexes.empty()
                        ? length <= settings.coreRadius
                        : std::find(settings.coreHexes.begin(), settings.coreHexes.end(), hex) !=
                              settings.coreHexes.end();
      for (int p = 0; p < static_cast<int>(homes.size()); ++p) {
        if (homes[static_cast<std::size_t>(p)] == hex) {
          planet.flipped = true;
          planet.kind = PlanetKind::Home;
          if (p < players) {
            planet.homeOf = p;
            if (!settings.corners)
              planet.docks[0] = p; // (with corners, a home colony is placed at the start)
          }
        }
      }
      state.planets.push_back(planet);
      continue;
    }
    const bool corner = length == kBoardRadius && (hex.q == 0 || hex.r == 0 || hex.s() == 0);
    const bool inZone = !settings.zoneHexes.empty()
                            ? std::find(settings.zoneHexes.begin(), settings.zoneHexes.end(),
                                        hex) != settings.zoneHexes.end()
                            : (settings.zoneCorners ? (length == 2 || corner) : length <= 2);
    if (inZone)
      state.zone.push_back(ZoneSite{hex, false, Find::Nothing});
  }
  // Unused homes (fewer than 4 players) stay empty home planets: nobody's,
  // producing nothing, and not settleable
  for (int p = 0; p < players; ++p) {
    if (!settings.corners) // (with corners, ships are placed at the start)
      state.ships.push_back(Ship{p, homes[static_cast<std::size_t>(p)], false});
    state.player[static_cast<std::size_t>(p)].resources.fill(settings.startResources);
  }
  for (int order = 0; order < players; ++order) {
    Resources &resources =
        state.player[static_cast<std::size_t>((first + order) % players)].resources;
    for (int i = 0; i < settings.seatBonus[static_cast<std::size_t>(order)]; ++i)
      ++resources[static_cast<std::size_t>(i % kResources)];
  }
  state.kindsLeft = settings.planetTiles;
  state.moonsLeft = settings.moons;
  state.numbersLeft = settings.numbers;
  if (settings.fitPiles) {
    // A kind and a number for every face-down planet that isn't a moon
    const int faceDown = static_cast<int>(std::count_if(
        state.planets.begin(), state.planets.end(), [](const Planet &p) { return !p.flipped; }));
    const int resourcePlanets = std::max(0, faceDown - settings.moons);
    state.kindsLeft = kindsFor(resourcePlanets, settings.kindWeights);
    state.numbersLeft = numbersFor(resourcePlanets);
  }
  // Planets beyond the tiles given (fewer homes than four): the extras, in turn
  const int planets = static_cast<int>(std::count_if(state.planets.begin(), state.planets.end(),
                                                     [](const Planet &p) { return !p.flipped; }));
  for (int extra = 0;
       !settings.fitPiles &&
       std::accumulate(state.kindsLeft.begin(), state.kindsLeft.end(), 0) + state.moonsLeft <
           planets;
       ++extra) {
    assert(!settings.extraTiles.empty() && !settings.extraNumbers.empty());
    ++state.kindsLeft[static_cast<std::size_t>(
        settings.extraTiles[static_cast<std::size_t>(extra) % settings.extraTiles.size()])];
    state.numbersLeft.push_back(
        settings.extraNumbers[static_cast<std::size_t>(extra) % settings.extraNumbers.size()]);
  }
  state.homeIncomeLeft = settings.firstSkipsIncome ? 0 : homeIncomeFor(state, first);
  nextHomeIncome(state);
  state.findsLeft = settings.finds;
  // A find for every site in the zone: a layout that leaves more of the
  // usual places empty than there are finds gets "nothing" tiles for the
  // rest, so the special finds stay as they are. (Without, the dealer had
  // nothing to draw for the last sites, and the game never ended.) Fewer
  // sites than finds is as before: some are never drawn.
  const int finds = std::accumulate(state.findsLeft.begin(), state.findsLeft.end(), 0);
  const int sites = static_cast<int>(state.zone.size());
  if (finds < sites)
    state.findsLeft[static_cast<std::size_t>(Find::Nothing)] += sites - finds;
  state.deck = settings.charters;
  state.deck.insert(state.deck.end(), static_cast<std::size_t>(settings.enforcers), kEnforcerCard);
  state.deck.insert(state.deck.end(), static_cast<std::size_t>(settings.cartels), kCartelCard);
  state.deck.insert(state.deck.end(), static_cast<std::size_t>(settings.shipyards), kShipyardCard);
  state.deck.insert(state.deck.end(), static_cast<std::size_t>(settings.supplyDrops),
                    kSupplyDropCard);
  if (settings.corners) {
    // The start: home colonies, then ships, placed in snake order
    state.phase = Phase::PlaceColony;
    state.turn = setupPlayer(state);
  }
  return state;
}

// ---------------------------------------------------------------------------
// Contracts

namespace {
// The kinds of planet a player has stations on (homes not counted)
std::array<int, kResources> stationKinds(const State &state, int player) {
  std::array<int, kResources> count{};
  for (const Planet &planet : state.planets) {
    if (planet.homeOf < 0 && givesResources(planet.kind) && hasStationOn(state, planet, player))
      ++count[static_cast<std::size_t>(planet.kind)];
  }
  return count;
}
int kindsCovered(const State &state, int player) {
  const auto kinds = stationKinds(state, player);
  return static_cast<int>(std::count_if(kinds.begin(), kinds.end(), [](int n) { return n > 0; }));
}
// Colonies built (the home colony left out)
int colonies(const State &state, int player) {
  int count = 0;
  if (state.settings.corners) {
    for (std::size_t i = 0; i < state.stations.size(); ++i)
      count += state.stations[i].owner == player && state.stations[i].colony &&
               static_cast<int>(i) != homeColonyOf(state, player);
    return count;
  }
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

const std::vector<Corner> &boardCorners() {
  static const std::vector<Corner> kCorners =
      HexGrid::everyCorner(HexGrid::spiral(Hex{}, kBoardRadius));
  return kCorners;
}

int cornerNumber(Corner corner) {
  const std::vector<Corner> &all = boardCorners();
  const auto found = std::lower_bound(all.begin(), all.end(), corner);
  return found != all.end() && *found == corner ? static_cast<int>(found - all.begin()) : -1;
}

int bundleOf(const Resources &resources) {
  int bundle = 0;
  for (std::size_t i = 0; i < resources.size(); ++i)
    bundle |= std::clamp(resources[i], 0, 15) << (4 * i);
  return bundle;
}

Resources resourcesOf(int bundle) {
  Resources out{};
  for (std::size_t i = 0; i < out.size(); ++i)
    out[i] = (bundle >> (4 * i)) & 15;
  return out;
}

bool isCharter(int card) { return card >= 0 && card < static_cast<int>(contracts().size()); }

const std::vector<Contract> &contracts() {
  static const std::vector<Contract> kContracts = {
      {"Stations on 3 kinds of planet", 2,
       [](const State &s, int p) { return kindsCovered(s, p) >= 3; }},
      {"Stations on 2 gas giants", 1, twoOf<PlanetKind::GasGiant>},
      {"Two colonies", 2, [](const State &s, int p) { return colonies(s, p) >= 2; }},
      {"A station on a core planet", 1,
       [](const State &s, int p) { return coreStationsOf(s, p) >= 1; }},
      // (5 badges and one find were scored by about 80% of those who kept
      // them, in 2,400 four-player games: nearly free points)
      {"6 explorer badges earned", 2,
       [](const State &s, int p) {
         return s.player[static_cast<std::size_t>(p)].badgesEarned >= 6;
       }},
      {"Two finds kept from the zone", 1,
       [](const State &s, int p) { return s.player[static_cast<std::size_t>(p)].findsKept >= 2; }},
      {"Stations on opposite sides of the board", 3,
       [](const State &s, int p) {
         for (const Planet &a : s.planets) {
           for (const Planet &b : s.planets) {
             if (hasStationOn(s, a, p) && hasStationOn(s, b, p) &&
                 HexGrid::distance(a.hex, b.hex) >= 6)
               return true;
           }
         }
         return false;
       }},
      {"Sharing 2 planets with other players", 2,
       [](const State &s, int p) {
         int shared = 0;
         for (const Planet &planet : s.planets)
           shared += hasStationOn(s, planet, p) && ownersOn(s, planet).size() >= 2;
         return shared >= 2;
       }},
      {"A station next to the alien station", 1,
       [](const State &s, int p) {
         for (const ZoneSite &site : s.zone) {
           if (!site.flipped || site.find != Find::AlienStation)
             continue;
           if (s.settings.corners) {
             for (const Station &station : s.stations)
               if (station.owner == p && touches(station.at, site.hex))
                 return true;
             continue;
           }
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
           if (planet.homeOf < 0 && (planet.number == 6 || planet.number == 8) &&
               hasColonyOn(s, planet, p))
             return true;
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
      // Round 37: the five above merged into one, and 5 kinds made 4
      {"Stations on 2 planets of the same kind", 1,
       [](const State &s, int p) {
         const auto kinds = stationKinds(s, p);
         return std::ranges::any_of(kinds, [](int n) { return n >= 2; });
       }},
      {"Stations on 4 kinds of planet", 3,
       [](const State &s, int p) { return kindsCovered(s, p) >= 4; }},
      // (19) Round 61: a station or colony by another player's exhausted
      // home world (Settings::exhaustHomesAt)
      {"Homesteader: a station on another player's exhausted home world", 2,
       [](const State &s, int p) {
         for (const Planet &planet : s.planets)
           if (planet.formerHomeOf >= 0 && planet.formerHomeOf != p && hasStationOn(s, planet, p))
             return true;
         return false;
       }},
      // (20) Round 63: a station touching no planet and no outpost, which
      // only a holder of this Charter may build (one). Its point is on the
      // station (2, see points()), not the card: easier to keep track of
      {"Deep-space relay: a station touching no planet and no alien outpost", 0,
       [](const State &s, int p) {
         return std::any_of(s.stations.begin(), s.stations.end(), [&](const Station &st) {
           return st.owner == p && isRelaySpot(s, st.at);
         });
       }},
  };
  return kContracts;
}

// ---------------------------------------------------------------------------
// Scoring

int stationsOf(const State &state, int player) {
  int count = 0;
  if (state.settings.corners) {
    for (const Station &station : state.stations)
      count += station.owner == player;
    return count;
  }
  for (const Planet &planet : state.planets)
    count += dockedAt(planet, player);
  return count;
}

int points(const State &state, int player) {
  const Player &p = state.player[static_cast<std::size_t>(player)];
  int total = stationsOf(state, player) + colonies(state, player); // a colony is 2 in all
  if (state.settings.corners && homeColonyOf(state, player) >= 0)
    ++total; // (the home colony: 2, as any colony)
  if (state.coreHolder == player)
    total += state.settings.coreControlPoints;
  if (state.embargoHolder == player)
    total += state.settings.embargoPoints;
  total += p.badgePoints;
  // A Deep-space relay's station: 2 (round 63)
  total += static_cast<int>(
      std::count_if(state.stations.begin(), state.stations.end(), [&](const Station &s) {
        return s.owner == player && state.settings.corners && isRelaySpot(state, s.at);
      }));
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
  case Phase::HomeNumberDraw:
  case Phase::HomeDieDraw:
    return GameBots::kDealer;
  case Phase::TradeReply:
    return state.offer.replier;
  default:
    return state.turn;
  }
}

namespace {

// After a setup placement: the next player's, or the first turn
void nextSetupStep(State &state) {
  ++state.setupStep;
  const int n = state.players;
  if (state.setupStep < 2 * n) {
    state.phase = state.setupStep < n ? Phase::PlaceColony : Phase::PlaceShip;
    state.turn = setupPlayer(state);
    return;
  }
  state.turn = state.first;
  state.homeIncomeLeft = state.settings.firstSkipsIncome ? 0 : homeIncomeFor(state, state.first);
  nextHomeIncome(state);
}

// The corners a player may put their home colony on: corners of their home
// planet, free under the distance rule
std::vector<int> colonyPlaces(const State &state, int player) {
  std::vector<int> out;
  for (const Planet &planet : state.planets) {
    if (planet.homeOf != player)
      continue;
    for (Corner corner : HexGrid::corners(planet.hex))
      if (onBoard(corner) && cornerFree(state, corner))
        out.push_back(cornerNumber(corner));
  }
  std::sort(out.begin(), out.end());
  return out;
}

// The corners a player may put their starting ship on: their home colony's,
// or any within startShipReach edges of it
std::vector<int> shipPlaces(const State &state, int player) {
  if (state.settings.deepSpaceStart) {
    // Any corner touching no planet (homes included), with no ship already
    std::vector<int> out;
    for (Corner corner : boardCorners()) {
      bool byAPlanet = false;
      for (Hex hex : HexGrid::hexesTouching(corner))
        byAPlanet = byAPlanet || planetAt(state, hex) >= 0;
      const bool taken = std::any_of(state.ships.begin(), state.ships.end(),
                                     [&](const Ship &ship) { return ship.corner == corner; });
      if (!byAPlanet && !taken)
        out.push_back(cornerNumber(corner));
    }
    return out;
  }
  const int home = homeColonyOf(state, player);
  if (home < 0)
    return {};
  const Corner at = state.stations[static_cast<std::size_t>(home)].at;
  std::vector<int> out{cornerNumber(at)};
  for (const auto &[corner, steps] : cornersWithin(at, state.settings.startShipReach)) {
    (void)steps;
    out.push_back(corner);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// (corners) Turns over the next planet or zone tile the ship's stop touches,
// if any is left face down: the dealer draws it, and this is called again
// once the turn is back in its main part
void turnNextOver(State &state) {
  while (!state.turning.empty()) {
    const Hex hex = state.turning.front();
    state.turning.erase(state.turning.begin());
    const int planet = planetAt(state, hex);
    if (planet >= 0 && !state.planets[static_cast<std::size_t>(planet)].flipped) {
      state.pendingPlanet = planet;
      state.phase = Phase::PlanetDraw;
      return;
    }
    const int site = siteAt(state, hex);
    if (site >= 0 && !state.zone[static_cast<std::size_t>(site)].flipped) {
      state.pendingSite = site;
      state.phase = Phase::ZoneDraw;
      return;
    }
  }
}

// The main part of a turn with corners: building ships at colonies, moving
// along edges, settling on corners, upgrading stations. Returns whether it
// was done, or nothing if the action isn't one of these (the rest work the
// same with either rules).
std::optional<bool> applyMainOnCorners(State &state, const Action &action) {
  using Kind = Action::Kind;
  Player &me = current(state);
  const Settings &settings = state.settings;
  const auto station = [&](int index) -> Station * {
    if (index < 0 || index >= static_cast<int>(state.stations.size()))
      return nullptr;
    Station &found = state.stations[static_cast<std::size_t>(index)];
    return found.owner == state.turn ? &found : nullptr;
  };
  switch (action.kind) {
  case Kind::BuildShip:
  case Kind::PlayShipyard: {
    // (Shipyard Surplus: a second ship free, so room for two)
    const bool surplus = action.kind == Kind::PlayShipyard;
    const int building = surplus ? 2 : 1;
    const Station *colony = station(action.a);
    if (!colony || !colony->colony || shipsOf(state, state.turn) + building > settings.maxShips ||
        !canAfford(me.resources, settings.shipCost) ||
        (surplus && !canPlayCard(state, kShipyardCard)))
      return false;
    if (surplus)
      playCard(state, kShipyardCard);
    pay(me.resources, settings.shipCost);
    for (int i = 0; i < building; ++i)
      state.ships.push_back(Ship{state.turn, Hex{}, true, colony->at}); // moves from next turn
    return true;
  }
  case Kind::Move: {
    if (action.a < 0 || action.a >= static_cast<int>(state.ships.size()))
      return false;
    Ship &ship = state.ships[static_cast<std::size_t>(action.a)];
    if (ship.owner != state.turn || ship.moved)
      return false;
    const auto moves = cornerMovesFor(state, ship);
    const auto found = moves.find(action.b);
    if (found == moves.end())
      return false;
    if (found->second > settings.shipMove)
      --me.resources[static_cast<std::size_t>(Resource::Fuel)]; // the boost
    ship.corner = boardCorners()[static_cast<std::size_t>(action.b)];
    ship.moved = true;
    // Where it stops, what it touches turns over
    for (Hex hex : HexGrid::hexesTouching(ship.corner))
      state.turning.push_back(hex);
    turnNextOver(state);
    return true;
  }
  case Kind::Settle: {
    if (action.a < 0 || action.a >= static_cast<int>(state.ships.size()))
      return false;
    const Ship ship = state.ships[static_cast<std::size_t>(action.a)];
    if (ship.owner != state.turn || !cornerFree(state, ship.corner) ||
        !canAfford(me.resources, settings.stationCost))
      return false;
    // A station must touch a planet it can be built by (turned over, not a
    // home), or an alien outpost
    if (!canStandAt(state, state.turn, ship.corner))
      return false;
    pay(me.resources, settings.stationCost);
    state.ships.erase(state.ships.begin() + action.a);
    state.stations.push_back(Station{ship.corner, state.turn, false});
    updateCoreControl(state, state.turn);
    return true;
  }
  case Kind::BuildColony: {
    Station *mine = station(action.a);
    // (a station only by an outpost can't become a colony: a colony pays
    // double from its planets, and it has none)
    if (!mine || mine->colony || !canAfford(me.resources, settings.colonyCost) ||
        !byASettleablePlanet(state, mine->at))
      return false;
    pay(me.resources, settings.colonyCost);
    mine->colony = true;
    return true;
  }
  default:
    return std::nullopt;
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
      for (const auto &[corner, steps] : reachable) {
        (void)steps;
        corners.push_back(corner);
      }
      std::sort(corners.begin(), corners.end()); // a fixed order, whatever the hash map's
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

bool applyMain(State &state, const Action &action) {
  using Kind = Action::Kind;
  Player &me = current(state);
  const Settings &settings = state.settings;
  if (settings.corners) {
    if (const std::optional<bool> done = applyMainOnCorners(state, action))
      return *done;
  }
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
    // (a Charter, not one revealed already, met now)
    if (held == me.contracts.end() || !isCharter(action.a) ||
        std::find(me.revealed.begin(), me.revealed.end(), action.a) != me.revealed.end() ||
        !contracts()[static_cast<std::size_t>(action.a)].met(state, state.turn))
      return false;
    me.contracts.erase(held);
    me.revealed.push_back(action.a);
    return true;
  }
  case Kind::PlayEnforcer: {
    if (!canPlayCard(state, kEnforcerCard))
      return false;
    playCard(state, kEnforcerCard);
    ++me.enforcersPlayed;
    updateEmbargo(state, state.turn);
    state.phase = Phase::Blockade; // then as on a 7: move it, and steal
    return true;
  }
  case Kind::PlaySupplyDrop: {
    if (!canPlayCard(state, kSupplyDropCard) || action.a < 0 || action.a >= kResources ||
        action.b < 0 || action.b >= kResources)
      return false;
    playCard(state, kSupplyDropCard);
    ++me.resources[static_cast<std::size_t>(action.a)];
    ++me.resources[static_cast<std::size_t>(action.b)];
    return true;
  }
  case Kind::PlayCartel: {
    if (!canPlayCard(state, kCartelCard) || action.a < 0 || action.a >= kResources)
      return false;
    playCard(state, kCartelCard);
    const auto r = static_cast<std::size_t>(action.a);
    for (int p = 0; p < state.players; ++p) {
      if (p == state.turn)
        continue;
      int &theirs = state.player[static_cast<std::size_t>(p)].resources[r];
      const int taken = settings.cartelTake > 0 ? std::min(theirs, settings.cartelTake) : theirs;
      theirs -= taken;
      me.resources[r] += taken;
    }
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
  case Phase::HomeDieDraw:
  case Phase::HomeIncome:
    // The home's resource: chosen, or rolled on the home die (where its
    // sixth face, "your choice", hands the choice to the player)
    if (state.phase == Phase::HomeDieDraw && action.kind == Kind::RollHomeDie &&
        action.a == kResources) {
      state.phase = Phase::HomeIncome;
      done = true;
      break;
    }
    if ((state.phase == Phase::HomeIncome ? action.kind == Kind::ChooseHome
                                          : action.kind == Kind::RollHomeDie) &&
        action.a >= 0 && action.a < kResources) {
      ++me.resources[static_cast<std::size_t>(action.a)];
      --state.homeIncomeLeft;
      nextHomeIncome(state);
      // The exhausted home: at exhaustHomesAt points, this was its last
      // resource; it becomes a planet of that kind, its number drawn next
      const auto home = std::find_if(state.planets.begin(), state.planets.end(),
                                     [&](const Planet &p) { return p.homeOf == state.turn; });
      if (state.settings.exhaustHomesAt > 0 && home != state.planets.end() &&
          points(state, state.turn) >= state.settings.exhaustHomesAt) {
        home->kind = static_cast<PlanetKind>(action.a);
        home->formerHomeOf = state.turn;
        home->homeOf = -1;
        state.homeIncomeLeft = 0;
        state.pendingPlanet = static_cast<int>(home - state.planets.begin());
        state.phase = Phase::HomeNumberDraw;
      }
      done = true;
    }
    break;
  case Phase::HomeNumberDraw:
    if (action.kind == Kind::NumberHome &&
        std::find(state.settings.exhaustedNumbers.begin(), state.settings.exhaustedNumbers.end(),
                  action.a) != state.settings.exhaustedNumbers.end()) {
      Planet &home = state.planets[static_cast<std::size_t>(state.pendingPlanet)];
      if (state.settings.exhaustedKindRandom) {
        if (action.b < 0 || action.b >= kResources)
          break;
        home.kind = static_cast<PlanetKind>(action.b);
      }
      home.number = action.a;
      state.pendingPlanet = -1;
      state.phase = Phase::RollDraw;
      done = true;
    }
    break;
  case Phase::RollDraw:
    if (action.kind == Kind::Roll && action.a >= 1 && action.a <= 6 && action.b >= 1 &&
        action.b <= 6) {
      state.lastRoll = action.a + action.b;
      if (state.lastRoll == 7) {
        discardOverLimit(state);
        state.phase = Phase::Blockade;
      } else {
        produce(state, state.lastRoll);
        afterTheRoll(state);
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
      afterTheRoll(state); // (on a 7; after an Enforcer, trading's done)
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
      afterTheRoll(state);
    done = true;
    break;
  }
  case Phase::Main:
    done = applyMain(state, action);
    break;
  case Phase::Trading:
  case Phase::TradeReply:
  case Phase::TradeCounter:
    done = applyTrading(state, action);
    break;
  case Phase::PlanetDraw: {
    if (action.kind != Kind::RevealPlanet)
      break;
    if (action.a == static_cast<int>(PlanetKind::Moon)) {
      // A moon: no number
      if (state.moonsLeft <= 0 || action.b != 0)
        break;
      --state.moonsLeft;
    } else {
      // (the red-number rule: no 6 or 8 by a face-up 6 or 8, while there's another)
      if (state.settings.redNumbersApart && isRed(action.b) &&
          redNumberNextTo(state, state.pendingPlanet) && pileHasOtherThanRed(state))
        break;
      if (action.a < 0 || action.a >= kResources ||
          state.kindsLeft[static_cast<std::size_t>(action.a)] <= 0 ||
          !removeOne(state.numbersLeft, action.b))
        break;
      --state.kindsLeft[static_cast<std::size_t>(action.a)];
    }
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
    if (action.kind != Kind::DealContracts) // (two the same only if the deck holds two)
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
      if (!isCharter(action.a))
        me.boughtThisTurn.push_back(action.a); // (an action card: playable from next turn)
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
  case Phase::PlaceColony: {
    const std::vector<int> places = colonyPlaces(state, state.turn);
    if (action.kind != Kind::PlaceColony ||
        std::find(places.begin(), places.end(), action.a) == places.end())
      break;
    state.stations.push_back(
        Station{boardCorners()[static_cast<std::size_t>(action.a)], state.turn, true});
    nextSetupStep(state);
    done = true;
    break;
  }
  case Phase::PlaceShip: {
    const std::vector<int> places = shipPlaces(state, state.turn);
    if (action.kind != Kind::PlaceShip ||
        std::find(places.begin(), places.end(), action.a) == places.end())
      break;
    const Corner at = boardCorners()[static_cast<std::size_t>(action.a)];
    state.ships.push_back(Ship{state.turn, Hex{}, false, at});
    done = true;
    if (state.settings.deepSpaceStart) {
      // The finds it lands by turn over first (the setup goes on after)
      for (Hex hex : HexGrid::hexesTouching(at))
        state.turning.push_back(hex);
      state.placingShip = true;
      turnNextOver(state);
      if (state.phase == Phase::PlaceShip) { // (nothing to turn over)
        state.placingShip = false;
        nextSetupStep(state);
      }
      break;
    }
    nextSetupStep(state);
    break;
  }
  }
  // (corners) more to turn over where a ship stopped
  if (done && state.phase == Phase::Main && !state.turning.empty())
    turnNextOver(state);
  // (corners) a starting ship's finds all turned over: the setup goes on
  if (done && state.placingShip && state.phase == Phase::Main && state.turning.empty()) {
    state.placingShip = false;
    nextSetupStep(state);
  }
  if (done && state.phase != Phase::HomeIncome && state.phase != Phase::HomeDieDraw &&
      points(state, state.turn) >= state.settings.winPoints) {
    if (state.settings.finishRound) {
      state.finalRound = true; // (the game ends when the round does)
    } else {
      state.winner = state.turn;
      state.phase = Phase::Over;
    }
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
  case Phase::PlaceColony:
    for (int corner : colonyPlaces(state, state.turn))
      out.push_back({Kind::PlaceColony, corner});
    break;
  case Phase::PlaceShip:
    for (int corner : shipPlaces(state, state.turn))
      out.push_back({Kind::PlaceShip, corner});
    break;
  // Trading: the search sees only ending it and answering yes or no (the
  // offers and counters, too many to search, come from the trading rule:
  // TradingRule.hpp)
  case Phase::Trading:
    out.push_back({Kind::EndTrading});
    break;
  case Phase::TradeReply:
    if (holds(state.player[static_cast<std::size_t>(state.offer.replier)].resources,
              resourcesOf(state.offer.get)))
      out.push_back({Kind::AcceptTrade});
    out.push_back({Kind::DeclineTrade});
    break;
  case Phase::TradeCounter:
    if (holds(me.resources, resourcesOf(state.offer.counterGet)))
      out.push_back({Kind::AcceptTrade});
    out.push_back({Kind::DeclineTrade});
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
    if (settings.corners) {
      cornerActions(state, out);
    } else if (shipsOf(state, state.turn) < settings.maxShips &&
               canAfford(me.resources, settings.shipCost)) {
      for (std::size_t i = 0; i < state.planets.size(); ++i) {
        if (dockedAt(state.planets[i], state.turn))
          out.push_back({Kind::BuildShip, static_cast<int>(i)});
      }
    }
    // (with corners, cornerActions gave the ships' and stations' moves)
    for (std::size_t i = 0; !settings.corners && i < state.ships.size(); ++i) {
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
    if (!settings.corners && canAfford(me.resources, settings.colonyCost)) {
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
      const bool revealed =
          std::find(me.revealed.begin(), me.revealed.end(), id) != me.revealed.end();
      const bool offered = std::any_of(out.begin(), out.end(), [&](const Action &a) {
        return a.kind == Kind::RevealContract && a.a == id;
      });
      if (isCharter(id) && !revealed && !offered &&
          contracts()[static_cast<std::size_t>(id)].met(state, state.turn))
        out.push_back({Kind::RevealContract, id});
    }
    if (canPlayCard(state, kCartelCard))
      for (int r = 0; r < kResources; ++r)
        out.push_back({Kind::PlayCartel, r});
    if (canPlayCard(state, kSupplyDropCard))
      for (int a = 0; a < kResources; ++a)
        for (int b = a; b < kResources; ++b)
          out.push_back({Kind::PlaySupplyDrop, a, b});
    if (canPlayCard(state, kEnforcerCard))
      out.push_back({Kind::PlayEnforcer});
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
    // From the pile: the five kinds, then the moons
    std::array<int, kResources + 1> pile{};
    std::copy(state.kindsLeft.begin(), state.kindsLeft.end(), pile.begin());
    pile[kResources] = state.moonsLeft;
    const int drawn = weighted(pile);
    if (drawn == kResources)
      return {Kind::RevealPlanet, static_cast<int>(PlanetKind::Moon), 0};
    // A number from the pile (by the red-number rule, not a 6 or 8 by a
    // face-up 6 or 8 while there's another)
    std::vector<int> allowed = state.numbersLeft;
    if (state.settings.redNumbersApart && redNumberNextTo(state, state.pendingPlanet) &&
        pileHasOtherThanRed(state))
      allowed.erase(std::remove_if(allowed.begin(), allowed.end(), isRed), allowed.end());
    const int number = allowed[random.below(static_cast<std::uint32_t>(allowed.size()))];
    return {Kind::RevealPlanet, drawn, number};
  }
  case Phase::ZoneDraw:
    return {Kind::RevealFind, weighted(state.findsLeft)};
  case Phase::HomeDieDraw:
    return {Kind::RollHomeDie, static_cast<int>(random.below(kResources + 1))};
  case Phase::HomeNumberDraw: {
    const std::vector<int> &numbers = state.settings.exhaustedNumbers;
    const int number = numbers[random.below(static_cast<std::uint32_t>(numbers.size()))];
    const int kind = state.settings.exhaustedKindRandom
                         ? static_cast<int>(random.below(static_cast<std::uint32_t>(kResources)))
                         : -1;
    return {Kind::NumberHome, number, kind};
  }
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
      << " phase " << phaseName(state.phase) << "\n";
  out << settingsText(state.settings) << "\n";
  for (const Planet &p : state.planets) {
    out << "planet " << hexText(p.hex) << " "
        << (p.flipped ? kKindNames[static_cast<std::size_t>(p.kind)] : "?") << " " << p.number
        << (p.core ? " core" : "") << " home=" << p.homeOf << " former=" << p.formerHomeOf
        << " docks=" << p.docks[0] << "," << p.docks[1] << " colony=" << p.colony[0] << ","
        << p.colony[1] << "\n";
  }
  for (const ZoneSite &z : state.zone)
    out << "zone " << hexText(z.hex) << " "
        << (z.flipped ? kFindNames[static_cast<std::size_t>(z.find)] : "?") << "\n";
  for (const Ship &s : state.ships)
    out << "ship " << s.owner << " "
        << (state.settings.corners ? "corner " + std::to_string(cornerNumber(s.corner))
                                   : hexText(s.at))
        << (s.moved ? " moved" : "") << "\n";
  for (const Station &s : state.stations)
    out << "station " << s.owner << " corner " << cornerNumber(s.at) << (s.colony ? " colony" : "")
        << "\n";
  for (int i = 0; i < state.players; ++i) {
    const Player &p = state.player[static_cast<std::size_t>(i)];
    out << "player " << i << " resources=" << joined(p.resources) << " badges=" << p.badges << ","
        << p.badgesEarned << "," << p.badgePoints << " finds=" << p.findsKept
        << " contracts=" << joined(p.contracts) << " revealed=" << joined(p.revealed)
        << " enforcers=" << p.enforcersPlayed << " new=" << joined(p.boughtThisTurn) << "\n";
  }
  out << "left kinds=" << joined(state.kindsLeft) << " moons=" << state.moonsLeft
      << " numbers=" << joined(state.numbersLeft) << " finds=" << joined(state.findsLeft)
      << " deck=" << joined(state.deck) << "\n";
  out << "blockade=" << state.blockade << " core=" << state.coreHolder
      << " embargo=" << state.embargoHolder << "," << (state.cardPlayed ? 1 : 0)
      << " winner=" << state.winner << " roll=" << state.lastRoll
      << " pending=" << state.pendingPlanet << "," << state.pendingSite
      << " victims=" << joined(state.victims) << " salvage=" << state.salvageLeft
      << " income=" << state.homeIncomeLeft << " offered=" << state.offered[0] << ","
      << state.offered[1] << " first=" << state.first << " setup=" << state.setupStep
      << (state.placingShip ? " placing" : "") << "\n";
  out << "final=" << (state.finalRound ? 1 : 0) << " trading=" << (state.tradingDone ? 1 : 0)
      << " offer=" << state.offer.give << "," << state.offer.get << "," << state.offer.to << ","
      << state.offer.replier << "," << state.offer.counterGive << "," << state.offer.counterGet
      << " made=";
  for (const auto &made : state.offersMade)
    out << made[0] << ":" << made[1] << ";";
  out << "\n";
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
  case Kind::PlayEnforcer:
    return "enforcer";
  case Kind::PlayShipyard:
    return "shipyard " + n(action.a);
  case Kind::RollHomeDie:
    return "home_die " + n(action.a);
  case Kind::PlaySupplyDrop:
    return "supply " + res(action.a) + " " + res(action.b);
  case Kind::PlayCartel:
    return "cartel " + res(action.a);
  case Kind::CashBadges:
    return "badges";
  case Kind::EndTurn:
    return "end";
  case Kind::PlaceColony:
    return "place_colony " + n(action.a);
  case Kind::PlaceShip:
    return "place_ship " + n(action.a);
  case Kind::OfferTrade:
    return "offer " + n(action.a) + " " + n(action.b) + " " + n(action.c);
  case Kind::AcceptTrade:
    return "accept";
  case Kind::DeclineTrade:
    return "decline";
  case Kind::CounterTrade:
    return "counter " + n(action.a) + " " + n(action.b);
  case Kind::EndTrading:
    return "end_trading";
  case Kind::NumberHome:
    return "number_home " + n(action.a) + " " + n(action.b);
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
    if (verb == "enforcer")
      return make(Kind::PlayEnforcer);
    if (verb == "end")
      return make(Kind::EndTurn);
    if (verb == "accept")
      return make(Kind::AcceptTrade);
    if (verb == "decline")
      return make(Kind::DeclineTrade);
    if (verb == "end_trading")
      return make(Kind::EndTrading);
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
    if (verb == "cartel" && r >= 0)
      return make(Kind::PlayCartel, r);
    if (verb == "find") {
      const int f = indexOf(kFindNames, w[1]);
      return f >= 0 ? make(Kind::RevealFind, f) : std::nullopt;
    }
    const auto v = num(w[1]);
    if (!v)
      return std::nullopt;
    if (verb == "ship")
      return make(Kind::BuildShip, *v);
    if (verb == "shipyard")
      return make(Kind::PlayShipyard, *v);
    if (verb == "home_die")
      return make(Kind::RollHomeDie, *v);
    if (verb == "place_colony")
      return make(Kind::PlaceColony, *v);
    if (verb == "place_ship")
      return make(Kind::PlaceShip, *v);
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
  if (w.size() == 4 && verb == "offer") {
    const auto give = num(w[1]), get = num(w[2]), to = num(w[3]);
    return give && get && to ? make(Kind::OfferTrade, *give, *get, *to) : std::nullopt;
  }
  if (w.size() == 3) {
    if (verb == "supply") {
      const int a = res(w[1]), b = res(w[2]);
      return a >= 0 && b >= 0 ? make(Kind::PlaySupplyDrop, a, b) : std::nullopt;
    }
    if (verb == "trade") {
      const int give = res(w[1]), get = res(w[2]);
      return give >= 0 && get >= 0 ? make(Kind::Trade, give, get) : std::nullopt;
    }
    if (verb == "planet") {
      const int kind = indexOf(kKindNames, w[1]);
      const auto number = num(w[2]);
      const bool known =
          (kind >= 0 && kind < kResources) || kind == static_cast<int>(PlanetKind::Moon);
      return known && number ? make(Kind::RevealPlanet, kind, *number) : std::nullopt;
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
    if (verb == "number_home")
      return make(Kind::NumberHome, *a, *b);
    if (verb == "counter")
      return make(Kind::CounterTrade, *a, *b);
    if (verb == "blockade")
      return make(Kind::MoveBlockade, *a, *b);
    if (verb == "deal")
      return make(Kind::DealContracts, *a, *b);
  }
  return std::nullopt;
}

} // namespace CosmicHex
