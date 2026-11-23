// Production (Production.cpp): what each roll pays (a station 1, a colony
// 2, from every planet a corner touches), a bank that never runs short, the
// blockade (no pay, a steal), the Imperial Tithe on a 7, the home's income
// and the home die, and the exhausted home (Settings::exhaustHomesAt).
#include <CosmicHex/Rules.hpp>

#include "TestGames.hpp"

#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using namespace CosmicHex;
using namespace CosmicHex::TestGames;
using Kind = Action::Kind;

namespace {

// A payout as {planet, player, resource, count}, to compare
std::vector<std::array<int, 4>> rows(const std::vector<Payout> &payouts) {
  std::vector<std::array<int, 4>> out;
  out.reserve(payouts.size());
  for (const Payout &pay : payouts)
    out.push_back({pay.planet, pay.player, static_cast<int>(pay.resource), pay.count});
  return out;
}

constexpr int kFuel = static_cast<int>(Resource::Fuel);

// Player 0's roll on hexes, with a gas giant numbered 8 at the centre
// holding player 0's station and player 1's colony
State aGasGiantShared() {
  State state = newGame(2);
  const int planet = planetIndex(state, Hex{0, 0});
  flip(state, planet, PlanetKind::GasGiant, 8);
  dock(state, planet, 0);
  dock(state, planet, 1, true);
  EXPECT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  return state;
}

} // namespace

// A roll pays each station on a turned-over planet showing it 1, a colony
// 2, in the planets' order; other numbers pay nothing
TEST(ProductionTest, ARollPaysStationsOneAndColoniesTwo) {
  State state = aGasGiantShared();
  const int planet = planetIndex(state, Hex{0, 0});
  EXPECT_EQ(rows(payouts(state, 8)),
            (std::vector<std::array<int, 4>>{{planet, 0, kFuel, 1}, {planet, 1, kFuel, 2}}));
  for (int roll : {2, 3, 4, 5, 6, 9, 10, 11, 12})
    EXPECT_TRUE(payouts(state, roll).empty()) << roll;
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 4, 4}));
  EXPECT_EQ(state.player[0].resources[of(Resource::Fuel)], 1 + 1);
  EXPECT_EQ(state.player[1].resources[of(Resource::Fuel)], 1 + 2);
}

// What a station and a colony are paid is a setting each
TEST(ProductionTest, ThePayIsASetting) {
  State state = aGasGiantShared();
  state.settings.stationPays = 3;
  state.settings.colonyPays = 5;
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 2, 6}));
  EXPECT_EQ(state.player[0].resources[of(Resource::Fuel)], 1 + 3);
  EXPECT_EQ(state.player[1].resources[of(Resource::Fuel)], 1 + 5);
}

// A face-down planet, a moon and a home pay nothing, whatever is rolled
TEST(ProductionTest, OnlyTurnedOverResourcePlanetsPay) {
  State state = newGame(2);
  const int faceDown = planetIndex(state, Hex{0, 0}), moon = planetIndex(state, Hex{3, 0});
  state.planets[static_cast<std::size_t>(faceDown)].number = 8; // (not yet turned over)
  dock(state, faceDown, 0);
  flip(state, moon, PlanetKind::Moon, 0);
  dock(state, moon, 0);
  for (int roll = 2; roll <= 12; ++roll)
    EXPECT_TRUE(payouts(state, roll).empty()) << roll;
}

// On corners a station is paid by every planet it touches showing the
// roll, a colony twice; a planet showing another number pays nothing
TEST(ProductionTest, ACornerIsPaidByEveryPlanetItTouches) {
  State state = cornersAtMain(); // (every planet a gas giant on 6)
  const Corner spot = cornerTouching(state, 2);
  state.stations.push_back(Station{spot, 1, false});
  const auto paidTo = [&](int roll) {
    int count = 0;
    for (const Payout &pay : payouts(state, roll))
      count += pay.player == 1 ? pay.count : 0;
    return count;
  };
  EXPECT_EQ(paidTo(6), 2);
  state.stations.back().colony = true;
  EXPECT_EQ(paidTo(6), 4);
  // One of the two on an 8
  for (Planet &planet : state.planets)
    if (touchesHex(spot, planet.hex)) {
      planet.number = 8;
      break;
    }
  EXPECT_EQ(paidTo(6), 2);
  EXPECT_EQ(paidTo(8), 2);
  // The home colony's home pays nothing by the dice
  for (const Payout &pay : payouts(state, 6))
    EXPECT_NE(state.planets[static_cast<std::size_t>(pay.planet)].kind, PlanetKind::Home);
}

