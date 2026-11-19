// The build screen's rows: red with why, or where it
// goes, every answer from the rules' own legal moves
#include <CosmicHex/Handheld/BuildMenu.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

namespace {

// A real game on the agreed rules, played on (the dealer by chance, the
// players by their first legal move) to its first main part
State atFirstMain() {
  State state = newGame(4, agreedRules(boardR3(), 4));
  SeededRandom::Generator random(11);
  for (int guard = 0; guard < 2000 && state.phase != Phase::Main; ++guard) {
    const Action next = Game::currentPlayer(state) == GameBots::kDealer
                            ? Game::dealerAction(state, random)
                            : Game::legalActions(state).front();
    EXPECT_TRUE(Game::apply(state, next));
  }
  EXPECT_EQ(state.phase, Phase::Main);
  return state;
}

Resources &cardsOf(State &state) {
  return state.player[static_cast<std::size_t>(state.turn)].resources;
}

} // namespace

TEST(BuildMenuTest, ARowForEachThingInOrder) {
  const auto rows = buildRows(atFirstMain());
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
  State state = atFirstMain();
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
  State state = atFirstMain();
  cardsOf(state) = {0, 1, 0, 1, 0}; // a card's cost but its Crystals
  const BuildRow card = buildRows(state)[3];
  EXPECT_FALSE(card.canBuild);
  EXPECT_EQ(card.note, "needs 1 more Crystals");
}

// With plenty, a row is buildable exactly when the rules list a way to build
// it, and then it carries those ways; a red row's "needs" means the rules
// would allow it with the cost added
TEST(BuildMenuTest, RowsAgreeWithTheRules) {
  State state = atFirstMain();
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
  State state = atFirstMain();
  cardsOf(state) = {9, 9, 9, 9, 9};
  state.phase = Phase::RollDraw;
  for (const BuildRow &row : buildRows(state)) {
    EXPECT_FALSE(row.canBuild) << nameOf(row.what);
    EXPECT_EQ(row.note, "not until your roll's done");
  }
}
