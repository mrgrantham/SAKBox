// Building and moving (BuildingAndMoving.cpp) and the board's lookups
// (Board.cpp): the distance rule, the ship limit, moves with and without a
// fuel boost, colonies upgraded from stations, and nothing built without
// its cost; corners numbered, and the hex index the same as a search.
#include <CosmicHex/Rules.hpp>

#include "TestGames.hpp"

#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Paths.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <set>

using namespace CosmicHex;
using namespace CosmicHex::TestGames;
using Kind = Action::Kind;

namespace {

// Whether a planet a station can be built by touches the corner
bool bySettleablePlanet(const State &state, Corner corner) {
  for (Hex hex : HexGrid::hexesTouching(corner)) {
    const int planet = planetIndex(state, hex);
    if (planet >= 0 && state.planets[static_cast<std::size_t>(planet)].flipped &&
        state.planets[static_cast<std::size_t>(planet)].homeOf < 0)
      return true;
  }
  return false;
}

// A corner a station could go on: by a planet, with no station on it or
// next to it, nor a ship on it
std::optional<Corner> aFreeSpot(const State &state) {
  for (Corner corner : boardCorners()) {
    const bool shipThere = std::any_of(state.ships.begin(), state.ships.end(),
                                       [&](const Ship &ship) { return ship.corner == corner; });
    if (!shipThere && bySettleablePlanet(state, corner) && clearOfStations(state, corner))
      return corner;
  }
  return std::nullopt;
}

// The edges between two corners
int edgesBetween(Corner from, Corner to) {
  return static_cast<int>(HexGrid::shortestPath(from, to, [](Corner) { return true; }).size()) - 1;
}

// Where player 0's ship can move to now, and how far each is
std::set<int> moveLengths(const State &state, int ship) {
  std::set<int> lengths;
  for (const Action &action : Game::legalActions(state))
    if (action.kind == Kind::Move && action.a == ship)
      lengths.insert(edgesBetween(state.ships[static_cast<std::size_t>(ship)].corner,
                                  boardCorners()[static_cast<std::size_t>(action.b)]));
  return lengths;
}

} // namespace

// The distance rule: no station on a corner next to another's (anyone's);
// two edges away is free
TEST(BuildingAndMovingTest, TheDistanceRule) {
  State state = cornersAtMain();
  const std::optional<Corner> spot = aFreeSpot(state);
  ASSERT_TRUE(spot.has_value());
  const Corner at = spot.value_or(Corner{});
  const int mine = firstShipOf(state, 0);
  state.ships[static_cast<std::size_t>(mine)].corner = at;
  const Corner next = HexGrid::neighbours(at)[0];
  State crowded = state;
  crowded.stations.push_back(Station{next, 1, false});
  EXPECT_FALSE(has(Game::legalActions(crowded), {Kind::Settle, mine}));
  EXPECT_FALSE(Game::apply(crowded, {Kind::Settle, mine}));
  // Two edges away: allowed
  for (Corner further : HexGrid::neighbours(next)) {
    if (further == at)
      continue;
    State spaced = state;
    spaced.stations.push_back(Station{further, 1, false});
    EXPECT_EQ(edgesBetween(at, further), 2);
    EXPECT_TRUE(Game::apply(spaced, {Kind::Settle, mine}));
    break;
  }
}

// A station needs a turned-over planet that isn't a home (or an outpost):
// on hexes not on a face-down planet nor a home, one station a player a
// planet
TEST(BuildingAndMovingTest, WhereAStationCanGoOnHexes) {
  State state = atMain();
  state.player[0].resources = {5, 5, 5, 5, 5};
  const int planet = planetIndex(state, Hex{2, -1});
  state.ships[0].at = Hex{2, -1};
  EXPECT_FALSE(Game::apply(state, {Kind::Settle, 0})); // (face down)
  state.ships[0].at = Hex{-4, 2};
  EXPECT_FALSE(Game::apply(state, {Kind::Settle, 0})); // (player 1's home)
  flip(state, planet, PlanetKind::LushPlanet, 9);
  state.ships[0].at = Hex{2, -1};
  state.ships.push_back(Ship{0, Hex{2, -1}, false});
  ASSERT_TRUE(Game::apply(state, {Kind::Settle, 0}));
  EXPECT_FALSE(Game::apply(state, {Kind::Settle, 1})); // (one each)
}

