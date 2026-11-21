// The board's buttons (BoardButtons) and the two pickers (Pickers): what
// shows when, what a touch hits, and the moves the choices make, from a
// real game's legal moves
#include <CosmicHex/Handheld/BoardButtons.hpp>
#include <CosmicHex/Handheld/Pickers.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Kind = Action::Kind;

namespace {
// A four-player game played by the first legal move until `until` says stop
State playUntil(bool (*until)(const State &), int limit = 4000) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  SeededRandom::Generator dealer(7);
  for (int i = 0; i < limit && !until(state) && state.winner < 0; ++i) {
    const Action action = Game::currentPlayer(state) == GameBots::kDealer
                              ? Game::dealerAction(state, dealer)
                              : Game::legalActions(state).front();
    Game::apply(state, action);
  }
  return state;
}
} // namespace

TEST(BoardButtonsTest, TheCornersAndWhatShows) {
  const BoardButtons::Situation waiting{};
  const auto off = BoardButtons::layout(256, 192, waiting);
  // Not your move: no Roll, no End turn; Build dimmed, Show resources usable
  EXPECT_EQ(off.size(), 5u);
  EXPECT_EQ(BoardButtons::at(off, 10, 10), BoardButtons::Button::ShowResources);
  EXPECT_FALSE(BoardButtons::at(off, 240, 10).has_value()); // (Build, dimmed)
  const BoardButtons::Situation mine{.yourMove = true,
                                     .canRoll = true,
                                     .canEndTurn = false,
                                     .canTrade = true,
                                     .holdsCards = false};
  const auto on = BoardButtons::layout(256, 192, mine);
  EXPECT_EQ(BoardButtons::at(on, 240, 10), BoardButtons::Button::Build);
  EXPECT_EQ(BoardButtons::at(on, 240, 36), BoardButtons::Button::Trade);
  EXPECT_EQ(BoardButtons::at(on, 10, 180), BoardButtons::Button::Roll);
  EXPECT_FALSE(BoardButtons::at(on, 240, 180).has_value()); // (no End turn before the roll)
  EXPECT_FALSE(BoardButtons::at(on, 128, 96).has_value());  // (the board's middle)
  // Every button inside the screen, on the 3DS's bigger one too
  for (const auto &p : BoardButtons::layout(320, 240, mine)) {
    EXPECT_GE(p.x, 0);
    EXPECT_LE(p.x + p.size, 320);
    EXPECT_LE(p.y + p.size, 240);
  }
}

// Bottom left: Roll, then End Turn in its place once you've rolled; bottom
// right, apart from it: Confirm while placing, usable once a place is chosen
TEST(BoardButtonsTest, EndTurnTakesRollsPlaceAndConfirmIsApart) {
  const BoardButtons::Situation rolled{.yourMove = true, .canEndTurn = true};
  const auto after = BoardButtons::layout(256, 192, rolled);
  EXPECT_EQ(BoardButtons::at(after, 10, 180), BoardButtons::Button::EndTurn);
  EXPECT_FALSE(BoardButtons::at(after, 240, 180).has_value());
  BoardButtons::Situation placing = rolled;
  placing.placing = true;
  EXPECT_FALSE(BoardButtons::at(BoardButtons::layout(256, 192, placing), 240, 180).has_value());
  placing.placeChosen = true;
  const auto chosen = BoardButtons::layout(256, 192, placing);
  EXPECT_EQ(BoardButtons::at(chosen, 240, 180), BoardButtons::Button::Confirm);
  EXPECT_EQ(BoardButtons::at(chosen, 10, 180), BoardButtons::Button::EndTurn);
}

