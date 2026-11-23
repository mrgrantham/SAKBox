// Trading (Trading.cpp): offers between players (Settings::playerTrading)
// to anyone in turn or to one player, accepted, declined or countered, and
// offers that can't be paid; then the bank's rate, and the alien outposts'
// better rates (a ship by one, or better, a station).
#include <CosmicHex/Rules.hpp>

#include "TestGames.hpp"

#include <HexGrid/CornersAndEdges.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::TestGames;
using Kind = Action::Kind;

namespace {

const int kOneMineral = bundleOf({1, 0, 0, 0, 0});
const int kOneFood = bundleOf({0, 1, 0, 0, 0});
const int kOneFuel = bundleOf({0, 0, 1, 0, 0});

// Four players at player 0's trading step, each holding 2 of everything
State fourTrading() {
  State state = atTrading(4);
  for (Player &player : state.player)
    player.resources = {2, 2, 2, 2, 2};
  return state;
}

} // namespace

// An offer to anyone is answered by each other player in turn from the
// next; declined by all, it's over and nothing changes hands
TEST(TradingTest, AnOfferToAnyoneGoesRoundInTurn) {
  State state = fourTrading();
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, kOneMineral, kOneFood, -1}));
  EXPECT_EQ(state.phase, Phase::TradeReply);
  EXPECT_EQ(state.offersMade, (std::vector<std::array<int, 2>>{{kOneMineral, kOneFood}}));
  for (int replier : {1, 2, 3}) {
    EXPECT_EQ(Game::currentPlayer(state), replier);
    EXPECT_EQ(Game::legalActions(state),
              (std::vector<Action>{{Kind::AcceptTrade}, {Kind::DeclineTrade}}));
    ASSERT_TRUE(Game::apply(state, {Kind::DeclineTrade}));
  }
  EXPECT_EQ(state.phase, Phase::Trading);
  EXPECT_EQ(state.offer, TradeOffer{});
  for (int p = 0; p < 4; ++p)
    EXPECT_EQ(state.player[static_cast<std::size_t>(p)].resources, (Resources{2, 2, 2, 2, 2}));
}

// An offer to one player is theirs alone to answer; accepted, the
// resources change hands; declined, it's over
TEST(TradingTest, AnOfferToOnePlayer) {
  State state = fourTrading();
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, bundleOf({2, 0, 0, 0, 0}), kOneFood, 2}));
  EXPECT_EQ(Game::currentPlayer(state), 2);
  ASSERT_TRUE(Game::apply(state, {Kind::AcceptTrade}));
  EXPECT_EQ(state.phase, Phase::Trading);
  EXPECT_EQ(state.player[0].resources, (Resources{0, 3, 2, 2, 2}));
  EXPECT_EQ(state.player[2].resources, (Resources{4, 1, 2, 2, 2}));
  // Another offer (no limit), declined: over at once
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, kOneFood, kOneFuel, 3}));
  ASSERT_TRUE(Game::apply(state, {Kind::DeclineTrade}));
  EXPECT_EQ(state.phase, Phase::Trading);
  EXPECT_EQ(state.offersMade.size(), 2u);
}

// A counter-offer goes back to the offerer: accepted, its terms are traded;
// declined, the offer goes on to the next player (to anyone), or is over
// (to one)
TEST(TradingTest, CounterOffers) {
  State state = fourTrading();
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, kOneMineral, kOneFood, -1}));
  // Player 1 would give a fuel (not a food) for the mineral
  ASSERT_TRUE(Game::apply(state, {Kind::CounterTrade, kOneFuel, kOneMineral}));
  EXPECT_EQ(state.phase, Phase::TradeCounter);
  EXPECT_EQ(Game::currentPlayer(state), 0);
  ASSERT_TRUE(Game::apply(state, {Kind::DeclineTrade}));
  EXPECT_EQ(state.phase, Phase::TradeReply);
  EXPECT_EQ(Game::currentPlayer(state), 2);
  EXPECT_EQ(state.offer.counterGive, 0);
  ASSERT_TRUE(Game::apply(state, {Kind::CounterTrade, kOneFuel, kOneMineral}));
  ASSERT_TRUE(Game::apply(state, {Kind::AcceptTrade}));
  EXPECT_EQ(state.phase, Phase::Trading);
  EXPECT_EQ(state.player[0].resources, (Resources{1, 2, 3, 2, 2}));
  EXPECT_EQ(state.player[2].resources, (Resources{3, 2, 1, 2, 2}));
  EXPECT_EQ(state.player[1].resources, (Resources{2, 2, 2, 2, 2}));
  // To one player, a counter declined ends it
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, kOneMineral, kOneFood, 3}));
  ASSERT_TRUE(Game::apply(state, {Kind::CounterTrade, kOneFuel, kOneMineral}));
  ASSERT_TRUE(Game::apply(state, {Kind::DeclineTrade}));
  EXPECT_EQ(state.phase, Phase::Trading);
}

