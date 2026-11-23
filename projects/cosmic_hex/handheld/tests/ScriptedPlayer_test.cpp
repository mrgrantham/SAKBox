// The scripted player (ScriptedPlayer) on each of the touch panel's
// screens: the dice thrown when they wait, a marked place touched then
// Confirm, the build screen gone down to the row it chose, the turn ended
// after three tries, a card picked or kept, an offer answered, Back when it
// gets nowhere; and the same seed, the same inputs
#include <CosmicHex/Handheld/ScriptedPlayer.hpp>

#include "TestTables.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Key = BottomScreen::Key;
using Mode = BottomScreen::Mode;
using Button = BoardButtons::Button;

namespace {

constexpr int kWidth = 256, kHeight = 192;

// A touch panel and its map, as ScriptedGame has them (the map not drawn:
// only where things are)
struct Panel {
  BottomScreen screen{kWidth, kHeight};
  BoardPicture map{kWidth, kHeight, ScratchPadd::TextureRecipe::Smoothing::Coarse};
};

// An input in words, to compare and to read in a failure
std::string said(const ScriptedInput &input) {
  switch (input.kind) {
  case ScriptedInput::Kind::None:
    return "nothing";
  case ScriptedInput::Kind::Tap:
    return "tap " + std::to_string(input.x) + "," + std::to_string(input.y);
  case ScriptedInput::Kind::Press:
    return "press " + std::to_string(static_cast<int>(input.key));
  }
  return "?";
}
std::string tapOn(std::array<int, 2> at) {
  return "tap " + std::to_string(at[0]) + "," + std::to_string(at[1]);
}
std::string pressOf(Key key) { return said({.kind = ScriptedInput::Kind::Press, .key = key}); }
// A tap on the middle of a place on the map (as the player rounds it)
ScriptedInput tapAt(HexGrid::Point point) {
  return {.kind = ScriptedInput::Kind::Tap,
          .x = static_cast<int>(std::lround(point.x)),
          .y = static_cast<int>(std::lround(point.y))};
}

// Your move in `state`, its legal moves the snapshot's and the panel's
TableSnapshot yourMoveIn(const State &state, Panel &panel) {
  TableSnapshot snapshot = TestTables::snapshotWith(state);
  snapshot.yourMove = true;
  snapshot.moves = Game::legalActions(state);
  panel.screen.update(state, snapshot.moves, true);
  return snapshot;
}

// Salvage's two to pick: the resource picker open
State salvaging() {
  State state = newGame(4, agreedRules(boardR3(), 4));
  state.phase = Phase::Salvage;
  state.salvageLeft = 2;
  return state;
}

} // namespace

// Not your move and no roll waiting: nothing to do
TEST(ScriptedPlayerTest, NothingWhileItIsntYourMove) {
  Panel panel;
  const State state = TestTables::atFirstMain();
  panel.screen.update(state, {}, false);
  ScriptedPlayer player(1);
  EXPECT_EQ(said(player.next(TestTables::snapshotWith(state), panel.screen, panel.map, false)),
            "nothing");
}

// A screen to look at (another player's panel) is closed with Back, whoever's
// move it is: this player doesn't look
TEST(ScriptedPlayerTest, ScreensToLookAtAreClosed) {
  Panel panel;
  const State state = TestTables::atFirstMain();
  panel.screen.update(state, {}, false);
  const auto others = panel.screen.buttonAt(Button::OtherPlayers);
  ASSERT_TRUE(others.has_value());
  panel.screen.tap((*others)[0], (*others)[1], std::nullopt);
  ASSERT_EQ(panel.screen.mode(), Mode::Others);
  ScriptedPlayer player(1);
  EXPECT_EQ(said(player.next(TestTables::snapshotWith(state), panel.screen, panel.map, false)),
            pressOf(Key::Back));
}

