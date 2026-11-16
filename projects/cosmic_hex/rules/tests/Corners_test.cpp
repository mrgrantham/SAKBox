// Cosmic Hex with corners and edges (Settings::corners; design.md, "Corners
// and edges"): the start (home colonies, then ships, in snake order), ships
// moving along edges (2 a turn, 1 more for fuel), planets turning over where
// a move ends, stations on corners under the distance rule, income from
// every planet a station touches, ships built only at colonies, the
// blockade, the text forms, and bots playing whole games.
#include <CosmicHex/Rules.hpp>

#include <GameBots/Bots.hpp>
#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Paths.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

using namespace CosmicHex;
using Kind = Action::Kind;

namespace {

// Corners, on layout R2 (layouts/layouts.txt: clusters of 3, 2 and 1)
Settings cornerSettings() {
  Settings settings;
  settings.corners = true;
  settings.shipMove = 2;
  settings.boostMove = 1;
  settings.planetHexes = {{0, 0}, {-4, 4}, {-3, 0}, {-2, 0}, {-2, 3}, {-2, 4}, {-1, 3}, {0, -4},
                          {0, 4}, {1, -3}, {2, -4}, {2, -3}, {2, 0},  {3, 0},  {4, -4}};
  return settings;
}

// Player 0's ship (ships are placed in reverse order, so it may not be first)
int firstShipOf(const State &state, int player) {
  for (std::size_t i = 0; i < state.ships.size(); ++i)
    if (state.ships[i].owner == player)
      return static_cast<int>(i);
  ADD_FAILURE() << "no ship of player " << player;
  return 0;
}

// Plays the start by taking each placement's first choice
State afterSetup(int players = 4, int first = 0) {
  State state = newGame(players, cornerSettings(), first);
  while (state.phase == Phase::PlaceColony || state.phase == Phase::PlaceShip) {
    const std::vector<Action> legal = Game::legalActions(state);
    EXPECT_FALSE(legal.empty());
    if (legal.empty())
      break;
    EXPECT_TRUE(Game::apply(state, legal.front()));
  }
  return state;
}

// A game in its main part, player 0's turn, every planet turned over with
// number 6, and nobody's ship anywhere useful
State atMain() {
  State state = afterSetup(2);
  for (Planet &planet : state.planets) {
    if (planet.homeOf >= 0)
      continue;
    planet.flipped = true;
    planet.kind = PlanetKind::GasGiant;
    planet.number = 6;
  }
  state.turn = 0;
  state.phase = Phase::Main;
  state.player[0].resources = {5, 5, 5, 5, 5};
  return state;
}

int numberOf(Corner corner) { return cornerNumber(corner); }

// A corner touching exactly the given planet hexes among the board's planets
Corner cornerTouching(const State &state, int planetsTouched) {
  for (Corner corner : boardCorners()) {
    int count = 0;
    bool home = false;
    for (Hex hex : HexGrid::hexesTouching(corner)) {
      for (const Planet &planet : state.planets) {
        if (planet.hex == hex) {
          ++count;
          home = home || planet.homeOf >= 0;
        }
      }
    }
    if (count == planetsTouched && !home)
      return corner;
  }
  ADD_FAILURE() << "no corner touching " << planetsTouched << " planets";
  return Corner{};
}

} // namespace

// The start: colonies on a corner of each home planet in turn order from
// the first player, then ships in reverse, each within 3 edges of its
// colony; then the first player's turn
TEST(CornersTest, TheStartIsPlacedInSnakeOrder) {
  State state = newGame(4, cornerSettings(), 1);
  EXPECT_EQ(state.phase, Phase::PlaceColony);
  EXPECT_TRUE(state.ships.empty());
  std::vector<int> order;
  while (state.phase == Phase::PlaceColony || state.phase == Phase::PlaceShip) {
    order.push_back(state.turn);
    const std::vector<Action> legal = Game::legalActions(state);
    ASSERT_FALSE(legal.empty());
    for (const Action &action : legal) {
      const Corner corner = boardCorners()[static_cast<std::size_t>(action.a)];
      if (action.kind == Kind::PlaceColony) {
        // on their own home planet
        bool home = false;
        for (const Planet &planet : state.planets)
          home = home ||
                 (planet.homeOf == state.turn && HexGrid::hexesTouching(corner)[0] == planet.hex) ||
                 (planet.homeOf == state.turn && (HexGrid::hexesTouching(corner)[1] == planet.hex ||
                                                  HexGrid::hexesTouching(corner)[2] == planet.hex));
        EXPECT_TRUE(home);
      }
    }
    ASSERT_TRUE(Game::apply(state, legal.back()));
  }
  EXPECT_EQ(order, (std::vector<int>{1, 2, 3, 0, 0, 3, 2, 1}));
  EXPECT_EQ(state.turn, 1);
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  EXPECT_EQ(state.stations.size(), 4u);
  EXPECT_EQ(state.ships.size(), 4u);
  for (int p = 0; p < 4; ++p)
    EXPECT_EQ(points(state, p), 2); // the home colony
}