// The bank never runs short: the rules keep no stock, and a roll pays in
// full however many resources are already out
TEST(ProductionTest, TheBankNeverRunsShort) {
  State state = aGasGiantShared();
  state.player[0].resources = {0, 0, 90, 0, 0};
  state.player[1].resources = {0, 0, 90, 0, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 3, 5}));
  EXPECT_EQ(state.player[0].resources[of(Resource::Fuel)], 91);
  EXPECT_EQ(state.player[1].resources[of(Resource::Fuel)], 92);
}

// The blockaded planet pays nothing; another showing the same number does
TEST(ProductionTest, TheBlockadedPlanetPaysNothing) {
  State state = aGasGiantShared();
  const int blockaded = planetIndex(state, Hex{0, 0}), other = planetIndex(state, Hex{3, 0});
  flip(state, other, PlanetKind::IceMoon, 8);
  dock(state, other, 0);
  state.blockade = blockaded;
  EXPECT_EQ(rows(payouts(state, 8)),
            (std::vector<std::array<int, 4>>{{other, 0, static_cast<int>(Resource::Water), 1}}));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 4, 4}));
  EXPECT_EQ(state.player[0].resources[of(Resource::Fuel)], 1);
  EXPECT_EQ(state.player[1].resources[of(Resource::Fuel)], 1);
  EXPECT_EQ(state.player[0].resources[of(Resource::Water)], 2);
}

// The blockade moves to another turned-over planet that isn't a home (not
// the one it's on, nor a face-down one)
TEST(ProductionTest, WhereTheBlockadeCanGo) {
  State state = newGame(2);
  const int here = planetIndex(state, Hex{0, 0}), there = planetIndex(state, Hex{3, 0});
  flip(state, here, PlanetKind::GasGiant, 8);
  flip(state, there, PlanetKind::IceMoon, 5);
  state.blockade = here;
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 3, 4}));
  ASSERT_EQ(state.phase, Phase::Blockade);
  EXPECT_EQ(Game::legalActions(state), (std::vector<Action>{{Kind::MoveBlockade, there, -1}}));
  const int home = planetIndex(state, Hex{-4, 2}), faceDown = planetIndex(state, Hex{0, 3});
  for (int to : {here, home, faceDown, -1})
    EXPECT_FALSE(Game::apply(state, {Kind::MoveBlockade, to, -1})) << to;
  ASSERT_TRUE(Game::apply(state, {Kind::MoveBlockade, there, -1}));
  EXPECT_EQ(state.blockade, there);
  EXPECT_EQ(state.phase, Phase::Main);
}

// Its mover steals from a player with a station there and something to
// steal, never from themselves; the dealer draws what's taken, from what
// the victim holds
TEST(ProductionTest, TheBlockadeSteals) {
  State state = newGame(3);
  const int planet = planetIndex(state, Hex{0, 0});
  flip(state, planet, PlanetKind::GasGiant, 8);
  dock(state, planet, 0);
  dock(state, planet, 1);
  state.player[1].resources = {0, 0, 0, 2, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 1, 6}));
  EXPECT_EQ(Game::legalActions(state), (std::vector<Action>{{Kind::MoveBlockade, planet, 1}}));
  EXPECT_FALSE(Game::apply(state, {Kind::MoveBlockade, planet, 0}));  // (themselves)
  EXPECT_FALSE(Game::apply(state, {Kind::MoveBlockade, planet, 2}));  // (no station there)
  EXPECT_FALSE(Game::apply(state, {Kind::MoveBlockade, planet, -1})); // (someone to steal from)
  ASSERT_TRUE(Game::apply(state, {Kind::MoveBlockade, planet, 1}));
  EXPECT_EQ(Game::currentPlayer(state), GameBots::kDealer);
  EXPECT_FALSE(Game::apply(state, {Kind::Steal, 0})); // (they've no minerals)
  ASSERT_TRUE(Game::apply(state, {Kind::Steal, static_cast<int>(Resource::Water)}));
  EXPECT_EQ(state.player[1].resources, (Resources{0, 0, 0, 1, 0}));
  EXPECT_EQ(state.player[0].resources, (Resources{2, 1, 1, 2, 1}));
  EXPECT_EQ(state.phase, Phase::Main);
  // A player with nothing isn't stolen from
  State empty = newGame(2);
  const int there = planetIndex(empty, Hex{0, 0});
  flip(empty, there, PlanetKind::GasGiant, 8);
  dock(empty, there, 1);
  empty.player[1].resources = {0, 0, 0, 0, 0};
  ASSERT_TRUE(Game::apply(empty, {Kind::ChooseHome, 0}));
  ASSERT_TRUE(Game::apply(empty, {Kind::Roll, 1, 6}));
  EXPECT_EQ(Game::legalActions(empty), (std::vector<Action>{{Kind::MoveBlockade, there, -1}}));
}

