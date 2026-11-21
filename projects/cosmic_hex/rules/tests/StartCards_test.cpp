// A starting hand dealt at random (Settings::startCards): the dealer deals
// it one card at a time round the table from the first player, before the
// setup; the log's text keeps it; without it, the start is as before
#include <CosmicHex/Rules.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <numeric>

using namespace CosmicHex;
using Kind = Action::Kind;

namespace {
int cardsOf(const State &state, int player) {
  const Resources &r = state.player[static_cast<std::size_t>(player)].resources;
  return std::accumulate(r.begin(), r.end(), 0);
}
} // namespace

// The deal comes first, the dealer's, round the table from the first player;
// then the setup begins as it would have (the agreed rules: placing)
TEST(StartCardsTest, TheDealerDealsRoundTheTableFirst) {
  Settings settings = agreedRules(boardR3());
  settings.startCards = 3;
  State state = newGame(4, settings, 2);
  EXPECT_EQ(state.phase, Phase::StartCardsDraw);
  EXPECT_EQ(Game::currentPlayer(state), GameBots::kDealer);
  for (int p = 0; p < 4; ++p)
    EXPECT_EQ(cardsOf(state, p), 0);
  // (the first card to the first player, 2; the next to 3)
  ASSERT_TRUE(Game::apply(state, {Kind::DealStartCard, 4}));
  EXPECT_EQ(state.player[2].resources[4], 1);
  ASSERT_TRUE(Game::apply(state, {Kind::DealStartCard, 0}));
  EXPECT_EQ(state.player[3].resources[0], 1);
  EXPECT_FALSE(Game::apply(state, {Kind::EndTurn}));          // (not a deal)
  EXPECT_FALSE(Game::apply(state, {Kind::DealStartCard, 5})); // (no such resource)
  SeededRandom::Generator random(7);
  while (state.phase == Phase::StartCardsDraw)
    ASSERT_TRUE(Game::apply(state, Game::dealerAction(state, random)));
  for (int p = 0; p < 4; ++p)
    EXPECT_EQ(cardsOf(state, p), 3);
  const State usual = newGame(4, agreedRules(boardR3()), 2);
  EXPECT_EQ(state.phase, usual.phase);
  EXPECT_EQ(state.turn, usual.turn);
}

// Without corners, the deal leads to the first player's home income
TEST(StartCardsTest, ThenTheFirstTurnWithoutCorners) {
  Settings settings;
  settings.startCards = 2;
  State state = newGame(2, settings, 1);
  SeededRandom::Generator random(3);
  while (state.phase == Phase::StartCardsDraw)
    ASSERT_TRUE(Game::apply(state, Game::dealerAction(state, random)));
  const State usual = newGame(2, Settings{}, 1);
  EXPECT_EQ(state.phase, usual.phase);
  EXPECT_EQ(state.turn, 1);
  EXPECT_EQ(cardsOf(state, 0), 2);
}

// The starting state and the deal's actions read back from their text (an
// action log); a game without a dealt hand writes as it always has
TEST(StartCardsTest, TheLogKeepsIt) {
  Settings settings = agreedRules(boardR3());
  settings.startCards = 3;
  const State start = newGame(4, settings);
  const auto read = Game::readState(Game::writeState(start));
  ASSERT_TRUE(read.has_value());
  // (a log's promise: what's read back writes out the same)
  EXPECT_EQ(Game::writeState(*read), Game::writeState(start));
  EXPECT_EQ(read->phase, Phase::StartCardsDraw);
  EXPECT_EQ(read->startCardsLeft, 12);
  const Action deal{Kind::DealStartCard, 3};
  EXPECT_EQ(Game::readAction(Game::writeAction(deal)), deal);
  const std::string plain = Game::writeState(newGame(4, agreedRules(boardR3())));
  EXPECT_EQ(plain.find("startcards"), std::string::npos);
  EXPECT_EQ(plain.find("dealing"), std::string::npos);
}
