// Setting up (Setup.cpp): a starting hand dealt round the table
// (Settings::startCards), and with corners the start's placements: home
// colonies in turn order, then ships in reverse (snake order), placing
// turning nothing over unless it's the deep-space start, and homes chosen.
#include <CosmicHex/Rules.hpp>

#include "TestGames.hpp"

#include <HexGrid/CornersAndEdges.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>

using namespace CosmicHex;
using namespace CosmicHex::TestGames;
using Kind = Action::Kind;

namespace {

bool anythingTurnedOver(const State &state) {
  for (const Planet &planet : state.planets)
    if (planet.flipped && planet.kind != PlanetKind::Home)
      return true;
  for (const ZoneSite &site : state.zone)
    if (site.flipped)
      return true;
  return false;
}

} // namespace

// A starting hand: startCards each, dealt one at a time round the table
// from the first player, in place of one of each
TEST(SetupTest, AStartingHandIsDealtRoundTheTable) {
  Settings settings;
  settings.startCards = 2;
  State state = newGame(3, settings, 1);
  EXPECT_EQ(state.startCardsLeft, 6);
  for (int p = 0; p < 3; ++p)
    EXPECT_EQ(cardsOf(state, p), 0);
  // Card i to player (1 + i) % 3: resource i to each in turn
  for (int i = 0; i < 6; ++i) {
    const int to = (1 + i) % 3;
    const int before = state.player[static_cast<std::size_t>(to)].resources[i % kResources];
    ASSERT_TRUE(Game::apply(state, {Kind::DealStartCard, i % kResources}));
    EXPECT_EQ(state.player[static_cast<std::size_t>(to)].resources[i % kResources], before + 1)
        << "card " << i;
  }
  for (int p = 0; p < 3; ++p)
    EXPECT_EQ(cardsOf(state, p), 2);
  EXPECT_EQ(state.player[1].resources, (Resources{1, 0, 0, 1, 0}));
  // Then the first player's turn, and no more dealing
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  EXPECT_EQ(state.turn, 1);
  EXPECT_FALSE(Game::apply(state, {Kind::DealStartCard, 0}));
}

// A seat bonus comes on top of a dealt hand, by turn order
TEST(SetupTest, ASeatBonusOnTopOfADealtHand) {
  Settings settings;
  settings.startCards = 1;
  settings.seatBonus = {0, 2, 0, 0};
  const State state = newGame(2, settings, 1);
  EXPECT_EQ(state.player[1].resources, (Resources{0, 0, 0, 0, 0})); // (1st to play)
  EXPECT_EQ(state.player[0].resources, (Resources{1, 1, 0, 0, 0})); // (2nd)
}

// With corners, the dealt hand comes first and the placements follow,
// from the first player
TEST(SetupTest, WithCornersThePlacementsFollowTheDeal) {
  Settings settings = cornerSettings();
  settings.startCards = 1;
  State state = newGame(3, settings, 2);
  EXPECT_EQ(state.phase, Phase::StartCardsDraw);
  for (int i = 0; i < 3; ++i)
    ASSERT_TRUE(Game::apply(state, {Kind::DealStartCard, 0}));
  EXPECT_EQ(state.phase, Phase::PlaceColony);
  EXPECT_EQ(state.turn, 2);
}

// Snake order for three: colonies 2, 0, 1 from the first player, ships 1,
// 0, 2 back; then the first player's home income
TEST(SetupTest, SnakeOrderForThree) {
  State state = newGame(3, cornerSettings(), 2);
  std::vector<int> colonies, ships;
  while (state.phase == Phase::PlaceColony || state.phase == Phase::PlaceShip) {
    (state.phase == Phase::PlaceColony ? colonies : ships).push_back(state.turn);
    const std::vector<Action> legal = Game::legalActions(state);
    ASSERT_FALSE(legal.empty());
    ASSERT_TRUE(Game::apply(state, legal.front()));
  }
  EXPECT_EQ(colonies, (std::vector<int>{2, 0, 1}));
  EXPECT_EQ(ships, (std::vector<int>{1, 0, 2}));
  EXPECT_EQ(state.turn, 2);
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  for (const Station &station : state.stations)
    EXPECT_TRUE(station.colony);
}