// Your roll waiting: Roll touched, which throws the dice
TEST(ScriptedPlayerTest, RollsWhenTheDiceWait) {
  TableGame game(TestTables::yourTable(3));
  ASSERT_TRUE(TestTables::playedOnUntil(
      game, [](const TableGame &g) { return g.waitingForDice() && g.state().turn == 0; }));
  const TableSnapshot snapshot = snapshotOf(game, 0, false, 3);
  Panel panel;
  panel.screen.update(snapshot.state, snapshot.moves, snapshot.yourMove, true);
  const auto roll = panel.screen.buttonAt(Button::Roll);
  ASSERT_TRUE(roll.has_value());
  ScriptedPlayer player(3);
  const ScriptedInput input = player.next(snapshot, panel.screen, panel.map, true);
  EXPECT_EQ(said(input), tapOn(*roll));
  EXPECT_TRUE(panel.screen.tap(input.x, input.y, std::nullopt).roll);
}

// The start's placing: a marked corner touched, then Confirm
TEST(ScriptedPlayerTest, PlacesOnAMarkedCornerThenConfirms) {
  TableGame game(TestTables::yourTable(4));
  ASSERT_TRUE(TestTables::playedOnUntil(game, [](const TableGame &g) {
    return g.waitingForPerson() && g.state().phase == Phase::PlaceColony;
  }));
  const TableSnapshot snapshot = snapshotOf(game, 0, false, 4);
  Panel panel;
  panel.screen.update(snapshot.state, snapshot.moves, true);
  ASSERT_EQ(panel.screen.mode(), Mode::Placing);
  ScriptedPlayer player(4);
  const ScriptedInput touch = player.next(snapshot, panel.screen, panel.map, false);
  const std::vector<Corner> marked = panel.screen.placingCorners();
  const auto touched = std::find_if(marked.begin(), marked.end(), [&](const Corner &corner) {
    return said(touch) == said(tapAt(panel.map.where(corner)));
  });
  ASSERT_NE(touched, marked.end()) << said(touch) << " is on no marked corner";
  panel.screen.tap(touch.x, touch.y, BoardPicture::Spot{touched->hex, *touched});
  ASSERT_TRUE(panel.screen.chosenPlacement().has_value());
  EXPECT_EQ(said(player.next(snapshot, panel.screen, panel.map, false)), pressOf(Key::Confirm));
}

// Something to build: Build touched, then down the build screen to the row
// chosen (one it can build), then Build
TEST(ScriptedPlayerTest, BuildsARowItCanBuild) {
  State state = TestTables::atFirstMain();
  state.player[static_cast<std::size_t>(state.turn)].resources = {5, 5, 5, 5, 5};
  Panel panel;
  const TableSnapshot snapshot = yourMoveIn(state, panel);
  const auto build = panel.screen.buttonAt(Button::Build);
  ASSERT_TRUE(build.has_value());
  ScriptedPlayer player(2);
  // (each try on the board builds four times in five)
  ScriptedInput input;
  for (int tries = 0; tries < 3 && said(input) != tapOn(*build); ++tries)
    input = player.next(snapshot, panel.screen, panel.map, false);
  ASSERT_EQ(said(input), tapOn(*build));
  panel.screen.tap(input.x, input.y, std::nullopt);
  ASSERT_EQ(panel.screen.mode(), Mode::Build);
  std::size_t downs = 0;
  for (input = player.next(snapshot, panel.screen, panel.map, false);
       said(input) == pressOf(Key::Down) && downs < 10;
       input = player.next(snapshot, panel.screen, panel.map, false))
    ++downs;
  EXPECT_EQ(said(input), pressOf(Key::Confirm));
  const std::vector<BuildRow> rows = buildRows(state);
  ASSERT_GE(downs, 1u); // (the first press chooses the top row)
  ASSERT_LE(downs, rows.size());
  EXPECT_TRUE(rows[downs - 1].canBuild);
}

// Three tries on the board in a turn (a build, a ship touched), then End
// Turn
TEST(ScriptedPlayerTest, EndsTheTurnAfterThreeTries) {
  const State state = TestTables::atFirstMain();
  Panel panel;
  const TableSnapshot snapshot = yourMoveIn(state, panel);
  const auto endTurn = panel.screen.buttonAt(Button::EndTurn);
  ASSERT_TRUE(endTurn.has_value());
  ScriptedPlayer player(5);
  for (int tries = 0; tries < 3; ++tries)
    player.next(snapshot, panel.screen, panel.map, false);
  EXPECT_EQ(said(player.next(snapshot, panel.screen, panel.map, false)), tapOn(*endTurn));
}

