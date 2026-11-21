// A new ship moves off its colony the turn it's built (Settings::
// newShipsLeave): by the usual move; the turn can't end while it still
// stands on the colony with somewhere to go; without the setting, it waits
// for the next turn as before
#include <CosmicHex/Rules.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>

using namespace CosmicHex;
using Kind = Action::Kind;

namespace {
bool has(const std::vector<Action> &actions, Kind kind) {
  return std::any_of(actions.begin(), actions.end(),
                     [&](const Action &a) { return a.kind == kind; });
}
// A game played on (first legal moves) until `player` can build a ship
State untilShipCanBeBuilt(const Settings &settings) {
  State state = newGame(4, settings);
  SeededRandom::Generator random(5);
  for (int guard = 0; guard < 20000 && state.winner < 0; ++guard) {
    if (Game::currentPlayer(state) == GameBots::kDealer) {
      Game::apply(state, Game::dealerAction(state, random));
      continue;
    }
    const auto legal = Game::legalActions(state);
    if (has(legal, Kind::BuildShip))
      return state;
    // (end the turn when there's nothing else; else the first move)
    Game::apply(state, legal[guard % legal.size()]);
  }
  return state;
}
} // namespace

TEST(NewShipsLeaveTest, ANewShipMovesThisTurnByTheUsualMove) {
  Settings settings = agreedRules(boardR3());
  settings.newShipsLeave = true;
  State state = untilShipCanBeBuilt(settings);
  const auto legal = Game::legalActions(state);
  const auto build = std::find_if(legal.begin(), legal.end(),
                                  [](const Action &a) { return a.kind == Kind::BuildShip; });
  ASSERT_NE(build, legal.end());
  ASSERT_TRUE(Game::apply(state, *build));
  const auto ship = static_cast<int>(state.ships.size()) - 1;
  EXPECT_FALSE(state.ships.back().moved);
  // (the turn can't end yet; its moves are the usual ones)
  const auto now = Game::legalActions(state);
  EXPECT_FALSE(has(now, Kind::EndTurn));
  EXPECT_FALSE(Game::apply(state, Action{Kind::EndTurn}));
  const auto move = std::find_if(
      now.begin(), now.end(), [&](const Action &a) { return a.kind == Kind::Move && a.a == ship; });
  ASSERT_NE(move, now.end());
  ASSERT_TRUE(Game::apply(state, *move));
  // (the dealer turns over anything it landed by first)
  SeededRandom::Generator random(9);
  for (int guard = 0; guard < 20 && Game::currentPlayer(state) == GameBots::kDealer; ++guard)
    Game::apply(state, Game::dealerAction(state, random));
  if (state.phase == Phase::Main) {
    EXPECT_TRUE(has(Game::legalActions(state), Kind::EndTurn));
  }
}

TEST(NewShipsLeaveTest, OffANewShipWaitsAsBefore) {
  State state = untilShipCanBeBuilt(agreedRules(boardR3()));
  const auto legal = Game::legalActions(state);
  const auto build = std::find_if(legal.begin(), legal.end(),
                                  [](const Action &a) { return a.kind == Kind::BuildShip; });
  ASSERT_NE(build, legal.end());
  ASSERT_TRUE(Game::apply(state, *build));
  EXPECT_TRUE(state.ships.back().moved);
  EXPECT_TRUE(has(Game::legalActions(state), Kind::EndTurn));
}
