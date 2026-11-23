// Exploring (Exploring.cpp): what each zone find gives (nothing, salvage,
// ruins' badges, an alien outpost, a pirate hideout's steals), the badge for
// turning a tile over, and badges cashed for points (at once, or when
// their holder chooses).
#include <CosmicHex/Rules.hpp>

#include "TestGames.hpp"

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::TestGames;
using Kind = Action::Kind;

namespace {

// Player 0 turning over the zone's first tile (on hexes), as a move onto it
// would: the dealer draws `find`
State turningOver(Find find, int players = 3) {
  State state = atMain(players);
  state.ships[0].at = state.zone[0].hex;
  state.pendingSite = 0;
  state.phase = Phase::ZoneDraw;
  EXPECT_TRUE(Game::apply(state, {Kind::RevealFind, static_cast<int>(find)}));
  EXPECT_TRUE(state.zone[0].flipped);
  EXPECT_EQ(state.zone[0].find, find);
  EXPECT_EQ(state.pendingSite, -1);
  return state;
}

} // namespace

// Nothing: the tile's badge and no more; the find comes off its pile
TEST(ExploringTest, NothingGivesItsBadge) {
  const int left = atMain(3).findsLeft[static_cast<std::size_t>(Find::Nothing)];
  const State state = turningOver(Find::Nothing);
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.player[0].badgesEarned, 1);
  EXPECT_EQ(state.player[0].findsKept, 0);
  EXPECT_EQ(state.findsLeft[static_cast<std::size_t>(Find::Nothing)], left - 1);
  EXPECT_EQ(cardsOf(state, 0), 6); // (the start's 5 and the home's 1)
}

// Salvage: salvageResources of the player's choice, one at a time, before
// anything else; a kept find
TEST(ExploringTest, SalvageIsTakenOneAtATime) {
  State state = turningOver(Find::Salvage);
  EXPECT_EQ(state.phase, Phase::Salvage);
  EXPECT_EQ(state.player[0].findsKept, 1);
  EXPECT_EQ(Game::legalActions(state).size(), static_cast<std::size_t>(kResources));
  EXPECT_FALSE(Game::apply(state, {Kind::EndTurn}));
  EXPECT_FALSE(Game::apply(state, {Kind::TakeSalvage, kResources}));
  ASSERT_TRUE(Game::apply(state, {Kind::TakeSalvage, 3}));
  EXPECT_EQ(state.phase, Phase::Salvage);
  ASSERT_TRUE(Game::apply(state, {Kind::TakeSalvage, 4}));
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.player[0].resources, (Resources{2, 1, 1, 2, 2}));
}

// Ruins: ruinsBadges more badges (3 with the tile's), cashed at once into a
// point; a kept find
TEST(ExploringTest, RuinsGiveBadges) {
  const State state = turningOver(Find::Ruins);
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.player[0].badgesEarned, 1 + 2);
  EXPECT_EQ(state.player[0].findsKept, 1);
  EXPECT_EQ(state.player[0].badges, 0);
  EXPECT_EQ(state.player[0].badgePoints, 1);
}

// An alien outpost: the tile's badge; not a kept find (it stays on the
// board, to trade at)
TEST(ExploringTest, AnAlienOutpostStaysOnTheBoard) {
  const State state = turningOver(Find::AlienStation);
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.player[0].badgesEarned, 1);
  EXPECT_EQ(state.player[0].findsKept, 0);
}

