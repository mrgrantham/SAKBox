// What the cards screens offer (CardChoices): the cards you hold, playable
// only on your move and when the rules allow, and why not; and after
// buying, the two dealt to keep one of
#include <CosmicHex/Handheld/CardChoices.hpp>
#include <CosmicHex/Handheld/CardsHeld.hpp>

#include "TestTables.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Kind = Action::Kind;

// An Enforcer held in the main part of your turn is playable; off your
// turn it isn't, and says to wait for it
TEST(CardChoicesTest, ACardIsPlayableOnlyOnYourMove) {
  State state = TestTables::atFirstMain();
  const int you = state.turn;
  state.player[static_cast<std::size_t>(you)].contracts = {kEnforcerCard};
  const std::vector<Action> legal = Game::legalActions(state);
  const std::vector<HeldCard> mine = heldCards(state, legal, you, true);
  ASSERT_EQ(mine.size(), 1u);
  EXPECT_EQ(mine[0].card, Art::Card::Enforcer);
  EXPECT_EQ(mine[0].kind, Kind::PlayEnforcer);
  EXPECT_FALSE(mine[0].charter);
  EXPECT_TRUE(mine[0].playable);
  EXPECT_TRUE(mine[0].why.empty());
  const std::vector<HeldCard> waiting = heldCards(state, legal, you, false);
  ASSERT_EQ(waiting.size(), 1u);
  EXPECT_FALSE(waiting[0].playable);
  EXPECT_EQ(waiting[0].why, "On your turn");
}

// A card the rules don't allow now (an Enforcer outside the main part)
// says not now; a Charter not met yet says so; an id without a drawing
// isn't shown
TEST(CardChoicesTest, WhyACardCantBeUsed) {
  State state = TestTables::playedUntil([](const State &s) { return s.phase == Phase::Trading; });
  ASSERT_EQ(state.phase, Phase::Trading);
  const int you = state.turn;
  constexpr int kOccupier = 19, kNoDrawing = 0; // (a Charter without a card drawn)
  ASSERT_FALSE(cardOf(kNoDrawing).has_value());
  state.player[static_cast<std::size_t>(you)].contracts = {kNoDrawing, kEnforcerCard, kOccupier};
  const std::vector<Action> legal = Game::legalActions(state);
  ASSERT_TRUE(std::none_of(legal.begin(), legal.end(),
                           [](const Action &a) { return a.kind == Kind::PlayEnforcer; }));
  const std::vector<HeldCard> held = heldCards(state, legal, you, true);
  ASSERT_EQ(held.size(), 2u); // (in the order held)
  EXPECT_EQ(held[0].card, Art::Card::Enforcer);
  EXPECT_FALSE(held[0].playable);
  EXPECT_EQ(held[0].why, "Not now: one card a turn, not the turn it's bought");
  EXPECT_EQ(held[1].card, Art::Card::CharterOccupier);
  EXPECT_TRUE(held[1].charter);
  EXPECT_EQ(held[1].kind, Kind::RevealContract);
  EXPECT_FALSE(held[1].playable);
  EXPECT_EQ(held[1].why, "Not met yet");
}

// After buying: the two dealt, each with the move that keeps it, in the
// same order; nothing to keep in any other part of the turn
TEST(CardChoicesTest, TheTwoDealtAreOfferedToKeep) {
  const State state =
      TestTables::playedBuyingUntil([](const State &s) { return s.phase == Phase::KeepContract; });
  ASSERT_EQ(state.phase, Phase::KeepContract);
  const std::vector<Action> legal = Game::legalActions(state);
  const auto choice = keepChoiceOf(state, legal);
  ASSERT_TRUE(choice.has_value());
  ASSERT_EQ(choice->cards.size(), choice->actions.size());
  EXPECT_GE(choice->cards.size(), 1u);
  EXPECT_LE(choice->cards.size(), 2u);
  for (std::size_t i = 0; i < choice->actions.size(); ++i) {
    EXPECT_EQ(choice->actions[i].kind, Kind::KeepContract);
    EXPECT_EQ(cardOf(choice->actions[i].a), choice->cards[i]);
  }
  const State main = TestTables::atFirstMain();
  EXPECT_FALSE(keepChoiceOf(main, Game::legalActions(main)).has_value());
}
