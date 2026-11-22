// The bots' trading rule (TradingRule.hpp): what they offer, what they take
#include <CosmicHex/TradingRule.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using Kind = Action::Kind;

namespace {
// Three players, player 1's trading step; nobody can build anything
State trading() {
  Settings settings;
  settings.playerTrading = true;
  State state = newGame(3, settings);
  EXPECT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  EXPECT_TRUE(Game::apply(state, {Kind::Roll, 1, 2}));
  EXPECT_EQ(state.phase, Phase::Trading);
  for (Player &player : state.player)
    player.resources = {0, 0, 0, 0, 0};
  // No ships (so a station isn't a build to want): a ship is the next build
  state.ships.clear();
  return state;
}
} // namespace

// A player one resource short of a build offers their spare for it, one
// for one, then two for one, each once; then stops
TEST(TradingRuleTest, OffersSpareForWhatsShort) {
  State state = trading();
  // A ship costs a mineral and a fuel: three minerals, no fuel
  state.player[0].resources = {3, 0, 0, 0, 0};
  const TradeNeeds needs = tradeNeedsOf(state, 0);
  ASSERT_TRUE(needs.wantsSomething);
  const Action first = tradingRuleMove(state);
  ASSERT_EQ(first.kind, Kind::OfferTrade);
  EXPECT_EQ(resourcesOf(first.b)[2], 1); // a fuel
  ASSERT_TRUE(Game::apply(state, first));
  ASSERT_TRUE(Game::apply(state, {Kind::DeclineTrade}));
  ASSERT_TRUE(Game::apply(state, {Kind::DeclineTrade}));
  const Action second = tradingRuleMove(state);
  ASSERT_EQ(second.kind, Kind::OfferTrade);
  EXPECT_NE(second.a, first.a); // (two for one now)
  ASSERT_TRUE(Game::apply(state, second));
  ASSERT_TRUE(Game::apply(state, {Kind::DeclineTrade}));
  ASSERT_TRUE(Game::apply(state, {Kind::DeclineTrade}));
  EXPECT_EQ(tradingRuleMove(state).kind, Kind::EndTrading);
}

// Another accepts what brings their build closer and costs only their spare,
// but not from someone within two points of winning
TEST(TradingRuleTest, AcceptsWhatHelpsButNotTheNearlyWon) {
  State state = trading();
  state.player[0].resources = {0, 0, 1, 0, 0}; // a fuel to give
  state.player[1].resources = {1, 0, 0, 0, 3}; // a mineral, short of fuel; crystals spare
  ASSERT_TRUE(Game::apply(
      state, {Kind::OfferTrade, bundleOf({0, 0, 1, 0, 0}), bundleOf({0, 0, 0, 0, 1}), 1}));
  EXPECT_EQ(tradingRuleMove(state).kind, Kind::AcceptTrade);
  // Asked for what they need themselves: declined
  State asked = state;
  asked.offer.get = bundleOf({1, 0, 0, 0, 0});
  EXPECT_EQ(tradingRuleMove(asked).kind, Kind::DeclineTrade);
  // The offerer nearly won: declined
  state.settings.winPoints = points(state, 0) + 1;
  EXPECT_EQ(tradingRuleMove(state).kind, Kind::DeclineTrade);
}

// An offer from a person that doesn't help: countered with one of what
// they're short of for one of what the offer asked (between bots, declined);
// the counter is one the rules take, and the offerer's answer to it legal
TEST(TradingRuleTest, CountersAPersonsOfferItWontTake) {
  State state = trading();
  state.player[0].resources = {0, 0, 1, 1, 0}; // a fuel and a water
  state.player[1].resources = {1, 0, 0, 0, 3}; // a mineral, short of fuel; crystals spare
  // (a water, which doesn't help, for a crystal)
  ASSERT_TRUE(Game::apply(
      state, {Kind::OfferTrade, bundleOf({0, 0, 0, 1, 0}), bundleOf({0, 0, 0, 0, 1}), 1}));
  EXPECT_EQ(tradingRuleMove(state).kind, Kind::DeclineTrade); // (between bots)
  const Action counter = tradingRuleMove(state, true);
  ASSERT_EQ(counter.kind, Kind::CounterTrade);
  EXPECT_EQ(resourcesOf(counter.a), (Resources{0, 0, 0, 0, 1})); // a crystal from them
  EXPECT_EQ(resourcesOf(counter.b), (Resources{0, 0, 1, 0, 0})); // for a fuel
  ASSERT_TRUE(Game::apply(state, counter));
  EXPECT_EQ(state.phase, Phase::TradeCounter);
  EXPECT_FALSE(Game::legalActions(state).empty());
}

// No counter when there's nothing to ask for that the offerer holds, nor
// for someone within two points of winning
TEST(TradingRuleTest, NoCounterWithoutADealToMake) {
  State state = trading();
  state.player[0].resources = {0, 0, 0, 1, 0}; // no fuel to give
  state.player[1].resources = {1, 0, 0, 0, 3};
  ASSERT_TRUE(Game::apply(
      state, {Kind::OfferTrade, bundleOf({0, 0, 0, 1, 0}), bundleOf({0, 0, 0, 0, 1}), 1}));
  EXPECT_EQ(tradingRuleMove(state, true).kind, Kind::DeclineTrade);
  state.player[0].resources = {0, 0, 1, 1, 0};
  ASSERT_EQ(tradingRuleMove(state, true).kind, Kind::CounterTrade);
  state.settings.winPoints = points(state, 0) + 1;
  EXPECT_EQ(tradingRuleMove(state, true).kind, Kind::DeclineTrade);
}

// A free build is nothing to trade for: the next
// build the rule looks at is one that costs something
TEST(TradingRuleTest, AFreeBuildIsNothingToTradeFor) {
  State state = trading();
  state.settings.stationCost = {0, 0, 0, 0, 0};
  state.settings.shipCost = {1, 1, 1, 1, 0};
  state.ships.push_back(Ship{0, Hex{}, false, boardCorners()[0]}); // (a ship: a station's free)
  state.player[0].resources = {1, 1, 1, 0, 0};                     // a ship but for water
  const TradeNeeds needs = tradeNeedsOf(state, 0);
  ASSERT_TRUE(needs.wantsSomething);
  EXPECT_EQ(needs.shortfall, (Resources{0, 0, 0, 1, 0}));
}