// The first player can go without their first home income (firstSkipsIncome):
// after the placements, their turn starts at the roll
TEST(SetupTest, TheFirstMaySkipTheirIncomeAfterThePlacements) {
  Settings settings = cornerSettings();
  settings.firstSkipsIncome = true;
  State state = newGame(2, settings, 1);
  while (state.phase == Phase::PlaceColony || state.phase == Phase::PlaceShip)
    ASSERT_TRUE(Game::apply(state, Game::legalActions(state).front()));
  EXPECT_EQ(state.turn, 1);
  EXPECT_EQ(state.phase, Phase::RollDraw);
}

// Placing a starting ship turns nothing over (but for the deep-space
// start), even beside a face-down tile; its colony's own corner is a place
TEST(SetupTest, PlacingAShipTurnsNothingOver) {
  State state = newGame(4, cornerSettings(), 0);
  while (state.phase == Phase::PlaceColony)
    ASSERT_TRUE(Game::apply(state, Game::legalActions(state).front()));
  // (a place beside a face-down planet or tile)
  const auto byAFaceDownTile = [&](const Action &place) {
    for (Hex hex : HexGrid::hexesTouching(boardCorners()[static_cast<std::size_t>(place.a)])) {
      const int planet = planetIndex(state, hex);
      if ((planet >= 0 && !state.planets[static_cast<std::size_t>(planet)].flipped) ||
          siteIndex(state, hex) >= 0)
        return true;
    }
    return false;
  };
  int besideATile = 0;
  while (state.phase == Phase::PlaceShip) {
    const std::vector<Action> legal = Game::legalActions(state);
    const Corner colony = state.stations[static_cast<std::size_t>(state.turn)].at;
    EXPECT_TRUE(has(legal, {Kind::PlaceShip, cornerNumber(colony)}));
    const auto chosen = std::find_if(legal.begin(), legal.end(), byAFaceDownTile);
    besideATile += chosen != legal.end();
    ASSERT_TRUE(Game::apply(state, chosen != legal.end() ? *chosen : legal.front()));
  }
  EXPECT_GT(besideATile, 0);
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  EXPECT_FALSE(anythingTurnedOver(state));
  for (int p = 0; p < 4; ++p)
    EXPECT_EQ(state.player[static_cast<std::size_t>(p)].badgesEarned, 0);
}

// A starting ship goes within startShipReach edges of its colony
TEST(SetupTest, AStartingShipsReachIsASetting) {
  const auto places = [](int reach) {
    Settings settings = cornerSettings();
    settings.startShipReach = reach;
    State state = newGame(2, settings, 0);
    while (state.phase == Phase::PlaceColony)
      EXPECT_TRUE(Game::apply(state, Game::legalActions(state).front()));
    return Game::legalActions(state).size();
  };
  EXPECT_EQ(places(0), 1u); // (the colony's own corner)
  EXPECT_GT(places(1), 1u); // (and its neighbours on the board)
  EXPECT_LE(places(1), 4u);
  EXPECT_GT(places(3), places(1));
}

// The deep-space start: a ship placed by no tile at all goes on at once,
// to the next player
TEST(SetupTest, ADeepSpaceShipByNothingGoesOn) {
  Settings settings = cornerSettings();
  settings.deepSpaceStart = true;
  State state = newGame(2, settings, 0);
  while (state.phase == Phase::PlaceColony)
    ASSERT_TRUE(Game::apply(state, Game::legalActions(state).front()));
  ASSERT_EQ(state.turn, 1);
  for (const Action &place : Game::legalActions(state)) {
    const Corner corner = boardCorners()[static_cast<std::size_t>(place.a)];
    bool byATile = false;
    for (Hex hex : HexGrid::hexesTouching(corner))
      byATile = byATile || siteIndex(state, hex) >= 0 || planetIndex(state, hex) >= 0;
    if (byATile)
      continue;
    ASSERT_TRUE(Game::apply(state, place));
    EXPECT_EQ(state.phase, Phase::PlaceShip);
    EXPECT_EQ(state.turn, 0);
    EXPECT_EQ(state.player[1].badgesEarned, 0);
    return;
  }
  ADD_FAILURE() << "no corner by nothing";
}

