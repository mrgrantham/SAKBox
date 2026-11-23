// Scoring (Scoring.cpp): the points from each source (stations, colonies,
// badges, revealed Charters, the Galactic Embargo, Core Control, Imperial
// Dominion), and how the game ends: at once on reaching the target on your
// own turn, or with the round played out (Settings::finishRound), a tie
// going to the most stations and colonies, then shared.
#include <CosmicHex/Rules.hpp>

#include "TestGames.hpp"

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::TestGames;
using Kind = Action::Kind;

namespace {

// A turn in which nothing happens: the home's income, a roll of 3, the end
void playQuietTurn(State &state) {
  if (state.phase == Phase::HomeIncome) {
    EXPECT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  }
  EXPECT_TRUE(Game::apply(state, {Kind::Roll, 1, 2}));
  EXPECT_TRUE(Game::apply(state, {Kind::EndTurn}));
}

// Three players finishing the round, player `first` first
State finishingTheRound(int first) {
  Settings settings;
  settings.finishRound = true;
  return newGame(3, settings, first);
}

} // namespace

// On hexes a station scores 1 (the home's too) and a colony 2
TEST(ScoringTest, StationsScoreOneAndColoniesTwo) {
  State state = newGame(2);
  EXPECT_EQ(points(state, 0), 1); // the home station
  const int station = planetIndex(state, Hex{0, 0}), colony = planetIndex(state, Hex{3, 0});
  flip(state, station, PlanetKind::GasGiant, 8);
  flip(state, colony, PlanetKind::IceMoon, 5);
  dock(state, station, 0);
  EXPECT_EQ(points(state, 0), 2);
  dock(state, colony, 0, true);
  EXPECT_EQ(points(state, 0), 4);
  EXPECT_EQ(stationsOf(state, 0), 3); // (a colony is one station)
  EXPECT_EQ(points(state, 1), 1);
}

// On corners the home colony scores 2, a station 1 and a colony 2
TEST(ScoringTest, OnCornersTheHomeColonyScoresTwo) {
  State state = cornersAtMain();
  EXPECT_EQ(points(state, 0), 2);
  state.stations.push_back(Station{cornerTouching(state, 2), 0, false});
  EXPECT_EQ(points(state, 0), 3);
  state.stations.back().colony = true;
  EXPECT_EQ(points(state, 0), 4);
  EXPECT_EQ(stationsOf(state, 0), 2);
}

// Badges score only once cashed, a point each
TEST(ScoringTest, CashedBadgesScoreAndUncashedDont) {
  State state = newGame(2);
  state.player[0].badges = 2;
  EXPECT_EQ(points(state, 0), 1);
  state.player[0].badgePoints = 2;
  EXPECT_EQ(points(state, 0), 3);
}

// A Charter scores its points once revealed, not while held; the relay's
// are its setting
TEST(ScoringTest, RevealedChartersScoreTheirPoints) {
  State state = newGame(2);
  state.player[0].contracts = {21, 19, kDeepSpaceRelay};
  EXPECT_EQ(points(state, 0), 1);
  state.player[0].revealed = {21};
  EXPECT_EQ(points(state, 0), 1 + contracts()[21].points);
  state.player[0].revealed = {21, 19};
  EXPECT_EQ(points(state, 0), 1 + contracts()[21].points + contracts()[19].points);
  state.player[0].revealed = {kDeepSpaceRelay};
  EXPECT_EQ(points(state, 0), 1 + state.settings.relayPoints);
  state.settings.relayPoints = 3;
  EXPECT_EQ(points(state, 0), 1 + 3);
}

// The Galactic Embargo scores its holder embargoPoints, nobody else
TEST(ScoringTest, TheGalacticEmbargoScoresItsHolder) {
  State state = newGame(2);
  state.embargoHolder = 1;
  EXPECT_EQ(points(state, 1), 1 + 2);
  EXPECT_EQ(points(state, 0), 1);
  state.settings.embargoPoints = 4;
  EXPECT_EQ(points(state, 1), 1 + 4);
}

// Core Control scores 2 to the first with 2 core stations; with its points
// at 0 (the agreed rules) nobody holds it
TEST(ScoringTest, CoreControlScoresOnlyWhenOn) {
  const auto withTwoCoreStations = [](int controlPoints) {
    State state = atMain();
    state.settings.coreControlPoints = controlPoints;
    flip(state, planetIndex(state, Hex{0, 0}), PlanetKind::GasGiant, 4);
    flip(state, planetIndex(state, Hex{1, 1}), PlanetKind::IceMoon, 10);
    state.player[0].resources = {0, 5, 0, 5, 0};
    state.ships[0].at = Hex{0, 0};
    EXPECT_TRUE(Game::apply(state, {Kind::Settle, 0}));
    state.ships.push_back(Ship{0, Hex{1, 1}, false});
    EXPECT_TRUE(Game::apply(state, {Kind::Settle, static_cast<int>(state.ships.size()) - 1}));
    return state;
  };
  const State on = withTwoCoreStations(2);
  EXPECT_EQ(on.coreHolder, 0);
  EXPECT_EQ(points(on, 0), 3 + 2);
  const State off = withTwoCoreStations(0);
  EXPECT_EQ(off.coreHolder, -1);
  EXPECT_EQ(points(off, 0), 3);
}

