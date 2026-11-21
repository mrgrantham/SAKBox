// Setup.cpp - setting up: the boards to choose from, the rules as agreed,
// newGame, the starting hands dealt at random (Settings::startCards), and,
// with corners, the start's placements (home colonies, then ships, in snake
// order).
#include "RulesInternal.hpp"

#include <HexGrid/HexagonMap.hpp>

#include <cassert>
#include <span>
#include <utility>

namespace CosmicHex {

using namespace Internal;

namespace {

// The homes: slots on the outer ring, evenly spread (players 0 and 1 face
// each other, so a two-player game uses opposite homes)
constexpr std::array<Hex, kMaxPlayers> kHomes = {{{4, -2}, {-4, 2}, {2, 2}, {-2, -2}}};

bool isSlot(Hex hex) { return ((hex.q - hex.r) % 3 + 3) % 3 == 0; }

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

// The player whose setup placement is next: colonies in turn order from the
// first player, then ships in reverse (snake order)
int setupPlayer(const State &state) {
  const int n = state.players, step = state.setupStep;
  return step < n ? (state.first + step) % n : (state.first + 2 * n - 1 - step) % n;
}

// The starting hands dealt: the start newGame would otherwise have begun
// with (placing pieces, with corners; else the first player's home income)
void startAfterDealing(State &state) {
  if (state.settings.corners) {
    state.phase = Phase::PlaceColony;
    state.turn = setupPlayer(state);
    return;
  }
  state.turn = state.first;
  nextHomeIncome(state);
}

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
// planet, free under the distance rule; or, choosing homes, corners of any
// home not yet taken
bool homeNotYetTaken(const Planet &planet) {
  // (homes start face up; a face-down planet's kind isn't set yet)
  return planet.flipped && planet.kind == PlanetKind::Home && planet.homeOf < 0 &&
         planet.formerHomeOf < 0;
}
std::vector<int> colonyPlaces(const State &state, int player) {
  std::vector<int> out;
  const bool choosing = state.settings.chooseHome;
  for (const Planet &planet : state.planets) {
    if (choosing ? !homeNotYetTaken(planet) : planet.homeOf != player)
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
    // Any corner touching no planet (homes included), with no ship already:
    // the planets' corners marked first (six each, rather than every
    // corner's hexes looked up among the planets)
    const std::vector<Corner> &all = boardCorners();
    std::vector<bool> byAPlanet(all.size(), false);
    for (const Planet &planet : state.planets)
      for (int i = 0; i < 6; ++i)
        if (const int number = cornerNumber(HexGrid::corner(planet.hex, i)); number >= 0)
          byAPlanet[static_cast<std::size_t>(number)] = true;
    std::vector<int> out;
    for (std::size_t number = 0; number < all.size(); ++number) {
      if (byAPlanet[number])
        continue;
      const bool taken = std::any_of(state.ships.begin(), state.ships.end(),
                                     [&](const Ship &ship) { return ship.corner == all[number]; });
      if (!taken)
        out.push_back(static_cast<int>(number));
    }
    return out;
  }
  const int home = homeColonyOf(state, player);
  if (home < 0)
    return {};
  const Corner at = state.stations[static_cast<std::size_t>(home)].at;
  std::vector<int> out{cornerNumber(at)};
  for (const CornerReach &reach : cornersWithin(at, state.settings.startShipReach))
    out.push_back(reach.corner);
  std::sort(out.begin(), out.end());
  return out;
}

} // namespace

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

std::optional<Board> boardFromLine(std::string_view line) {
  Board board;
  bool first = true; // (the layout's name)
  while (!line.empty()) {
    const std::size_t space = line.find(' ');
    const std::string_view field = line.substr(0, space);
    line.remove_prefix(space == std::string_view::npos ? line.size() : space + 1);
    if (std::exchange(first, false))
      continue;
    const std::size_t eq = field.find('=');
    if (eq == std::string_view::npos)
      continue;
    const std::string_view key = field.substr(0, eq);
    std::vector<Hex> *into = key == "homes"                          ? &board.homes
                             : key == "planets"                      ? &board.planets
                             : key == "zone" || key == "small_finds" ? &board.zone
                                                                     : nullptr;
    if (!into)
      continue; // (reach= and such, for people reading the file)
    const auto values = intList(field.substr(eq + 1));
    if (!values || values->size() % 2 != 0)
      return std::nullopt;
    into->clear();
    for (std::size_t i = 0; i < values->size(); i += 2)
      into->push_back(Hex{(*values)[i], (*values)[i + 1]});
  }
  return board;
}

std::vector<NamedBoard> boardChoices(int players) {
  // (copied from layouts/layouts.txt; AgreedRules_test checks they match)
  static constexpr std::array<std::string_view, 2> kThreePlayerLines{
      "T4 homes=4,-2,-2,-2,-2,4 planets=0,0,0,2,-1,2,2,-2,2,-1,-2,0,-1,-1,-4,4,4,0,0,-4,4,-4,-4,0,0"
      ",4 small_finds=-4,1,1,3,3,-4,0,-3,-3,3,3,0,0,-2,-2,2,2,0",
      "T5 homes=4,-2,-2,-2,-2,4 planets=0,0,-3,1,-3,0,1,2,0,3,2,-3,3,-3,0,-4,-4,4,4,0,2,0,0,-2,-2,2"
      " small_finds=1,0,0,-1,-1,1,3,1,1,-4,-4,3",
  };
  static constexpr std::array<std::string_view, 9> kLines{
      "R3 homes=4,-2,-4,2,2,2,-2,-2 planets=0,0,-4,0,-4,4,-2,0,-2,3,-2,4,-1,-1,-1,3,0,-4,0,4,1,-3,"
      "1,1,2,-4,2,-3,2,0,4,-4,4,0 small_finds=-3,0,3,0,-2,1,2,-1,-1,0,1,0,0,-1,0,1",
      "S5 homes=4,-4,-4,4,4,0,-4,0 planets=0,0,-4,2,-3,1,-2,-1,-2,3,-2,4,-1,-2,-1,-1,-1,3,0,-4,0,"
      "4,1,-3,1,1,1,2,2,-4,2,-3,2,1,3,-1,4,-2 small_finds=-3,2,3,-2,-2,1,2,-1",
      "K6 homes=4,-4,-4,4,4,0,-4,0 planets=0,0,-4,2,-3,1,-3,2,-2,-1,-2,4,-1,-3,-1,3,1,-3,1,3,2,-4,"
      "2,1,3,-2,3,-1,4,-2",
      "K10 homes=4,-4,-4,4,4,0,-4,0 planets=0,0,-4,2,-3,1,-3,2,-2,-2,-2,3,-1,-1,-1,3,0,-4,0,4,1,-3,"
      "1,1,2,-3,2,2,3,-2,3,-1,4,-2",
      "K14 homes=4,-4,-4,4,4,0,-4,0 planets=0,0,-4,2,-3,1,-3,2,-2,-2,-2,-1,-2,3,-2,4,0,-3,0,-2,0,2,"
      "0,3,2,-4,2,-3,2,1,2,2,3,-2,3,-1,4,-2",
      "WS2 homes=4,-4,-4,4,4,0,-4,0 planets=0,0,0,3,-1,4,0,4,0,-3,1,-4,0,-4,2,-3,2,-2,-2,3,-2,2,2,1"
      ",2,2,-2,-1,-2,-2,-4,2,4,-2 small_finds=1,-1,-1,1,-1,2,1,-2,-3,1,3,-1",
      "FR1 homes=4,-2,-4,2,2,2,-2,-2 planets=0,0,1,-2,2,-3,1,-3,2,-4,-1,2,-2,3,-1,3,-2,4,-3,0,-2,"
      "0,3,0,2,0,4,-4,-4,4,0,-4,0,4 small_finds=-1,-1,1,1,-2,1,2,-1,-1,0,1,0",
      "FR3 homes=4,-2,-4,2,2,2,-2,-2 planets=0,0,-2,4,-1,3,-2,3,-1,2,2,-4,1,-3,2,-3,1,-2,4,0,3,0,"
      "-4,0,-3,0,-4,4,4,-4,0,-4,0,4 small_finds=-1,0,1,0,2,-1,-2,1,-2,0,2,0",
      "FS1 homes=4,-4,-4,4,4,0,-4,0 planets=0,0,4,-2,3,-2,3,-1,2,-1,-4,2,-3,2,-3,1,-2,1,-1,2,-2,3"
      ",1,-2,2,-3,-1,-1,-2,-1,1,1,2,1,0,-4,0,4 small_finds=1,-3,-1,3,1,2,-1,-2,0,-3,0,3,2,-4,-2,4",
  };
  std::vector<NamedBoard> out;
  const auto lines = players == 3 ? std::span<const std::string_view>(kThreePlayerLines)
                                  : std::span<const std::string_view>(kLines);
  for (std::string_view line : lines) {
    const auto board = boardFromLine(line);
    assert(board.has_value());
    out.push_back({std::string(line.substr(0, line.find(' '))), *board, {}, ""});
  }
  // R3 leans to the first player under today's rules; one more resource to
  // start for the last two evens its turn order (chi-square 15.0
  // to 2.0). A choice when setting up, off unless chosen
  for (NamedBoard &choice : out) {
    if (choice.name == "R3")
      choice.seatBonus = {0, 0, 1, 1};
    // The four-cluster boards: fair, but their games run longer (// about 18 rounds, where the
    // others take 15 to 16); said beside them (the user's)
    if (choice.name.starts_with("FR") || choice.name.starts_with("FS"))
      choice.note = "longer games: about 18 rounds";
  }
  return out;
}

Settings agreedRules(const Board &board, int players) {
  Settings s;
  if (players == 3)
    s.winPoints = kThreePlayerWinPoints; //
  s.homeHexes = board.homes;
  s.planetHexes = board.planets;
  s.zoneHexes = board.zone;
  // Corners and edges, ships moving 2 edges and 1 more for a fuel, and the
  // first deep-space mission
  s.corners = true;
  s.deepSpaceStart = true;
  s.chooseHome = true; // (each player picks their home and its corner)
  s.shipMove = 2;
  s.boostMove = 1;
  // The hybrid home income: 2 a turn below 5 points, on
  // the home die; at 5 the home flips to a planet
  s.catchUpBelow = 5;
  s.homeDie = true;
  s.exhaustHomesAt = 5;
  s.discardOver = 7;    // the Imperial Tithe
  s.finishRound = true; // the round played out at 12
  // Free stations: a ship becomes a station at no cost; the ship
  // costs minerals, fuel and water (costs down, the quickest fair
  // game measured, about 13 rounds)
  s.stationCost = {0, 0, 0, 0, 0};
  s.shipCost = {1, 0, 1, 1, 0};
  s.playerTrading = true; //
  // No Core Control (the user's: it no longer made sense; any
  // cluster, or none, may use the centre)
  s.coreControlPoints = 0;
  // No fuel boost in the first round (the user's): a ship
  // starting in empty space can't reach a three's centre before the others
  // move; with no Core Control, it makes R3 fair (1,800 games)
  s.firstRoundBoost = false;
  // A starting hand of 3 dealt at random (the user's: 1 of each and the
  // home's 2 a turn made too many cards early); fair by turn order over
  // 3,600 games with the next rule
  s.startCards = 3;
  // A new ship moves off its colony the turn it's built, by the usual
  // move (the user's: no ship standing on a station); fair, games about 2
  // rounds shorter
  s.newShipsLeave = true;
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
          // (chosen at the start: nobody's until a home colony claims it)
          if (p < players && !(settings.corners && settings.chooseHome)) {
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
    state.player[static_cast<std::size_t>(p)].resources.fill(
        settings.startCards > 0 ? 0 : settings.startResources);
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
  // A starting hand dealt at random comes first; the start above follows it
  // (startAfterDealing)
  if (settings.startCards > 0) {
    state.startCardsLeft = settings.startCards * players;
    state.phase = Phase::StartCardsDraw;
  }
  indexHexes(state);
  return state;
}

namespace Internal {

// A starting card, to the next player round the table from the first
bool applyStartCard(State &state, const Action &action) {
  if (action.kind != Kind::DealStartCard || action.a < 0 || action.a >= kResources ||
      state.startCardsLeft <= 0)
    return false;
  const int dealt = state.settings.startCards * state.players - state.startCardsLeft;
  const int to = (state.first + dealt) % state.players;
  ++state.player[static_cast<std::size_t>(to)].resources[static_cast<std::size_t>(action.a)];
  if (--state.startCardsLeft == 0)
    startAfterDealing(state);
  return true;
}

bool applyPlaceColony(State &state, const Action &action) {
  const std::vector<int> places = colonyPlaces(state, state.turn);
  if (action.kind != Kind::PlaceColony ||
      std::find(places.begin(), places.end(), action.a) == places.end())
    return false;
  const Corner at = boardCorners()[static_cast<std::size_t>(action.a)];
  // (choosing homes: the home it's on becomes theirs)
  if (state.settings.chooseHome)
    for (Hex hex : HexGrid::hexesTouching(at))
      if (const int p = planetAt(state, hex);
          p >= 0 && homeNotYetTaken(state.planets[static_cast<std::size_t>(p)])) {
        state.planets[static_cast<std::size_t>(p)].homeOf = state.turn;
        break;
      }
  state.stations.push_back(Station{at, state.turn, true});
  nextSetupStep(state);
  return true;
}

bool applyPlaceShip(State &state, const Action &action) {
  const std::vector<int> places = shipPlaces(state, state.turn);
  if (action.kind != Kind::PlaceShip ||
      std::find(places.begin(), places.end(), action.a) == places.end())
    return false;
  const Corner at = boardCorners()[static_cast<std::size_t>(action.a)];
  state.ships.push_back(Ship{state.turn, Hex{}, false, at});
  if (state.settings.deepSpaceStart) {
    // The finds it lands by turn over first (the setup goes on after:
    // finishPlacingShip)
    for (Hex hex : HexGrid::hexesTouching(at))
      state.turning.push_back(hex);
    state.placingShip = true;
    turnNextOver(state);
    if (state.phase == Phase::PlaceShip) { // (nothing to turn over)
      state.placingShip = false;
      nextSetupStep(state);
    }
    return true;
  }
  nextSetupStep(state);
  return true;
}

void finishPlacingShip(State &state) {
  if (state.placingShip && state.phase == Phase::Main && state.turning.empty()) {
    state.placingShip = false;
    nextSetupStep(state);
  }
}

void colonyPlacements(const State &state, std::vector<Action> &out) {
  for (int corner : colonyPlaces(state, state.turn))
    out.push_back({Kind::PlaceColony, corner});
}

void shipPlacements(const State &state, std::vector<Action> &out) {
  for (int corner : shipPlaces(state, state.turn))
    out.push_back({Kind::PlaceShip, corner});
}

} // namespace Internal

} // namespace CosmicHex