// A starting ship goes on the colony's corner or one within 3 edges
TEST(CornersTest, AStartingShipStaysNearItsColony) {
  State state = newGame(2, cornerSettings(), 0);
  ASSERT_TRUE(Game::apply(state, Game::legalActions(state).front())); // player 0's colony
  ASSERT_TRUE(Game::apply(state, Game::legalActions(state).front())); // player 1's
  ASSERT_EQ(state.phase, Phase::PlaceShip);
  const Corner colony = state.stations[static_cast<std::size_t>(state.turn)].at;
  for (const Action &action : Game::legalActions(state)) {
    const Corner at = boardCorners()[static_cast<std::size_t>(action.a)];
    const auto way = HexGrid::shortestPath(colony, at, [](Corner) { return true; });
    EXPECT_LE(way.size(), 4u); // 3 edges or fewer
  }
  // One too far is refused
  for (Corner corner : boardCorners()) {
    const auto way = HexGrid::shortestPath(colony, corner, [](Corner) { return true; });
    if (way.size() == 5) {
      EXPECT_FALSE(Game::apply(state, {Kind::PlaceShip, numberOf(corner)}));
      break;
    }
  }
}

// Ships move 2 edges free, 1 more for a fuel, no further
TEST(CornersTest, ShipsMoveAlongEdges) {
  State state = atMain();
  const int mine = firstShipOf(state, 0);
  Ship &ship = state.ships[static_cast<std::size_t>(mine)];
  ship.moved = false;
  state.player[0].resources[static_cast<std::size_t>(Resource::Fuel)] = 1;
  const Corner from = ship.corner;
  std::set<std::size_t> lengths;
  for (const Action &action : Game::legalActions(state)) {
    if (action.kind != Kind::Move || action.a != mine)
      continue;
    const auto way = HexGrid::shortestPath(from, boardCorners()[static_cast<std::size_t>(action.b)],
                                           [](Corner) { return true; });
    lengths.insert(way.size() - 1);
  }
  EXPECT_EQ(lengths, (std::set<std::size_t>{1, 2, 3}));
  // 3 edges costs the fuel
  for (Corner corner : boardCorners()) {
    const auto way = HexGrid::shortestPath(from, corner, [](Corner) { return true; });
    if (way.size() == 4) {
      State moved = state;
      if (Game::apply(moved, {Kind::Move, mine, numberOf(corner)})) {
        EXPECT_EQ(moved.player[0].resources[static_cast<std::size_t>(Resource::Fuel)], 0);
        EXPECT_EQ(moved.ships[static_cast<std::size_t>(mine)].corner, corner);
        EXPECT_TRUE(moved.ships[static_cast<std::size_t>(mine)].moved);
        break;
      }
    }
  }
}

// Where a move ends, every face-down planet and tile it touches turns over,
// one after another; passing by turns nothing over
TEST(CornersTest, PlanetsTurnOverWhereAMoveEnds) {
  State state = atMain();
  const Corner rich = cornerTouching(state, 2);
  // Face down again: the two planets this corner touches
  std::vector<int> touched;
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    for (Hex hex : HexGrid::hexesTouching(rich))
      if (state.planets[i].hex == hex) {
        state.planets[i].flipped = false;
        touched.push_back(static_cast<int>(i));
      }
  }
  ASSERT_EQ(touched.size(), 2u);
  state.kindsLeft = {2, 0, 0, 0, 0};
  state.numbersLeft = {5, 9};
  const int mine = firstShipOf(state, 0);
  state.ships[static_cast<std::size_t>(mine)].corner = HexGrid::neighbours(rich)[0];
  state.ships[static_cast<std::size_t>(mine)].moved = false;
  const int badges = state.player[0].badgesEarned;
  ASSERT_TRUE(Game::apply(state, {Kind::Move, mine, numberOf(rich)}));
  EXPECT_EQ(state.phase, Phase::PlanetDraw);
  ASSERT_TRUE(Game::apply(state, {Kind::RevealPlanet, 0, 5}));
  EXPECT_EQ(state.phase, Phase::PlanetDraw); // the second
  ASSERT_TRUE(Game::apply(state, {Kind::RevealPlanet, 0, 9}));
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.player[0].badgesEarned, badges + 2);
  for (int planet : touched)
    EXPECT_TRUE(state.planets[static_cast<std::size_t>(planet)].flipped);
}