// No more than maxShips ships a player, on corners or on hexes
TEST(BuildingAndMovingTest, TheShipLimit) {
  State corners = cornersAtMain();
  const int colony = 0; // (player 0's home colony, placed first)
  ASSERT_EQ(corners.stations[colony].owner, 0);
  while (shipsOwned(corners, 0) < corners.settings.maxShips)
    ASSERT_TRUE(Game::apply(corners, {Kind::BuildShip, colony}));
  EXPECT_FALSE(hasKind(Game::legalActions(corners), Kind::BuildShip));
  EXPECT_FALSE(Game::apply(corners, {Kind::BuildShip, colony}));

  State hexes = atMain();
  hexes.player[0].resources = {5, 5, 5, 5, 5};
  const int home = planetIndex(hexes, Hex{4, -2});
  while (shipsOwned(hexes, 0) < hexes.settings.maxShips)
    ASSERT_TRUE(Game::apply(hexes, {Kind::BuildShip, home}));
  EXPECT_FALSE(hasKind(Game::legalActions(hexes), Kind::BuildShip));
  EXPECT_FALSE(Game::apply(hexes, {Kind::BuildShip, home}));
}

// A ship moves shipMove edges free, boostMove more for one fuel; with no
// fuel, only shipMove
TEST(BuildingAndMovingTest, MovesWithAndWithoutTheBoost) {
  State state = cornersAtMain();
  const int mine = firstShipOf(state, 0);
  state.ships[static_cast<std::size_t>(mine)].moved = false;
  state.player[0].resources[of(Resource::Fuel)] = 1;
  EXPECT_EQ(moveLengths(state, mine), (std::set<int>{1, 2, 3}));
  // Within the free move: no fuel
  for (const Action &action : Game::legalActions(state)) {
    if (action.kind != Kind::Move || action.a != mine)
      continue;
    const int edges = edgesBetween(state.ships[static_cast<std::size_t>(mine)].corner,
                                   boardCorners()[static_cast<std::size_t>(action.b)]);
    State moved = state;
    ASSERT_TRUE(Game::apply(moved, action));
    EXPECT_EQ(moved.player[0].resources[of(Resource::Fuel)], edges > 2 ? 0 : 1);
  }
  state.player[0].resources[of(Resource::Fuel)] = 0;
  EXPECT_EQ(moveLengths(state, mine), (std::set<int>{1, 2}));
}

// A ship moves once a turn, and only its owner's; the next turn it moves again
TEST(BuildingAndMovingTest, EachShipMovesOnceATurn) {
  State state = cornersAtMain();
  const int mine = firstShipOf(state, 0), theirs = firstShipOf(state, 1);
  state.ships[static_cast<std::size_t>(mine)].moved = false;
  std::optional<Action> move;
  for (const Action &action : Game::legalActions(state))
    if (action.kind == Kind::Move && action.a == mine && !move)
      move = action;
  ASSERT_TRUE(move.has_value());
  const Action first = move.value_or(Action{});
  for (const Action &action : Game::legalActions(state))
    EXPECT_FALSE(action.kind == Kind::Move && action.a == theirs);
  EXPECT_FALSE(Game::apply(state, {Kind::Move, theirs, first.b}));
  ASSERT_TRUE(Game::apply(state, first));
  SeededRandom::Generator dealer(1);
  while (Game::currentPlayer(state) == GameBots::kDealer) // (anything it landed by)
    ASSERT_TRUE(Game::apply(state, Game::dealerAction(state, dealer)));
  EXPECT_FALSE(Game::apply(state, {Kind::Move, mine, first.b}));
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  EXPECT_FALSE(state.ships[static_cast<std::size_t>(mine)].moved);
}

// On hexes a ship can't move onto the blockade's planet
TEST(BuildingAndMovingTest, NotOntoTheBlockade) {
  State state = atMain();
  const int planet = planetIndex(state, Hex{2, -1});
  flip(state, planet, PlanetKind::LushPlanet, 9);
  EXPECT_TRUE(has(Game::legalActions(state), {Kind::Move, 0, 2, -1}));
  state.blockade = planet;
  EXPECT_FALSE(has(Game::legalActions(state), {Kind::Move, 0, 2, -1}));
  EXPECT_FALSE(Game::apply(state, {Kind::Move, 0, 2, -1}));
}

