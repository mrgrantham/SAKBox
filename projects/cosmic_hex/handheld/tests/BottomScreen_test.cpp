// The bottom screen in a game (BottomScreen), played by touch against real
// games: the home's pay chosen on the resource picker; Build, a row, Build,
// and where it goes chosen on the board; the board's own taps left to the
// app; nothing but the board when it isn't your move
#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/MoveSpots.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Kind = Action::Kind;
using Mode = BottomScreen::Mode;

namespace {
State playUntil(bool (*until)(const State &)) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  SeededRandom::Generator random(11);
  for (int guard = 0; guard < 4000 && !until(state) && state.winner < 0; ++guard)
    Game::apply(state, Game::currentPlayer(state) == GameBots::kDealer
                           ? Game::dealerAction(state, random)
                           : Game::legalActions(state).front());
  return state;
}
constexpr int kWidth = 256, kHeight = 192;
} // namespace

TEST(BottomScreenTest, TheHomesPayIsPickedByTouch) {
  const State state = playUntil([](const State &s) {
    return s.phase == Phase::HomeIncome && Game::currentPlayer(s) != GameBots::kDealer;
  });
  ASSERT_EQ(state.phase, Phase::HomeIncome);
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), true);
  EXPECT_EQ(screen.mode(), Mode::Resources);
  // Fuel is the third card (6 + 49 * 2, 40 down); then Take, bottom right
  EXPECT_FALSE(screen.tap(6 + 49 * 2 + 20, 60, std::nullopt).play.has_value());
  const auto taken = screen.tap(220, 178, std::nullopt);
  ASSERT_TRUE(taken.play.has_value());
  if (taken.play) {
    EXPECT_EQ(taken.play->kind, Kind::ChooseHome);
    EXPECT_EQ(taken.play->a, static_cast<int>(Resource::Fuel));
  }
  EXPECT_EQ(screen.mode(), Mode::Board);
}

TEST(BottomScreenTest, BuildThenChooseWhereOnTheBoard) {
  State state = playUntil([](const State &s) { return s.phase == Phase::Main; });
  ASSERT_EQ(state.phase, Phase::Main);
  state.player[static_cast<std::size_t>(state.turn)].resources = {3, 3, 3, 3, 3};
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), true);
  EXPECT_EQ(screen.mode(), Mode::Board);
  // Build, top right
  EXPECT_TRUE(screen.tap(240, 10, std::nullopt).used);
  ASSERT_EQ(screen.mode(), Mode::Build);
  // The first row that can be built, then Build
  const auto rows = buildRows(state);
  std::size_t row = 0;
  while (row < rows.size() && !rows[row].canBuild)
    ++row;
  ASSERT_LT(row, rows.size());
  screen.tap(128, static_cast<int>(6 + 39 * row + 10), std::nullopt);
  const auto built = screen.tap(220, 178, std::nullopt);
  if (rows[row].actions.size() == 1) {
    ASSERT_TRUE(built.play.has_value());
    EXPECT_EQ(*built.play, rows[row].actions.front());
    return;
  }
  // More than one place: back on the board with them marked; touching one
  // makes that move
  ASSERT_EQ(screen.mode(), Mode::Placing);
  const auto corners = screen.placingCorners();
  ASSERT_FALSE(corners.empty());
  const BoardPicture::Spot there{corners.front().hex, corners.front()};
  const auto placed = screen.tap(128, 96, there);
  ASSERT_TRUE(placed.play.has_value());
  if (placed.play) {
    EXPECT_TRUE(concerns(state, *placed.play, there));
  }
  EXPECT_EQ(screen.mode(), Mode::Board);
}

TEST(BottomScreenTest, TheBoardsOwnTapsAndOthersTurns) {
  const State state = playUntil([](const State &s) { return s.phase == Phase::Main; });
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), true);
  // A tap on the board away from the buttons: the app's
  EXPECT_FALSE(screen.tap(128, 96, std::nullopt).used);
  // Someone else's move: only the board, Build not usable
  screen.update(state, Game::legalActions(state), false);
  EXPECT_EQ(screen.mode(), Mode::Board);
  screen.tap(240, 10, std::nullopt);
  EXPECT_EQ(screen.mode(), Mode::Board);
  // Drawn: the board's pixels with the buttons over them
  Canvas canvas(kWidth, kHeight);
  std::vector<Canvas::Colour> board(static_cast<std::size_t>(kWidth * kHeight), {24, 32, 48, 255});
  screen.draw(canvas, board);
  EXPECT_EQ(canvas.at(128, 96).b, 48);
  EXPECT_NE(canvas.at(4, 4).b, 48); // (Show resources' button)
}

// Every button tap shows: a flash, and a message when it can't do anything
// yet (round 101: the user couldn't tell taps had landed)
TEST(BottomScreenTest, ButtonsAlwaysAnswer) {
  const State state = playUntil([](const State &s) { return s.phase == Phase::Main; });
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), false);
  EXPECT_TRUE(screen.tap(240, 10, std::nullopt).used); // Build, not your move
  EXPECT_EQ(screen.message(), "Build: on your turn");
  EXPECT_TRUE(screen.showingFeedback());
  screen.clearFeedback();
  EXPECT_FALSE(screen.showingFeedback());
  screen.tap(10, 10, std::nullopt); // Show resources
  EXPECT_FALSE(screen.message().empty());
  // Drawn: the message's band at the bottom
  Canvas canvas(kWidth, kHeight);
  std::vector<Canvas::Colour> board(static_cast<std::size_t>(kWidth * kHeight), {24, 32, 48, 255});
  screen.draw(canvas, board);
  EXPECT_NE(canvas.at(128, kHeight - 11).b, 48);
}

// Touching one of your ships marks where it can go; touching one moves it
TEST(BottomScreenTest, TouchAShipThenWhereItGoes) {
  State state = playUntil([](const State &s) { return s.phase == Phase::Main; });
  const auto legal = Game::legalActions(state);
  const auto move = std::find_if(legal.begin(), legal.end(),
                                 [](const Action &a) { return a.kind == Kind::Move; });
  ASSERT_NE(move, legal.end());
  const Corner from = state.ships[static_cast<std::size_t>(move->a)].corner;
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, legal, true);
  EXPECT_TRUE(screen.tap(128, 96, BoardPicture::Spot{from.hex, from}).used);
  ASSERT_EQ(screen.mode(), Mode::Placing);
  const Corner to = boardCorners()[static_cast<std::size_t>(move->b)];
  const auto marked = screen.placingCorners();
  EXPECT_NE(std::find(marked.begin(), marked.end(), to), marked.end());
  const auto moved = screen.tap(128, 96, BoardPicture::Spot{to.hex, to});
  ASSERT_TRUE(moved.play.has_value());
  if (moved.play) {
    EXPECT_EQ(moved.play->kind, Kind::Move);
    EXPECT_EQ(moved.play->b, move->b);
  }
}
