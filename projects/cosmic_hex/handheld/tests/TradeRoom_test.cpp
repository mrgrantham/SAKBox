// The trade room against the rules: a bank trade (ending the turn's offers
// to players first, while they're open) and an offer to a player are moves
// the rules take; an offer to you is shown to answer, its Accept the rules'
#include <CosmicHex/Handheld/GameWords.hpp>
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

// The banner's words for an offer to you: who, in capitals, and the deal
TEST(TradeRoomTest, AnOfferToYouIsAnnounced) {
  State state = tradingFor(1);
  ASSERT_EQ(state.phase, Phase::Trading);
  state.player[1].resources = {0, 0, 2, 0, 0};
  state.player[0].resources = {0, 1, 0, 0, 0};
  Resources give{}, get{};
  give[2] = 2;
  get[1] = 1;
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, bundleOf(give), bundleOf(get), 0}));
  const PlayerNames names = PlayerNames::forGame(4, 0, "James", 7);
  TradeRoom room;
  room.setNames(names);
  room.update(state, Game::legalActions(state), true);
  ASSERT_TRUE(room.answering());
  EXPECT_FALSE(room.counterToYours());
  EXPECT_EQ(room.headline(), names.upperOf(1) + " OFFERS A TRADE");
  EXPECT_EQ(room.offerWords(), "2 " + std::string(GameWords::resource(2)) + " for your 1 " +
                                   std::string(GameWords::resource(1)));
}

// A counter to your offer is one even when it asks for just what you
// offered (regression: the screen took a counter that changed nothing for an
// offer, as it went by the kinds changed)
TEST(TradeRoomTest, ACounterIsSaidToBeOne) {
  State state = tradingFor(0);
  ASSERT_EQ(state.phase, Phase::Trading);
  state.player[0].resources = {1, 0, 0, 0, 0};
  state.player[1].resources = {0, 1, 0, 0, 0};
  Resources mine{}, theirs{};
  mine[0] = 1;
  theirs[1] = 1;
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, bundleOf(mine), bundleOf(theirs), 1}));
  // (they counter with the same deal from their side)
  ASSERT_TRUE(Game::apply(state, {Kind::CounterTrade, bundleOf(theirs), bundleOf(mine)}));
  ASSERT_EQ(state.phase, Phase::TradeCounter);
  const PlayerNames names = PlayerNames::forGame(4, 0, "James", 7);
  TradeRoom room;
  room.setNames(names);
  room.update(state, Game::legalActions(state), true);
  ASSERT_TRUE(room.answering());
  EXPECT_TRUE(room.changed().empty());
  EXPECT_TRUE(room.counterToYours());
  EXPECT_EQ(room.from(), 1);
  EXPECT_EQ(room.headline(), names.upperOf(1) + " COUNTERS");
}

// Countering an offer to you: the builder starts from the offer turned
// round, a change makes it yours, and it's sent as a counter-offer the
// rules take; Back first returns to the offer as it was
TEST(TradeRoomTest, AnOfferToYouCanBeCountered) {
  State state = tradingFor(1);
  ASSERT_EQ(state.phase, Phase::Trading);
  state.player[1].resources = {0, 0, 2, 0, 0};
  state.player[0].resources = {0, 2, 0, 0, 0};
  Resources give{}, get{};
  give[2] = 1; // (they give a fuel)
  get[1] = 1;  // (for a food)
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, bundleOf(give), bundleOf(get), 0}));
  TradeRoom room;
  room.update(state, Game::legalActions(state), true);
  ASSERT_TRUE(room.canCounter());
  room.startCountering();
  EXPECT_TRUE(room.countering());
  EXPECT_FALSE(room.showingOffer());
  EXPECT_TRUE(room.answering()); // (still an answer to make: its banner isn't shown again)
  EXPECT_EQ(room.partner(), 1);
  EXPECT_EQ(room.give(), get); // (you'd give what they want)
  EXPECT_EQ(room.get(), give); // (and get what they give)
  room.stopCountering();
  EXPECT_TRUE(room.showingOffer());
  room.startCountering();
  room.add(Side::Get, Resource::Fuel); // (two fuel, not one)
  ASSERT_TRUE(room.whyNot().empty()) << room.whyNot();
  const std::vector<Action> moves = room.moves();
  ASSERT_EQ(moves.size(), 1u);
  EXPECT_EQ(moves[0].kind, Kind::CounterTrade);
  EXPECT_EQ(resourcesOf(moves[0].b)[2], 2);
  ASSERT_TRUE(Game::apply(state, moves[0]));
  EXPECT_EQ(state.phase, Phase::TradeCounter);
  // (the offerer's to answer now: countering's over)
  room.update(state, Game::legalActions(state), false);
  EXPECT_FALSE(room.countering());
  EXPECT_FALSE(room.answering());
}

// A counter to your offer can only be accepted or declined: no Counter
TEST(TradeRoomTest, ACounterToYoursCantBeCountered) {
  State state = tradingFor(0);
  state.player[0].resources = {1, 0, 0, 0, 0};
  state.player[1].resources = {0, 1, 0, 0, 0};
  ASSERT_TRUE(Game::apply(
      state, {Kind::OfferTrade, bundleOf({1, 0, 0, 0, 0}), bundleOf({0, 1, 0, 0, 0}), 1}));
  ASSERT_TRUE(Game::apply(
      state, {Kind::CounterTrade, bundleOf({0, 1, 0, 0, 0}), bundleOf({2, 0, 0, 0, 0})}));
  TradeRoom room;
  room.update(state, Game::legalActions(state), true);
  ASSERT_TRUE(room.answering());
  EXPECT_FALSE(room.canCounter());
  room.startCountering();
  EXPECT_FALSE(room.countering());
}