// Buttons coming and going fade: the first layout at once; Roll going fades
// out where it was (not usable) as End Turn fades in, and then it's gone
TEST(BoardButtonsTest, ButtonsFadeInAndOut) {
  BoardButtons::Fades fades;
  const BoardButtons::Situation before{.yourMove = true, .canRoll = true};
  const BoardButtons::Situation rolled{.yourMove = true, .canEndTurn = true};
  const auto opacityOf = [&](BoardButtons::Button button, double now) {
    for (const auto &[place, opacity] : fades.shown(now))
      if (place.button == button)
        return opacity;
    return -1.0f;
  };
  fades.update(BoardButtons::layout(256, 192, before), 10);
  EXPECT_FLOAT_EQ(opacityOf(BoardButtons::Button::Roll, 10), 1);
  EXPECT_FALSE(fades.fading(10));
  fades.update(BoardButtons::layout(256, 192, rolled), 20);
  const double half = 20 + BoardButtons::Fades::kSeconds / 2;
  EXPECT_NEAR(opacityOf(BoardButtons::Button::Roll, half), 0.5f, 1e-3f);
  EXPECT_NEAR(opacityOf(BoardButtons::Button::EndTurn, half), 0.5f, 1e-3f);
  EXPECT_TRUE(fades.fading(half));
  for (const auto &[place, opacity] : fades.shown(half))
    if (place.button == BoardButtons::Button::Roll) {
      EXPECT_FALSE(place.usable);
    }
  fades.update(BoardButtons::layout(256, 192, rolled), 21);
  EXPECT_LT(opacityOf(BoardButtons::Button::Roll, 21), 0); // (gone)
  EXPECT_FLOAT_EQ(opacityOf(BoardButtons::Button::EndTurn, 21), 1);
}

TEST(PickersTest, TheHomesPayIsAResourceChoice) {
  const State state = playUntil([](const State &s) {
    return s.phase == Phase::HomeIncome && Game::currentPlayer(s) != GameBots::kDealer;
  });
  ASSERT_EQ(state.phase, Phase::HomeIncome);
  const auto legal = Game::legalActions(state);
  const auto choice = Pickers::resourceChoiceOf(state, legal);
  ASSERT_TRUE(choice.has_value());
  EXPECT_EQ(choice->howMany, 1);
  const auto action = choice->actionFor({Resource::Fuel});
  ASSERT_TRUE(action.has_value());
  if (action) {
    EXPECT_EQ(action->kind, Kind::ChooseHome);
    EXPECT_EQ(action->a, static_cast<int>(Resource::Fuel));
    State played = state;
    EXPECT_TRUE(Game::apply(played, *action));
  }
  EXPECT_FALSE(choice->actionFor({}).has_value());
}

TEST(PickersTest, TheSupplyDropTakesTwoInEitherOrder) {
  // (the moves alone: a Supply drop's, as the rules list them)
  std::vector<Action> legal;
  for (int a = 0; a < kResources; ++a)
    for (int b = a; b < kResources; ++b)
      legal.push_back({Kind::PlaySupplyDrop, a, b});
  legal.push_back({Kind::EndTurn});
  const auto choice = Pickers::cardChoiceOf(legal, Kind::PlaySupplyDrop);
  ASSERT_TRUE(choice.has_value());
  EXPECT_EQ(choice->howMany, 2);
  const auto both = choice->actionFor({Resource::Water, Resource::Food});
  ASSERT_TRUE(both.has_value());
  EXPECT_EQ(both, (Action{Kind::PlaySupplyDrop, 1, 3}));
  EXPECT_EQ(choice->actionFor({Resource::Food, Resource::Food}),
            (Action{Kind::PlaySupplyDrop, 1, 1}));
  EXPECT_FALSE(Pickers::cardChoiceOf(legal, Kind::PlayCartel).has_value());
}

TEST(PickersTest, StealingNeedsAChoiceOnlyWithTwoOrMore) {
  State state = newGame(4);
  for (int p = 0; p < 4; ++p)
    state.player[static_cast<std::size_t>(p)].resources = {p, 1, 0, 0, 0};
  const std::vector<Action> two{
      {Kind::MoveBlockade, 5, 1}, {Kind::MoveBlockade, 5, 3}, {Kind::MoveBlockade, 6, 2}};
  const auto choice = Pickers::stealChoiceOf(state, two, 5);
  ASSERT_TRUE(choice.has_value());
  ASSERT_EQ(choice->options.size(), 2u);
  EXPECT_EQ(choice->options[0].player, 1);
  EXPECT_EQ(choice->options[0].cards, 2); // (1 Minerals, 1 Food)
  EXPECT_EQ(choice->options[1].cards, 4);
  EXPECT_FALSE(Pickers::stealChoiceOf(state, two, 6).has_value()); // (only one: no choice)
}