// A pirate hideout: a steal from each other player holding anything, in turn
// order from the next; a kept find
TEST(ExploringTest, APirateHideoutStealsFromEachOther) {
  State state = atMain(4);
  state.player[2].resources = {0, 0, 0, 0, 0};
  state.player[3].resources = {0, 0, 0, 0, 3};
  state.pendingSite = 0;
  state.phase = Phase::ZoneDraw;
  ASSERT_TRUE(Game::apply(state, {Kind::RevealFind, static_cast<int>(Find::PirateHideout)}));
  EXPECT_EQ(state.player[0].findsKept, 1);
  EXPECT_EQ(state.phase, Phase::StealDraw);
  EXPECT_EQ(state.victims, (std::vector<int>{1, 3}));
  ASSERT_TRUE(Game::apply(state, {Kind::Steal, 0}));
  EXPECT_FALSE(Game::apply(state, {Kind::Steal, 0})); // (player 3 has no minerals)
  ASSERT_TRUE(Game::apply(state, {Kind::Steal, 4}));
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.player[0].resources, (Resources{3, 1, 1, 1, 2}));
  EXPECT_EQ(state.player[3].resources, (Resources{0, 0, 0, 0, 2}));
  // Nobody holding anything: straight back to the main part
  State poor = atMain(2);
  poor.player[1].resources = {0, 0, 0, 0, 0};
  poor.pendingSite = 0;
  poor.phase = Phase::ZoneDraw;
  ASSERT_TRUE(Game::apply(poor, {Kind::RevealFind, static_cast<int>(Find::PirateHideout)}));
  EXPECT_EQ(poor.phase, Phase::Main);
  EXPECT_TRUE(poor.victims.empty());
}

// A find drawn must be left in its pile
TEST(ExploringTest, AFindComesFromItsPile) {
  State state = atMain(3);
  state.findsLeft[static_cast<std::size_t>(Find::Ruins)] = 0;
  state.pendingSite = 0;
  state.phase = Phase::ZoneDraw;
  EXPECT_FALSE(Game::apply(state, {Kind::RevealFind, static_cast<int>(Find::Ruins)}));
  EXPECT_FALSE(Game::apply(state, {Kind::RevealFind, kFinds}));
  EXPECT_TRUE(Game::apply(state, {Kind::RevealFind, static_cast<int>(Find::Salvage)}));
}

// The salvage taken is a setting
TEST(ExploringTest, HowMuchSalvageIsASetting) {
  State state = atMain(3);
  state.settings.salvageResources = 3;
  state.pendingSite = 0;
  state.phase = Phase::ZoneDraw;
  ASSERT_TRUE(Game::apply(state, {Kind::RevealFind, static_cast<int>(Find::Salvage)}));
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(state.phase, Phase::Salvage);
    ASSERT_TRUE(Game::apply(state, {Kind::TakeSalvage, 0}));
  }
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.player[0].resources[0], 2 + 3);
}

// Badges cashed at once (autoCashBadges): every badgesPerPoint become a
// point after any action, the rest kept; nothing to choose
TEST(ExploringTest, BadgesAreCashedAtOnce) {
  State state = atMain();
  state.player[0].badges = 7;
  state.player[1].badges = 3; // (anyone's, not only the mover's)
  state.player[0].resources = {4, 0, 0, 0, 0};
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::CashBadges));
  ASSERT_TRUE(Game::apply(state, {Kind::Trade, 0, 1}));
  EXPECT_EQ(state.player[0].badges, 1);
  EXPECT_EQ(state.player[0].badgePoints, 2);
  EXPECT_EQ(state.player[1].badgePoints, 1);
}

// Held badges (autoCashBadges off) wait for CashBadges: badgesPerPoint for
// a point, one point an action
TEST(ExploringTest, HeldBadgesAreCashedByChoice) {
  State state = atMain();
  state.settings.autoCashBadges = false;
  state.settings.badgesPerPoint = 2;
  state.player[0].badges = 1;
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::CashBadges));
  EXPECT_FALSE(Game::apply(state, {Kind::CashBadges}));
  state.player[0].badges = 5;
  ASSERT_TRUE(hasKind(Game::legalActions(state), Kind::CashBadges));
  ASSERT_TRUE(Game::apply(state, {Kind::CashBadges}));
  EXPECT_EQ(state.player[0].badges, 3);
  EXPECT_EQ(state.player[0].badgePoints, 1);
}
