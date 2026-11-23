// TestGames.cpp - games set up for the rules' tests (TestGames.hpp).
#include "TestGames.hpp"

#include <HexGrid/CornersAndEdges.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>

namespace CosmicHex::TestGames {

using Kind = Action::Kind;

bool has(const std::vector<Action> &actions, const Action &action) {
  return std::find(actions.begin(), actions.end(), action) != actions.end();
}

bool hasKind(const std::vector<Action> &actions, Kind kind) {
  return std::any_of(actions.begin(), actions.end(),
                     [&](const Action &a) { return a.kind == kind; });
}

int cardsOf(const State &state, int player) {
  const Resources &held = state.player[static_cast<std::size_t>(player)].resources;
  return std::accumulate(held.begin(), held.end(), 0);
}

bool playOnUntil(State &state, const std::function<bool(const State &)> &until, int most) {
  SeededRandom::Generator random(1);
  for (int played = 0; played < most; ++played) {
    if (until(state))
      return true;
    if (Game::currentPlayer(state) == GameBots::kDealer) {
      if (!Game::apply(state, Game::dealerAction(state, random)))
        return false;
      continue;
    }
    const std::vector<Action> legal = Game::legalActions(state);
    if (legal.empty())
      return false;
    const auto quickest = std::find_if(legal.begin(), legal.end(), [](const Action &action) {
      return action.kind == Kind::EndTrading || action.kind == Kind::EndTurn;
    });
    if (!Game::apply(state, quickest != legal.end() ? *quickest : legal.front()))
      return false;
  }
  return until(state);
}

State atMain(int players) {
  State state = newGame(players);
  EXPECT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  EXPECT_TRUE(Game::apply(state, {Kind::Roll, 1, 2}));
  EXPECT_EQ(state.phase, Phase::Main);
  return state;
}

State atTrading(int players) {
  Settings settings;
  settings.playerTrading = true;
  State state = newGame(players, settings);
  EXPECT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  EXPECT_TRUE(Game::apply(state, {Kind::Roll, 1, 2}));
  EXPECT_EQ(state.phase, Phase::Trading);
  return state;
}

int planetIndex(const State &state, Hex hex) {
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    if (state.planets[i].hex == hex)
      return static_cast<int>(i);
  }
  return -1;
}

int siteIndex(const State &state, Hex hex) {
  for (std::size_t i = 0; i < state.zone.size(); ++i) {
    if (state.zone[i].hex == hex)
      return static_cast<int>(i);
  }
  return -1;
}

void flip(State &state, int planet, PlanetKind kind, int number) {
  Planet &turned = state.planets[static_cast<std::size_t>(planet)];
  turned.flipped = true;
  turned.kind = kind;
  turned.number = number;
}

void dock(State &state, int planet, int player, bool colony) {
  Planet &docked = state.planets[static_cast<std::size_t>(planet)];
  const std::size_t free = docked.docks[0] < 0 ? 0 : 1;
  EXPECT_LT(docked.docks[free], 0) << "no free dock";
  docked.docks[free] = player;
  docked.colony[free] = colony;
}

Settings cornerSettings() {
  Settings settings;
  settings.corners = true;
  settings.shipMove = 2;
  settings.boostMove = 1;
  settings.planetHexes = {{0, 0}, {-4, 4}, {-3, 0}, {-2, 0}, {-2, 3}, {-2, 4}, {-1, 3}, {0, -4},
                          {0, 4}, {1, -3}, {2, -4}, {2, -3}, {2, 0},  {3, 0},  {4, -4}};
  return settings;
}

State afterSetup(int players, int first) {
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

State cornersAtMain() {
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

int firstShipOf(const State &state, int player) {
  for (std::size_t i = 0; i < state.ships.size(); ++i)
    if (state.ships[i].owner == player)
      return static_cast<int>(i);
  ADD_FAILURE() << "no ship of player " << player;
  return 0;
}

int shipsOwned(const State &state, int player) {
  return static_cast<int>(std::count_if(state.ships.begin(), state.ships.end(),
                                        [&](const Ship &ship) { return ship.owner == player; }));
}

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

int planetsAt(const State &state, Corner corner) {
  int count = 0;
  for (Hex hex : HexGrid::hexesTouching(corner))
    for (const Planet &planet : state.planets)
      count += planet.hex == hex;
  return count;
}

bool touchesHex(Corner corner, Hex hex) {
  const auto hexes = HexGrid::hexesTouching(corner);
  return std::find(hexes.begin(), hexes.end(), hex) != hexes.end();
}

bool clearOfStations(const State &state, Corner corner) {
  for (const Station &station : state.stations) {
    if (station.at == corner)
      return false;
    for (Corner next : HexGrid::neighbours(corner))
      if (station.at == next)
        return false;
  }
  return true;
}

} // namespace CosmicHex::TestGames