// Offers that can't be paid, or aren't offers, are refused: giving what the
// offerer hasn't, nothing for something, to themselves or nobody
TEST(TradingTest, OffersThatCantBeMade) {
  State state = fourTrading();
  const int threeMinerals = bundleOf({3, 0, 0, 0, 0});
  for (const Action &offer :
       {Action{Kind::OfferTrade, threeMinerals, kOneFood, -1},
        Action{Kind::OfferTrade, 0, kOneFood, -1}, Action{Kind::OfferTrade, kOneMineral, 0, -1},
        Action{Kind::OfferTrade, kOneMineral, kOneFood, 0},
        Action{Kind::OfferTrade, kOneMineral, kOneFood, 4},
        Action{Kind::OfferTrade, kOneMineral, kOneFood, -2}})
    EXPECT_FALSE(Game::apply(state, offer)) << offer.a << " " << offer.b << " " << offer.c;
  EXPECT_EQ(state.phase, Phase::Trading);
}

// An answer that can't be paid is refused: accepting without what's asked,
// countering with what the answerer hasn't, the offerer accepting a counter
// they can't pay
TEST(TradingTest, AnswersThatCantBePaid) {
  State state = fourTrading();
  state.player[1].resources = {0, 0, 1, 0, 0}; // (no food)
  ASSERT_TRUE(Game::apply(state, {Kind::OfferTrade, kOneMineral, kOneFood, 1}));
  EXPECT_EQ(Game::legalActions(state), (std::vector<Action>{{Kind::DeclineTrade}}));
  EXPECT_FALSE(Game::apply(state, {Kind::AcceptTrade}));
  EXPECT_FALSE(Game::apply(state, {Kind::CounterTrade, kOneFood, kOneMineral})); // (no food)
  EXPECT_FALSE(Game::apply(state, {Kind::CounterTrade, kOneFuel, 0}));           // (for nothing)
  // A counter asking for 3 crystals, which the offerer hasn't
  ASSERT_TRUE(Game::apply(state, {Kind::CounterTrade, kOneFuel, bundleOf({0, 0, 0, 0, 3})}));
  EXPECT_EQ(Game::legalActions(state), (std::vector<Action>{{Kind::DeclineTrade}}));
  EXPECT_FALSE(Game::apply(state, {Kind::AcceptTrade}));
}

// Trading comes after the roll and before the rest of the turn: only offers
// and ending it; once ended, no more offers this turn; the next turn, again
TEST(TradingTest, TradingComesFirst) {
  State state = fourTrading();
  EXPECT_EQ(Game::legalActions(state), (std::vector<Action>{{Kind::EndTrading}}));
  for (const Action &action : {Action{Kind::Trade, 0, 1}, Action{Kind::BuyContract},
                               Action{Kind::EndTurn}, Action{Kind::Settle, 0}})
    EXPECT_FALSE(Game::apply(state, action));
  ASSERT_TRUE(Game::apply(state, {Kind::EndTrading}));
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_FALSE(Game::apply(state, {Kind::OfferTrade, kOneMineral, kOneFood, -1}));
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 1, 2}));
  EXPECT_EQ(state.phase, Phase::Trading);
  EXPECT_TRUE(state.offersMade.empty());
}