// A station goes on a free corner by a turned-over planet: not next to
// another station (the distance rule), nor where it would touch no planet
TEST(CornersTest, StationsFollowTheDistanceRule) {
  State state = atMain();
  const Corner spot = cornerTouching(state, 2);
  const int mine = firstShipOf(state, 0);
  state.ships[static_cast<std::size_t>(mine)].corner = spot;
  const std::size_t before = state.stations.size();
  ASSERT_TRUE(Game::apply(state, {Kind::Settle, mine}));
  EXPECT_EQ(state.stations.size(), before + 1);
  EXPECT_EQ(state.stations.back().at, spot);
  EXPECT_EQ(state.stations.back().owner, 0);
  // Next door: refused
  state.ships.push_back(Ship{0, Hex{}, false, HexGrid::neighbours(spot)[0]});
  EXPECT_FALSE(Game::apply(state, {Kind::Settle, static_cast<int>(state.ships.size()) - 1}));
  // Touching no planet: refused
  state.ships.back().corner = cornerTouching(state, 0);
  EXPECT_FALSE(Game::apply(state, {Kind::Settle, static_cast<int>(state.ships.size()) - 1}));
}

// On a roll, a station earns from every planet it touches with that number,
// a colony twice as much
TEST(CornersTest, AStationEarnsFromEveryPlanetItTouches) {
  State state = atMain();
  const Corner spot = cornerTouching(state, 2);
  state.stations.push_back(Station{spot, 1, false});
  const int before = state.player[1].resources[static_cast<std::size_t>(PlanetKind::GasGiant)];
  state.phase = Phase::RollDraw;
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 3, 3}));
  EXPECT_EQ(state.player[1].resources[static_cast<std::size_t>(PlanetKind::GasGiant)], before + 2);
  state.stations.back().colony = true;
  state.phase = Phase::RollDraw;
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 2, 4}));
  EXPECT_EQ(state.player[1].resources[static_cast<std::size_t>(PlanetKind::GasGiant)], before + 6);
}

// Ships are built only at colonies; a station can be upgraded to one
TEST(CornersTest, ShipsAreBuiltAtColonies) {
  State state = atMain();
  state.ships.clear(); // (room for more)
  state.stations.push_back(Station{cornerTouching(state, 2), 0, false});
  const int station = static_cast<int>(state.stations.size()) - 1;
  EXPECT_FALSE(Game::apply(state, {Kind::BuildShip, station}));
  ASSERT_TRUE(Game::apply(state, {Kind::BuildColony, station}));
  EXPECT_TRUE(state.stations.back().colony);
  ASSERT_TRUE(Game::apply(state, {Kind::BuildShip, station}));
  EXPECT_EQ(state.ships.back().corner, state.stations.back().at);
  EXPECT_TRUE(state.ships.back().moved); // (it moves from next turn)
}

// The blockade steals from players with a station touching its planet
TEST(CornersTest, TheBlockadeStealsFromStationsTouchingIt) {
  State state = atMain();
  const Corner spot = cornerTouching(state, 2);
  state.stations.push_back(Station{spot, 1, false});
  state.player[1].resources = {1, 0, 0, 0, 0};
  int planet = -1;
  for (std::size_t i = 0; i < state.planets.size(); ++i)
    if (state.planets[i].hex == HexGrid::hexesTouching(spot)[0] ||
        state.planets[i].hex == HexGrid::hexesTouching(spot)[1] ||
        state.planets[i].hex == HexGrid::hexesTouching(spot)[2])
      planet = static_cast<int>(i);
  ASSERT_GE(planet, 0);
  state.phase = Phase::Blockade;
  bool offered = false;
  for (const Action &action : Game::legalActions(state))
    offered = offered || (action.a == planet && action.b == 1);
  EXPECT_TRUE(offered);
}

// The start reads back from its text, as a log keeps it; the new actions too
TEST(CornersTest, TextReadsBack) {
  const State start = newGame(4, cornerSettings(), 2);
  const auto read = Game::readState(Game::writeState(start));
  ASSERT_TRUE(read.has_value());
  EXPECT_TRUE(read.value_or(State{}).settings.corners);
  EXPECT_EQ(read.value_or(State{}), start);
  for (const Action &action :
       {Action{Kind::PlaceColony, 12}, Action{Kind::PlaceShip, 40}, Action{Kind::Move, 1, 57}})
    EXPECT_EQ(Game::readAction(Game::writeAction(action)), std::make_optional(action));
}

// Bots play whole games with corners, on a cluster layout, to a winner
TEST(CornersTest, BotsPlayWholeGames) {
  const Settings settings = cornerSettings();
  for (std::uint64_t seed = 1; seed <= 1; ++seed) { // (a whole game is slow: one)
    State state = newGame(4, settings, static_cast<int>(seed % 4));
    GameBots::Ismcts<Game> bot;
    SeededRandom::Generator random(seed), dealer(seed + 100);
    for (int done = 0; done < 20000 && !Game::isOver(state); ++done) {
      const Action action = Game::currentPlayer(state) == GameBots::kDealer
                                ? Game::dealerAction(state, dealer)
                                : bot.choose(state, random, {.iterations = 10});
      ASSERT_TRUE(Game::apply(state, action)) << Game::writeAction(action);
    }
    EXPECT_TRUE(Game::isOver(state)) << "seed " << seed;
  }
}