// The Imperial Tithe: on a rolled 7, everyone holding more than discardOver
// gives back half, rounded down, the roller too; off at 0
TEST(ProductionTest, TheImperialTitheOnASeven) {
  for (int limit : {0, 7}) {
    Settings settings;
    settings.discardOver = limit;
    State state = newGame(3, settings);
    state.player[0].resources = {4, 3, 1, 0, 0}; // 9, with the home's 1 to come
    state.player[1].resources = {9, 0, 0, 0, 0}; // 9
    state.player[2].resources = {7, 0, 0, 0, 0}; // 7: at the limit
    ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, static_cast<int>(Resource::Water)}));
    ASSERT_TRUE(Game::apply(state, {Kind::Roll, 2, 5}));
    if (limit == 0) {
      EXPECT_EQ(cardsOf(state, 0), 9);
      EXPECT_EQ(cardsOf(state, 1), 9);
    } else {
      // 9 less 4, from the most held each time (the first of a tie)
      EXPECT_EQ(state.player[0].resources, (Resources{1, 2, 1, 1, 0}));
      EXPECT_EQ(state.player[1].resources, (Resources{5, 0, 0, 0, 0})); // 9 less 4
    }
    EXPECT_EQ(cardsOf(state, 2), 7);
  }
}

// An Enforcer moves the blockade without the Tithe (it's the 7's)
TEST(ProductionTest, NoTitheForAnEnforcer) {
  Settings settings;
  settings.discardOver = 7;
  State state = newGame(2, settings);
  state.phase = Phase::Main;
  state.player[1].resources = {9, 0, 0, 0, 0};
  state.player[0].contracts = {kEnforcerCard};
  ASSERT_TRUE(Game::apply(state, {Kind::PlayEnforcer}));
  EXPECT_EQ(state.phase, Phase::Blockade);
  EXPECT_EQ(cardsOf(state, 1), 9);
}

// The home's income: one resource chosen, then the dealer's roll
TEST(ProductionTest, TheHomesIncomeIsChosen) {
  State state = newGame(2);
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  EXPECT_EQ(Game::legalActions(state).size(), static_cast<std::size_t>(kResources));
  EXPECT_FALSE(Game::apply(state, {Kind::ChooseHome, kResources}));
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, static_cast<int>(Resource::Crystals)}));
  EXPECT_EQ(state.player[0].resources, (Resources{1, 1, 1, 1, 2}));
  EXPECT_EQ(state.phase, Phase::RollDraw);
  EXPECT_FALSE(Game::apply(state, {Kind::Roll, 0, 3})); // (a die is 1 to 6)
  EXPECT_FALSE(Game::apply(state, {Kind::Roll, 7, 3}));
}

// The home die: each face 0 to 4 gives that resource; the sixth hands the
// choice to the player
TEST(ProductionTest, TheHomeDie) {
  Settings settings;
  settings.homeDie = true;
  for (int face = 0; face < kResources; ++face) {
    State state = newGame(2, settings);
    ASSERT_EQ(state.phase, Phase::HomeDieDraw);
    ASSERT_TRUE(Game::apply(state, {Kind::RollHomeDie, face}));
    EXPECT_EQ(state.player[0].resources[static_cast<std::size_t>(face)], 2);
    EXPECT_EQ(state.phase, Phase::RollDraw);
  }
  State state = newGame(2, settings);
  EXPECT_FALSE(Game::apply(state, {Kind::RollHomeDie, kResources + 1}));
  ASSERT_TRUE(Game::apply(state, {Kind::RollHomeDie, kResources}));
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  EXPECT_EQ(cardsOf(state, 0), 5);
  // The dealer rolls one of the six faces
  SeededRandom::Generator random(11);
  State rolling = newGame(2, settings);
  for (int i = 0; i < 60; ++i) {
    const Action roll = Game::dealerAction(rolling, random);
    EXPECT_EQ(roll.kind, Kind::RollHomeDie);
    EXPECT_GE(roll.a, 0);
    EXPECT_LE(roll.a, kResources);
  }
}

// Below exhaustHomesAt points, the home's income is an ordinary turn's
TEST(ProductionTest, BelowTheThresholdTheHomeGivesAsUsual) {
  Settings settings;
  settings.exhaustHomesAt = 3;
  State state = newGame(2, settings);
  state.player[0].badgePoints = 1; // 2 points
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, kFuel}));
  EXPECT_EQ(state.phase, Phase::RollDraw);
  EXPECT_EQ(state.planets[static_cast<std::size_t>(planetIndex(state, Hex{4, -2}))].homeOf, 0);
}