// Imperial Dominion scores its holder dominionPoints: nothing at 0 (off)
TEST(ScoringTest, ImperialDominionScoresOnlyWhenOn) {
  State state = newGame(2);
  state.dominionHolder = 0;
  EXPECT_EQ(points(state, 0), 1);
  state.settings.dominionPoints = 2;
  EXPECT_EQ(points(state, 0), 1 + 2);
}

// Without finishing the round, reaching the target on your own turn wins at once
TEST(ScoringTest, TheTargetOnYourOwnTurnWinsAtOnce) {
  State state = atMain();
  state.player[0].badgePoints = state.settings.winPoints - 1; // (and the home's 1)
  state.player[0].resources = {4, 0, 0, 0, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::Trade, 0, 1})); // any action
  EXPECT_EQ(state.winner, 0);
  EXPECT_EQ(state.phase, Phase::Over);
  EXPECT_TRUE(Game::legalActions(state).empty());
  EXPECT_FALSE(Game::apply(state, {Kind::EndTurn}));
}

// Points reached on another's turn win only on your own, once your home's
// income is taken
TEST(ScoringTest, TheTargetReachedOnAnothersTurnWinsOnYourOwn) {
  State state = atMain();
  state.player[1].badgePoints = state.settings.winPoints - 1;
  state.player[0].resources = {4, 0, 0, 0, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::Trade, 0, 1}));
  EXPECT_FALSE(Game::isOver(state));
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  EXPECT_FALSE(Game::isOver(state));
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  EXPECT_EQ(state.winner, 1);
}

// Finishing the round: the target reached, the others still play, and the
// game ends when the turn comes back to the first player (not player 0)
TEST(ScoringTest, FinishingTheRoundPlaysItOut) {
  State state = finishingTheRound(1);
  state.player[1].badgePoints = state.settings.winPoints - 1;
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  EXPECT_TRUE(state.finalRound);
  EXPECT_FALSE(Game::isOver(state));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 1, 2}));
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  playQuietTurn(state); // player 2
  EXPECT_FALSE(Game::isOver(state));
  playQuietTurn(state); // player 0, the round's last
  ASSERT_TRUE(Game::isOver(state));
  EXPECT_EQ(state.phase, Phase::Over);
  EXPECT_EQ(state.winners, (std::vector<int>{1}));
  EXPECT_EQ(state.winner, 1);
}

// Finishing the round, the most points wins, whoever reached the target first
TEST(ScoringTest, FinishingTheRoundTheMostPointsWins) {
  State state = finishingTheRound(1);
  state.player[1].badgePoints = state.settings.winPoints - 1;
  state.player[0].badgePoints = state.settings.winPoints; // (one more, by the round's end)
  playQuietTurn(state);
  playQuietTurn(state);
  playQuietTurn(state);
  ASSERT_TRUE(Game::isOver(state));
  EXPECT_EQ(state.winners, (std::vector<int>{0}));
  EXPECT_EQ(state.winner, 0);
}

// A tie on points goes to the most stations and colonies on the board, a
// colony counting as one piece (not its 2 points)
TEST(ScoringTest, ATieGoesToTheMostStationsAndColonies) {
  State state = finishingTheRound(0);
  // Player 1: the home and a colony (3 pieces' worth of points, 2 pieces)
  const int colony = planetIndex(state, Hex{0, 0});
  flip(state, colony, PlanetKind::GasGiant, 8);
  dock(state, colony, 1, true);
  // Player 2: the home and two stations (3 points, 3 pieces)
  for (const Hex hex : {Hex{3, 0}, Hex{-3, 0}}) {
    const int planet = planetIndex(state, hex);
    flip(state, planet, PlanetKind::IceMoon, 5);
    dock(state, planet, 2);
  }
  state.player[1].badgePoints = state.settings.winPoints - 3;
  state.player[2].badgePoints = state.settings.winPoints - 3;
  ASSERT_EQ(points(state, 1), points(state, 2));
  playQuietTurn(state);
  playQuietTurn(state); // player 1 reaches the target
  EXPECT_TRUE(state.finalRound);
  playQuietTurn(state);
  ASSERT_TRUE(Game::isOver(state));
  EXPECT_EQ(state.winners, (std::vector<int>{2}));
  EXPECT_EQ(state.winner, 2);
}

// Still tied, the win is shared; the winners are listed in turn order from
// the first player, and the winner is the first of them
TEST(ScoringTest, AFullTieIsSharedTheWinnerFirstInTurnOrder) {
  State state = finishingTheRound(1);
  state.player[0].badgePoints = state.settings.winPoints - 1;
  state.player[2].badgePoints = state.settings.winPoints - 1;
  playQuietTurn(state); // player 1
  EXPECT_FALSE(state.finalRound);
  playQuietTurn(state); // player 2 reaches the target
  EXPECT_TRUE(state.finalRound);
  playQuietTurn(state); // player 0
  ASSERT_TRUE(Game::isOver(state));
  EXPECT_EQ(state.winners, (std::vector<int>{2, 0})); // (turn order: 1, 2, 0)
  EXPECT_EQ(state.winner, 2);
  EXPECT_EQ(Game::score(state, 2), 1.0);
  EXPECT_EQ(Game::score(state, 0), 0.0); // (the bots' score: the winner's alone)
}