// A station becomes a colony for colonyCost, once; only the owner's
TEST(BuildingAndMovingTest, AColonyFromAStation) {
  State state = cornersAtMain();
  state.stations.push_back(Station{cornerTouching(state, 2), 0, false});
  const int station = static_cast<int>(state.stations.size()) - 1;
  state.stations.push_back(Station{cornerTouching(state, 1), 1, false});
  const int theirs = static_cast<int>(state.stations.size()) - 1;
  EXPECT_FALSE(Game::apply(state, {Kind::BuildColony, theirs}));
  EXPECT_FALSE(has(Game::legalActions(state), {Kind::BuildColony, theirs}));
  ASSERT_TRUE(Game::apply(state, {Kind::BuildColony, station}));
  EXPECT_EQ(state.player[0].resources, (Resources{3, 3, 5, 5, 5}));
  EXPECT_TRUE(state.stations[static_cast<std::size_t>(station)].colony);
  EXPECT_FALSE(Game::apply(state, {Kind::BuildColony, station}));
  EXPECT_FALSE(Game::apply(state, {Kind::BuildColony, 0})); // (the home colony)
}

// On hexes a colony goes on the player's own dock, never at a home
TEST(BuildingAndMovingTest, AColonyOnHexes) {
  State state = atMain();
  state.player[0].resources = {5, 5, 5, 5, 5};
  const int planet = planetIndex(state, Hex{2, -1}), home = planetIndex(state, Hex{4, -2});
  flip(state, planet, PlanetKind::LushPlanet, 9);
  EXPECT_FALSE(Game::apply(state, {Kind::BuildColony, planet})); // (no station there)
  EXPECT_FALSE(Game::apply(state, {Kind::BuildColony, home}));
  dock(state, planet, 1);
  dock(state, planet, 0);
  ASSERT_TRUE(Game::apply(state, {Kind::BuildColony, planet}));
  EXPECT_EQ(state.planets[static_cast<std::size_t>(planet)].colony,
            (std::array<bool, kDocks>{false, true}));
  EXPECT_FALSE(Game::apply(state, {Kind::BuildColony, planet}));
}

// Nothing is built a resource short: a ship, a station, a colony, a card
TEST(BuildingAndMovingTest, NothingBuiltWithoutItsCost) {
  State state = cornersAtMain();
  state.stations.push_back(Station{cornerTouching(state, 2), 0, false});
  const int station = static_cast<int>(state.stations.size()) - 1;
  const int mine = firstShipOf(state, 0);
  const std::optional<Corner> spot = aFreeSpot(state);
  ASSERT_TRUE(spot.has_value());
  state.ships[static_cast<std::size_t>(mine)].corner = spot.value_or(Corner{});
  const Settings &costs = state.settings;
  for (const auto &[cost, build] : {std::pair{costs.shipCost, Action{Kind::BuildShip, 0}},
                                    std::pair{costs.stationCost, Action{Kind::Settle, mine}},
                                    std::pair{costs.colonyCost, Action{Kind::BuildColony, station}},
                                    std::pair{costs.contractCost, Action{Kind::BuyContract}}}) {
    for (std::size_t r = 0; r < cost.size(); ++r) {
      if (cost[r] == 0)
        continue;
      State poor = state;
      poor.player[0].resources = cost;
      EXPECT_TRUE(has(Game::legalActions(poor), build)) << static_cast<int>(build.kind);
      --poor.player[0].resources[r];
      EXPECT_FALSE(has(Game::legalActions(poor), build)) << static_cast<int>(build.kind);
      EXPECT_FALSE(Game::apply(poor, build)) << static_cast<int>(build.kind);
    }
  }
}

// Every corner of the board has its place in boardCorners() as its number;
// a corner off the board has none
TEST(BoardTest, CornersAreNumberedByTheirPlace) {
  const std::vector<Corner> &corners = boardCorners();
  ASSERT_FALSE(corners.empty());
  for (std::size_t i = 0; i < corners.size(); ++i)
    EXPECT_EQ(cornerNumber(corners[i]), static_cast<int>(i));
  EXPECT_TRUE(std::is_sorted(corners.begin(), corners.end()));
  EXPECT_EQ(cornerNumber(HexGrid::corner(Hex{9, 0}, 0)), -1);
}

// A state put together without the hex index finds the same pieces by
// searching: its moves are the same
TEST(BoardTest, TheHexIndexAgreesWithASearch) {
  for (const State &indexed : {atMain(), cornersAtMain()}) {
    ASSERT_TRUE(indexed.hexesIndexed);
    State searched = indexed;
    searched.hexesIndexed = false;
    searched.planetOnHex.fill(-1);
    searched.siteOnHex.fill(-1);
    EXPECT_EQ(Game::legalActions(searched), Game::legalActions(indexed));
  }
}