// On a 7 the blockade comes first, then trading
TEST(TradingTest, OnASevenTheBlockadeComesFirst) {
  Settings settings;
  settings.playerTrading = true;
  State state = newGame(3, settings);
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 3, 4}));
  EXPECT_EQ(state.phase, Phase::Blockade);
  ASSERT_TRUE(Game::apply(state, {Kind::MoveBlockade, -1, -1}));
  EXPECT_EQ(state.phase, Phase::Trading);
}

// The bank: bankRate of one resource for one of another, offered only for
// what's held at that rate
TEST(TradingTest, TheBanksRate) {
  State state = atMain();
  state.player[0].resources = {3, 0, 0, 0, 0};
  EXPECT_EQ(tradeRateOf(state, 0), 4);
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::Trade));
  EXPECT_FALSE(Game::apply(state, {Kind::Trade, 0, 1}));
  state.settings.bankRate = 3;
  EXPECT_EQ(tradeRateOf(state, 0), 3);
  std::vector<Action> trades;
  for (const Action &action : Game::legalActions(state))
    if (action.kind == Kind::Trade)
      trades.push_back(action);
  EXPECT_EQ(
      trades,
      (std::vector<Action>{
          {Kind::Trade, 0, 1}, {Kind::Trade, 0, 2}, {Kind::Trade, 0, 3}, {Kind::Trade, 0, 4}}));
  EXPECT_FALSE(Game::apply(state, {Kind::Trade, 0, 0})); // (for the same)
  ASSERT_TRUE(Game::apply(state, {Kind::Trade, 0, 4}));
  EXPECT_EQ(state.player[0].resources, (Resources{0, 0, 0, 0, 1}));
}

// On hexes the alien station trades at alienRate with a station or ship a
// hex from it, once it's turned over; never worse than the bank
TEST(TradingTest, TheAlienStationOnHexes) {
  State state = atMain();
  const int site = siteIndex(state, Hex{2, -2});
  ASSERT_GE(site, 0);
  state.zone[static_cast<std::size_t>(site)].find = Find::AlienStation;
  state.ships[0].at = Hex{2, -1};
  EXPECT_EQ(tradeRateOf(state, 0), 4); // (face down)
  state.zone[static_cast<std::size_t>(site)].flipped = true;
  EXPECT_EQ(tradeRateOf(state, 0), 3);
  EXPECT_EQ(tradeRateOf(state, 1), 4); // (not by it)
  state.settings.alienRate = 5;
  EXPECT_EQ(tradeRateOf(state, 0), 4);
  // A station a hex away, the ship gone
  state.settings.alienRate = 3;
  state.ships[0].at = Hex{4, -2};
  const int planet = planetIndex(state, Hex{2, -1});
  flip(state, planet, PlanetKind::GasGiant, 8);
  dock(state, planet, 0);
  EXPECT_EQ(tradeRateOf(state, 0), 3);
}

// On corners an alien outpost trades at alienRate with a ship on one of its
// corners, alienStationRate with a station, the better of the two
TEST(TradingTest, AlienOutpostsOnCorners) {
  State state = cornersAtMain();
  ZoneSite &outpost = state.zone.front();
  outpost.find = Find::AlienStation;
  const int mine = firstShipOf(state, 0);
  state.ships[static_cast<std::size_t>(mine)].corner = HexGrid::corner(outpost.hex, 0);
  EXPECT_EQ(tradeRateOf(state, 0), 4); // (face down)
  outpost.flipped = true;
  EXPECT_EQ(tradeRateOf(state, 0), 3);
  EXPECT_EQ(tradeRateOf(state, 1), 4);
  state.stations.push_back(Station{HexGrid::corner(outpost.hex, 3), 0, false});
  EXPECT_EQ(tradeRateOf(state, 0), 2);
  state.player[0].resources = {2, 0, 0, 0, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::Trade, 0, 2}));
  EXPECT_EQ(state.player[0].resources, (Resources{0, 0, 1, 0, 0}));
  // Another player's station there gives player 0 nothing
  state.stations.back().owner = 1;
  EXPECT_EQ(tradeRateOf(state, 0), 3);
  EXPECT_EQ(tradeRateOf(state, 1), 2);
}