// The deep-space start with trading on: a pirate hideout a starting ship
// turns over steals, and the setup goes on, with no trading in it (trading
// is a turn's); the first player's first turn has its trading after the
// roll. (On after the roll, the placer traded in the setup, and trading
// marked done then was never reset: the first turn had none)
TEST(SetupTest, AHideoutFoundPlacingLeavesTheFirstTurnItsTrading) {
  Settings settings = cornerSettings();
  settings.deepSpaceStart = true;
  settings.playerTrading = true;
  State state = newGame(2, settings, 0);
  while (state.phase == Phase::PlaceColony)
    ASSERT_TRUE(Game::apply(state, Game::legalActions(state).front()));
  ASSERT_EQ(state.turn, 1); // (ships in reverse: player 1 first)
  state.player[0].resources = {1, 0, 0, 0, 0};
  const std::vector<Action> places = Game::legalActions(state);
  const auto byAZoneTile = std::find_if(places.begin(), places.end(), [&](const Action &place) {
    const Corner corner = boardCorners()[static_cast<std::size_t>(place.a)];
    return std::ranges::any_of(HexGrid::hexesTouching(corner),
                               [&](Hex hex) { return siteIndex(state, hex) >= 0; });
  });
  ASSERT_NE(byAZoneTile, places.end());
  ASSERT_TRUE(Game::apply(state, *byAZoneTile));
  // The dealer: the first zone tile a pirate hideout; the rest as they come
  SeededRandom::Generator random(1);
  bool hideout = false, stole = false;
  while (state.turn == 1 && state.phase != Phase::PlaceShip) {
    EXPECT_NE(state.phase, Phase::Trading);
    if (state.phase == Phase::ZoneDraw && !hideout) {
      ASSERT_TRUE(Game::apply(state, {Kind::RevealFind, static_cast<int>(Find::PirateHideout)}));
      hideout = true;
      continue;
    }
    stole = stole || state.phase == Phase::StealDraw;
    ASSERT_TRUE(Game::apply(state, Game::dealerAction(state, random)));
  }
  ASSERT_TRUE(hideout);
  EXPECT_TRUE(stole);
  EXPECT_EQ(state.phase, Phase::PlaceShip);
  EXPECT_EQ(state.turn, 0);
  EXPECT_FALSE(state.tradingDone);
  ASSERT_TRUE(playOnUntil(state, [](const State &s) {
    return s.phase == Phase::Trading || (s.turn == 0 && s.phase == Phase::Main);
  }));
  EXPECT_EQ(state.phase, Phase::Trading);
  EXPECT_EQ(state.turn, 0);
}

// Choosing homes: a home claimed by a colony is no longer offered, and
// a home colony's corners follow the distance rule
TEST(SetupTest, AChosenHomeIsTaken) {
  Settings settings = cornerSettings();
  settings.chooseHome = true;
  State state = newGame(2, settings, 0);
  const std::size_t offered = Game::legalActions(state).size();
  ASSERT_TRUE(Game::apply(state, Game::legalActions(state).front()));
  const std::size_t left = Game::legalActions(state).size();
  EXPECT_EQ(left, offered - 6); // (the six corners of the home taken)
  for (const Action &place : Game::legalActions(state))
    EXPECT_TRUE(clearOfStations(state, boardCorners()[static_cast<std::size_t>(place.a)]));
}
