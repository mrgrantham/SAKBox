#include "../src/GameReplay.hpp"

#include "../../simulate/Batch.hpp"

#include <gtest/gtest.h>

namespace CosmicHex::Lab {
namespace {

// A short bot game with corners and edges (bots thinking little, so it's quick)
Batch::GameResult aGame() {
  Batch::Options options;
  options.layout = "R2";
  options.layoutsFile = "projects/cosmic_hex/layouts/layouts.txt";
  EXPECT_FALSE(Batch::readLayout(options).has_value());
  options.edges = true;
  options.iterations = 10;
  options.maxRounds = 12;
  const Batch::Bots bots(options);
  return Batch::playGame(options, 0, bots);
}

TEST(GameReplayTest, KeepsEveryStateOfAGame) {
  const Batch::GameResult game = aGame();
  const GameReplay replay = GameReplay::of(game.log);
  EXPECT_TRUE(replay.problem.empty()) << replay.problem;
  ASSERT_EQ(replay.actions.size(), game.log.actions.size());
  ASSERT_EQ(replay.states.size(), replay.actions.size() + 1);
  EXPECT_EQ(replay.states.front(), game.log.start);
  EXPECT_EQ(replay.states.back().winner, game.winner);
  // Points through the game, ending with the game's
  for (int p = 0; p < game.log.start.players; ++p) {
    const auto &points = replay.points[static_cast<std::size_t>(p)];
    ASSERT_EQ(points.size(), replay.states.size());
    EXPECT_EQ(points.back(), game.points[static_cast<std::size_t>(p)]);
  }
  // Every action has a sentence
  for (std::size_t i = 0; i < replay.actions.size(); ++i)
    EXPECT_FALSE(describe(replay.states[i], replay.actions[i]).empty());
}

TEST(GameReplayTest, StepsATurnAtATime) {
  const GameReplay replay = GameReplay::of(aGame().log);
  // Forwards from the start to the end, a turn at a time: every stop a new turn
  std::size_t step = 0;
  int turns = 0;
  while (step < replay.last()) {
    const std::size_t next = replay.nextTurn(step);
    ASSERT_GT(next, step);
    if (next < replay.last()) {
      const bool newPlayer = replay.states[next].turn != replay.states[next - 1].turn;
      const bool ended = replay.actions[next - 1].kind == Action::Kind::EndTurn;
      EXPECT_TRUE(newPlayer || ended) << "step " << next;
    }
    step = next;
    ++turns;
  }
  EXPECT_GT(turns, 8);
  // And back: from a turn's start to the one before
  const std::size_t second = replay.nextTurn(replay.nextTurn(0));
  EXPECT_EQ(replay.previousTurn(second), replay.nextTurn(0));
  EXPECT_EQ(replay.previousTurn(0), 0u);
}

TEST(GameReplayTest, ReadsALogFile) {
  const Batch::GameResult game = aGame();
  const auto replay = GameReplay::fromText(ActionLog::write(game.log));
  ASSERT_TRUE(replay.has_value()) << replay.error();
  EXPECT_EQ(replay->states.back().winner, game.winner);
  EXPECT_FALSE(GameReplay::fromText("not a log").has_value());
}

TEST(GameReplayTest, DescribesActions) {
  State state = newGame(4);
  state.phase = Phase::RollDraw;
  EXPECT_EQ(describe(state, {Action::Kind::Roll, 3, 4}), "Roll: 3 + 4 = 7: the blockade moves.");
  state.phase = Phase::Main;
  state.turn = 1;
  EXPECT_EQ(describe(state, {Action::Kind::Trade, 0, 4}),
            "Player 2 trades minerals for 1 crystals.");
  state.phase = Phase::PlanetDraw;
  EXPECT_EQ(describe(state, {Action::Kind::RevealPlanet, 1, 8}),
            "A planet turns over: lush planet, on 8.");
}

} // namespace
} // namespace CosmicHex::Lab