// At exhaustHomesAt points the resource taken is the home's last: it becomes
// a planet of that kind numbered from exhaustedNumbers, paying its stations
// (its owner's too) by the dice, open to others; its income stops
TEST(ProductionTest, AnExhaustedHomePaysByTheDice) {
  Settings settings;
  settings.exhaustHomesAt = 3;
  State state = newGame(2, settings);
  state.player[0].badgePoints = 2; // 3 points
  const int home = planetIndex(state, Hex{4, -2});
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, kFuel}));
  EXPECT_EQ(state.player[0].resources[of(Resource::Fuel)], 2); // (its last)
  EXPECT_EQ(state.phase, Phase::HomeNumberDraw);
  EXPECT_EQ(Game::currentPlayer(state), GameBots::kDealer);
  const Planet &planet = state.planets[static_cast<std::size_t>(home)];
  EXPECT_EQ(planet.kind, PlanetKind::GasGiant);
  EXPECT_EQ(planet.homeOf, -1);
  EXPECT_EQ(planet.formerHomeOf, 0);
  EXPECT_FALSE(Game::apply(state, {Kind::NumberHome, 6, -1})); // (not one of 4, 5, 9, 10)
  ASSERT_TRUE(Game::apply(state, {Kind::NumberHome, 9, -1}));
  EXPECT_EQ(planet.number, 9);
  EXPECT_EQ(state.phase, Phase::RollDraw);
  EXPECT_EQ(rows(payouts(state, 9)), (std::vector<std::array<int, 4>>{{home, 0, kFuel, 1}}));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 4, 5}));
  EXPECT_EQ(state.player[0].resources[of(Resource::Fuel)], 3);
  // Player 1 may settle by it
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 1, 2}));
  state.ships[1].at = planet.hex;
  ASSERT_TRUE(Game::apply(state, {Kind::Settle, 1}));
  EXPECT_EQ(planet.docks[1], 1);
  // Player 0's next turn starts at the roll: no home income
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  EXPECT_EQ(state.turn, 0);
  EXPECT_EQ(state.homeIncomeLeft, 0);
  EXPECT_EQ(state.phase, Phase::RollDraw);
}

// With the home die, the face rolled (or the player's choice, on the sixth)
// is the exhausted home's kind
TEST(ProductionTest, AnExhaustedHomesKindByTheHomeDie) {
  Settings settings;
  settings.exhaustHomesAt = 3;
  settings.homeDie = true;
  for (const auto &[faces, kind] :
       {std::pair{std::vector<Action>{Action{Kind::RollHomeDie, 4}}, PlanetKind::Nebula},
        std::pair{
            std::vector<Action>{Action{Kind::RollHomeDie, kResources}, Action{Kind::ChooseHome, 1}},
            PlanetKind::LushPlanet}}) {
    State state = newGame(2, settings);
    state.player[0].badgePoints = 2;
    for (const Action &face : faces)
      ASSERT_TRUE(Game::apply(state, face));
    EXPECT_EQ(state.phase, Phase::HomeNumberDraw);
    EXPECT_EQ(state.planets[static_cast<std::size_t>(planetIndex(state, Hex{4, -2}))].kind, kind);
  }
}

// The dealer draws the exhausted home's number from exhaustedNumbers, and
// its kind only when that's drawn too (exhaustedKindRandom)
TEST(ProductionTest, TheDealerDrawsTheExhaustedHomesNumber) {
  for (bool kindRandom : {false, true}) {
    Settings settings;
    settings.exhaustHomesAt = 1;
    settings.exhaustedKindRandom = kindRandom;
    State state = newGame(2, settings);
    ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
    ASSERT_EQ(state.phase, Phase::HomeNumberDraw);
    SeededRandom::Generator random(4);
    for (int i = 0; i < 40; ++i) {
      const Action draw = Game::dealerAction(state, random);
      EXPECT_NE(
          std::find(settings.exhaustedNumbers.begin(), settings.exhaustedNumbers.end(), draw.a),
          settings.exhaustedNumbers.end());
      if (kindRandom) {
        EXPECT_GE(draw.b, 0);
        EXPECT_LT(draw.b, kResources);
      } else {
        EXPECT_EQ(draw.b, -1);
      }
      State drawn = state;
      EXPECT_TRUE(Game::apply(drawn, draw));
    }
  }
}
