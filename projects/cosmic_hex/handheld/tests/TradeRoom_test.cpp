// The trade room against the rules: a bank trade (ending the turn's offers
// to players first, while they're open) and an offer to a player are moves
// the rules take; an offer to you is shown to answer, its Accept the rules'
#include <CosmicHex/Handheld/TradeRoom.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Kind = Action::Kind;
using Side = TradeRoom::Side;

namespace {
// A game played (first moves) until player `who` is trading with players
State tradingFor(int who) {
  State state = newGame(4, agreedRules(boardR3(), 4), 0);
  SeededRandom::Generator random(41);
  for (int guard = 0; guard < 4000; ++guard) {
    if (state.phase == Phase::Trading && state.turn == who && state.round > 2)
      break;
    Game::apply(state, Game::currentPlayer(state) == GameBots::kDealer
                           ? Game::dealerAction(state, random)
                           : Game::legalActions(state).front());
  }
  return state;
}
} // namespace

TEST(TradeRoomTest, ABankTradeEndsOffersToPlayersFirst) {
  State state = tradingFor(0);
  ASSERT_EQ(state.phase, Phase::Trading);
  // (enough of one kind for the bank)
  state.player[0].resources = {6, 0, 0, 0, 0};
  TradeRoom room;
  room.update(state, Game::legalActions(state), true);
  EXPECT_TRUE(room.open(TradeRoom::kBank));
  room.add(Side::Give, Resource::Minerals);
  room.add(Side::Get, Resource::Food);
  EXPECT_EQ(room.give()[0], tradeRateOf(state, 0));
  EXPECT_TRUE(room.whyNot().empty()) << room.whyNot();
  const std::vector<Action> moves = room.moves();
  ASSERT_EQ(moves.size(), 2u);
  EXPECT_EQ(moves[0].kind, Kind::EndTrading);
  for (const Action &move : moves)
    EXPECT_TRUE(Game::apply(state, move));
  EXPECT_EQ(state.player[0].resources[1], 1);
  // Now the players are closed for the turn
  room.update(state, Game::legalActions(state), true);
  EXPECT_FALSE(room.open(1));
}

TEST(TradeRoomTest, AnOfferToAPlayerIsTheRulesOwn) {
  State state = tradingFor(0);
  ASSERT_EQ(state.phase, Phase::Trading);
  state.player[0].resources = {2, 1, 0, 0, 0};
  state.player[1].resources = {0, 0, 2, 1, 0};
  TradeRoom room;
  room.update(state, Game::legalActions(state), true);
  room.choosePartner(1);
  EXPECT_TRUE(room.open(1));
  room.add(Side::Give, Resource::Minerals);
  room.add(Side::Give, Resource::Minerals);
  room.add(Side::Give, Resource::Minerals); // (only 2 held: stays 2)
  room.add(Side::Get, Resource::Fuel);
  EXPECT_EQ(room.give()[0], 2);
  const std::vector<Action> moves = room.moves();
  ASSERT_EQ(moves.size(), 1u);
  EXPECT_EQ(moves[0].kind, Kind::OfferTrade);
  EXPECT_EQ(moves[0].c, 1);
  EXPECT_TRUE(Game::apply(state, moves[0]));
  EXPECT_EQ(state.phase, Phase::TradeReply);
  EXPECT_EQ(state.offer.replier, 1);
}

TEST(TradeRoomTest, AnOfferToYouIsAnswered) {
  State state = tradingFor(1);
  ASSERT_EQ(state.phase, Phase::Trading);
  state.player[1].resources = {0, 0, 2, 0, 0};
  state.player[0].resources = {0, 1, 0, 0, 0};
  Resources give{}, get{};
  give[2] = 1;
  get[1] = 1;
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, bundleOf(give), bundleOf(get), 0}));
  TradeRoom room;
  room.update(state, Game::legalActions(state), true);
  EXPECT_TRUE(room.answering());
  EXPECT_EQ(room.from(), 1);
  EXPECT_EQ(room.theyGive()[2], 1);
  EXPECT_EQ(room.theyWant()[1], 1);
  ASSERT_TRUE(room.accept().has_value());
  EXPECT_TRUE(Game::apply(state, *room.accept()));
  EXPECT_EQ(state.player[0].resources[2], 1);
}