// The resource picker: the card in focus picked, or the next one along
TEST(ScriptedPlayerTest, PicksTheCardInFocusOrMovesOn) {
  Panel panel;
  const TableSnapshot snapshot = yourMoveIn(salvaging(), panel);
  ASSERT_EQ(panel.screen.mode(), Mode::Resources);
  ScriptedPlayer player(6);
  int picks = 0, moves = 0;
  for (int i = 0; i < 12; ++i) {
    const std::string input = said(player.next(snapshot, panel.screen, panel.map, false));
    picks += input == pressOf(Key::Confirm) ? 1 : 0;
    moves += input == pressOf(Key::Right) ? 1 : 0;
  }
  EXPECT_EQ(picks + moves, 12);
  EXPECT_GT(picks, 0);
  EXPECT_GT(moves, 0);
}

// After buying, one of the two dealt chosen, then kept
TEST(ScriptedPlayerTest, ChoosesACardToKeepThenKeepsIt) {
  const State state =
      TestTables::playedBuyingUntil([](const State &s) { return s.phase == Phase::KeepContract; });
  ASSERT_EQ(state.phase, Phase::KeepContract);
  Panel panel;
  const TableSnapshot snapshot = yourMoveIn(state, panel);
  ASSERT_EQ(panel.screen.mode(), Mode::Keep);
  ScriptedPlayer player(7);
  EXPECT_EQ(said(player.next(snapshot, panel.screen, panel.map, false)), pressOf(Key::Right));
  panel.screen.press(Key::Right);
  ASSERT_GE(panel.screen.keepChosen(), 0);
  EXPECT_EQ(said(player.next(snapshot, panel.screen, panel.map, false)), pressOf(Key::Confirm));
}

// An offer to you: accepted at first, declined if that got nowhere
TEST(ScriptedPlayerTest, AnswersAnOfferThenDeclinesIfStuck) {
  const State state = TestTables::offeredToYou();
  ASSERT_EQ(state.phase, Phase::TradeReply);
  Panel panel;
  const TableSnapshot snapshot = yourMoveIn(state, panel);
  ASSERT_EQ(panel.screen.mode(), Mode::Trade);
  ScriptedPlayer player(8);
  EXPECT_EQ(said(player.next(snapshot, panel.screen, panel.map, false)), pressOf(Key::Confirm));
  EXPECT_EQ(said(player.next(snapshot, panel.screen, panel.map, false)), pressOf(Key::Back));
}

// The same screen for many inputs: Back, to get out of whatever isn't working
TEST(ScriptedPlayerTest, BacksOutWhenItGetsNowhere) {
  Panel panel;
  const TableSnapshot snapshot = yourMoveIn(salvaging(), panel);
  ScriptedPlayer player(9);
  for (int i = 0; i < 13; ++i)
    EXPECT_NE(said(player.next(snapshot, panel.screen, panel.map, false)), pressOf(Key::Back))
        << "input " << i;
  EXPECT_EQ(said(player.next(snapshot, panel.screen, panel.map, false)), pressOf(Key::Back));
}

// Its choices are by its seed: the same seed, the same inputs
TEST(ScriptedPlayerTest, TheSameSeedMakesTheSameInputs) {
  const auto inputsOf = [](std::uint64_t seed) {
    Panel panel;
    const TableSnapshot snapshot = yourMoveIn(TestTables::atFirstMain(), panel);
    ScriptedPlayer player(seed);
    std::vector<std::string> inputs;
    inputs.reserve(24);
    for (int i = 0; i < 12; ++i)
      inputs.push_back(said(player.next(snapshot, panel.screen, panel.map, false)));
    Panel picker;
    const TableSnapshot salvage = yourMoveIn(salvaging(), picker);
    for (int i = 0; i < 12; ++i)
      inputs.push_back(said(player.next(salvage, picker.screen, picker.map, false)));
    return inputs;
  };
  EXPECT_EQ(inputsOf(10), inputsOf(10));
  EXPECT_NE(inputsOf(10), inputsOf(11));
}
