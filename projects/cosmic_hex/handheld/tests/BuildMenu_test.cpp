// The build screen's rows: red with why, or where it
// goes, every answer from the rules' own legal moves
#include <CosmicHex/Handheld/BuildMenu.hpp>

#include "TestTables.hpp"

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

namespace {

Resources &cardsOf(State &state) {
  return state.player[static_cast<std::size_t>(state.turn)].resources;
}

} // namespace

TEST(BuildMenuTest, ARowForEachThingInOrder) {
  const auto rows = buildRows(TestTables::atFirstMain());
  ASSERT_EQ(rows.size(), 4u);
  EXPECT_EQ(rows[0].what, Buildable::Ship);
  EXPECT_EQ(rows[1].what, Buildable::Station);
  EXPECT_EQ(rows[2].what, Buildable::Colony);
  EXPECT_EQ(rows[3].what, Buildable::Card);
  EXPECT_STREQ(nameOf(Buildable::Colony), "Colony");
}

// With no cards, nothing can be built: each row says what's missing, or that
// there's nowhere to put it
TEST(BuildMenuTest, WithNoCardsEveryRowIsRedAndSaysWhy) {
  State state = TestTables::atFirstMain();
  cardsOf(state) = {};
  for (const BuildRow &row : buildRows(state)) {
    EXPECT_FALSE(row.canBuild) << nameOf(row.what);
    EXPECT_TRUE(row.actions.empty()) << nameOf(row.what);
    EXPECT_FALSE(row.note.empty()) << nameOf(row.what);
  }
  // A card costs Food, Water and Crystals: all three missing
  EXPECT_EQ(buildRows(state)[3].note, "needs 1 Food, 1 Water, 1 Crystals");
}

TEST(BuildMenuTest, OneKindShortSaysHowManyMore) {
  State state = TestTables::atFirstMain();
  cardsOf(state) = {0, 1, 0, 1, 0}; // a card's cost but its Crystals
  const BuildRow card = buildRows(state)[3];
  EXPECT_FALSE(card.canBuild);
  EXPECT_EQ(card.note, "needs 1 more Crystals");
}

// With plenty, a row is buildable exactly when the rules list a way to build
// it, and then it carries those ways; a red row's "needs" means the rules
// would allow it with the cost added
TEST(BuildMenuTest, RowsAgreeWithTheRules) {
  State state = TestTables::atFirstMain();
  cardsOf(state) = {9, 9, 9, 9, 9};
  const auto legal = Game::legalActions(state);
  for (const BuildRow &row : buildRows(state)) {
    const bool listed = std::ranges::any_of(legal, [&](const Action &a) {
      return std::ranges::find(row.actions, a) != row.actions.end();
    });
    EXPECT_EQ(row.canBuild, listed) << nameOf(row.what);
  }
  // A ship from the home colony and a card from the deck, at least
  EXPECT_TRUE(buildRows(state)[0].canBuild);
  EXPECT_TRUE(buildRows(state)[3].canBuild);
  EXPECT_EQ(buildRows(state)[0].note, "at a colony");
}

// Before the main part (the roll not done), nothing, whatever you hold
TEST(BuildMenuTest, NothingBeforeTheRollsDone) {
  State state = TestTables::atFirstMain();
  cardsOf(state) = {9, 9, 9, 9, 9};
  state.phase = Phase::RollDraw;
  for (const BuildRow &row : buildRows(state)) {
    EXPECT_FALSE(row.canBuild) << nameOf(row.what);
    EXPECT_EQ(row.note, "not until your roll's done");
  }
}

// While the turn's offers to players are open (the user found "not until
// your roll's done" there, a ship on a corner and the cards to build): the
// rows are as they'll be once the offers end, and building ends them first
TEST(BuildMenuTest, DuringTheTurnsTradingBuildingEndsItFirst) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  SeededRandom::Generator random(11);
  for (int guard = 0; guard < 4000 && state.phase != Phase::Trading; ++guard) {
    const Action next = Game::currentPlayer(state) == GameBots::kDealer
                            ? Game::dealerAction(state, random)
                            : Game::legalActions(state).front();
    ASSERT_TRUE(Game::apply(state, next));
  }
  ASSERT_EQ(state.phase, Phase::Trading);
  cardsOf(state) = {9, 9, 9, 9, 9};
  State ended = state;
  ASSERT_TRUE(Game::apply(ended, Action{Action::Kind::EndTrading}));
  const auto during = buildRows(state), after = buildRows(ended);
  bool any = false;
  for (std::size_t i = 0; i < during.size(); ++i) {
    EXPECT_EQ(during[i].canBuild, after[i].canBuild) << nameOf(during[i].what);
    EXPECT_EQ(during[i].note, after[i].note) << nameOf(during[i].what);
    EXPECT_TRUE(during[i].endsTrading);
    EXPECT_FALSE(after[i].endsTrading);
    any = any || during[i].canBuild;
  }
  EXPECT_TRUE(any);
}
