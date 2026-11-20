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
  std::vector<Canvas::Colour> board(static_cast<std::size_t>(kWidth * kHeight), {24, 32, 48, 255});
  Canvas canvas = Canvas::over(board, kWidth, kHeight);
  screen.drawOverBoard(canvas);
  EXPECT_EQ(canvas.at(128, 96).b, 48);
  EXPECT_NE(canvas.at(4, 4).b, 48); // (Show resources' button)
}

// Every button tap shows: a glow, and a message when it can't do anything
// yet (the user couldn't tell taps had landed)
TEST(BottomScreenTest, ButtonsAlwaysAnswer) {
  const State state = playUntil([](const State &s) { return s.phase == Phase::Main; });
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), false);
  EXPECT_TRUE(screen.tap(240, 10, std::nullopt).used); // Build, not your move
  EXPECT_EQ(screen.message(), "Build: on your turn");
  EXPECT_TRUE(screen.showingFeedback());
  screen.clearFeedback();
  EXPECT_FALSE(screen.showingFeedback());
  screen.tap(241, 36, std::nullopt); // Trade (its screen still to come)
  EXPECT_FALSE(screen.message().empty());
  // Drawn: the message's band at the bottom
  std::vector<Canvas::Colour> board(static_cast<std::size_t>(kWidth * kHeight), {24, 32, 48, 255});
  Canvas canvas = Canvas::over(board, kWidth, kHeight);
  screen.drawOverBoard(canvas);
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

// The D-pad, A and B on a choosing screen: the home's
// pay picked with Right, Right, A and taken with A; B leaves the build screen
TEST(BottomScreenTest, TheButtonsWorkTheChoosingScreens) {
  const State state = playUntil([](const State &s) {
    return s.phase == Phase::HomeIncome && Game::currentPlayer(s) != GameBots::kDealer;
  });
  ASSERT_EQ(state.phase, Phase::HomeIncome);
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), true);
  using Key = BottomScreen::Key;
  screen.press(Key::Right);
  screen.press(Key::Right);
  EXPECT_FALSE(screen.press(Key::Confirm).play.has_value()); // (picked)
  const auto taken = screen.press(Key::Confirm);
  ASSERT_TRUE(taken.play.has_value());
  if (taken.play) {
    EXPECT_EQ(taken.play->a, static_cast<int>(Resource::Fuel));
  }
  // On the board the buttons are the app's
  EXPECT_FALSE(screen.press(Key::Down).used);
  const State main = playUntil([](const State &s) { return s.phase == Phase::Main; });
  screen.update(main, Game::legalActions(main), true);
  screen.tap(240, 10, std::nullopt);
  ASSERT_EQ(screen.mode(), Mode::Build);
  EXPECT_TRUE(screen.press(Key::Back).used);
  EXPECT_EQ(screen.mode(), Mode::Board);
}

// Show resources opens and closes, your move or not
TEST(BottomScreenTest, ShowResourcesAnyTime) {
  const State state = playUntil([](const State &s) { return s.phase == Phase::Main; });
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), false);
  EXPECT_EQ(screen.info(), BottomScreen::Info::None);
  screen.tap(10, 10, std::nullopt);
  EXPECT_EQ(screen.info(), BottomScreen::Info::Resources);
  screen.update(state, Game::legalActions(state), true); // (stays open as the game goes on)
  EXPECT_EQ(screen.info(), BottomScreen::Info::Resources);
  screen.tap(10, 10, std::nullopt);
  EXPECT_EQ(screen.info(), BottomScreen::Info::None);
}

// Your roll waits for you: Roll shows only then, and a tap on
// it says to throw the dice
TEST(BottomScreenTest, RollWhenYourRollWaits) {
  const State state = playUntil([](const State &s) { return s.phase == Phase::Main; });
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), false, false);
  EXPECT_FALSE(screen.tap(10, 178, std::nullopt).roll);
  screen.update(state, Game::legalActions(state), false, true);
  const auto rolled = screen.tap(10, 178, std::nullopt);
  EXPECT_TRUE(rolled.roll);
  EXPECT_TRUE(rolled.used);
}

// The start's placing: every place marked; a touch chooses (nothing played
// yet), End Turn places it; B takes the choice back, and End Turn then only
// says to touch a corner first
TEST(BottomScreenTest, AtTheStartATouchChoosesAndEndTurnPlaces) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  ASSERT_EQ(state.phase, Phase::PlaceColony);
  const std::vector<Action> legal = Game::legalActions(state);
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, legal, true);
  EXPECT_EQ(screen.mode(), Mode::Placing);
  EXPECT_EQ(screen.placingCorners().size(), legal.size());
  const auto endTurn = [&] {
    for (const auto &place : BoardButtons::layout(
             kWidth, kHeight, BoardButtons::Situation{.yourMove = true, .placing = true}))
      if (place.button == BoardButtons::Button::EndTurn)
        return place;
    ADD_FAILURE() << "no End Turn while placing";
    return BoardButtons::Place{};
  }();
  const auto tapEndTurn = [&] {
    return screen.tap(static_cast<int>(endTurn.x + endTurn.size / 2),
                      static_cast<int>(endTurn.y + endTurn.size / 2), std::nullopt);
  };
  // End Turn before a touch: nothing placed
  EXPECT_FALSE(tapEndTurn().play.has_value());
  EXPECT_FALSE(screen.message().empty());
  // A touch on a marked corner: chosen, not played
  const Corner corner = boardCorners()[static_cast<std::size_t>(legal[2].a)];
  const auto touched = screen.tap(128, 96, BoardPicture::Spot{corner.hex, corner});
  EXPECT_FALSE(touched.play.has_value());
  ASSERT_TRUE(screen.startPlacement().has_value());
  EXPECT_EQ(*screen.startPlacement(), legal[2]);
  // B takes it back; touched again, End Turn places it
  screen.press(BottomScreen::Key::Back);
  EXPECT_FALSE(screen.startPlacement().has_value());
  screen.tap(128, 96, BoardPicture::Spot{corner.hex, corner});
  const auto placed = tapEndTurn();
  ASSERT_TRUE(placed.play.has_value());
  if (placed.play) {
    EXPECT_EQ(*placed.play, legal[2]);
  }
  EXPECT_FALSE(screen.startPlacement().has_value());
}

namespace {
// A game played until `until`, buying a card whenever one can be bought
State playBuying(bool (*until)(const State &)) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  SeededRandom::Generator random(23);
  for (int guard = 0; guard < 6000 && !until(state) && state.winner < 0; ++guard) {
    if (Game::currentPlayer(state) == GameBots::kDealer) {
      Game::apply(state, Game::dealerAction(state, random));
      continue;
    }
    const std::vector<Action> legal = Game::legalActions(state);
    const auto buy = std::find_if(legal.begin(), legal.end(),
                                  [](const Action &a) { return a.kind == Kind::BuyContract; });
    Game::apply(state, buy != legal.end() ? *buy : legal.front());
  }
  return state;
}
} // namespace

// After buying: the two cards dealt open by themselves; choosing one and
// Keep keeps it
TEST(BottomScreenTest, AfterBuyingOneOfTwoIsKept) {
  const State state = playBuying([](const State &s) { return s.phase == Phase::KeepContract; });
  ASSERT_EQ(state.phase, Phase::KeepContract);
  const std::vector<Action> legal = Game::legalActions(state);
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, legal, true);
  EXPECT_EQ(screen.mode(), Mode::Keep);
  EXPECT_FALSE(screen.press(BottomScreen::Key::Confirm).play.has_value()); // (none chosen)
  screen.press(BottomScreen::Key::Right);
  const auto kept = screen.press(BottomScreen::Key::Confirm);
  ASSERT_TRUE(kept.play.has_value());
  if (kept.play) {
    EXPECT_EQ(kept.play->kind, Kind::KeepContract);
  }
}

// The cards screen: Cards opens it; a card chosen and Play plays it (an
// Enforcer here, given to the player)
TEST(BottomScreenTest, TheCardsScreenPlaysACard) {
  State state = playUntil([](const State &s) {
    return s.phase == Phase::Main && Game::currentPlayer(s) != GameBots::kDealer;
  });
  ASSERT_EQ(state.phase, Phase::Main);
  state.player[static_cast<std::size_t>(state.turn)].contracts.push_back(kEnforcerCard);
  const std::vector<Action> legal = Game::legalActions(state);
  const bool playable = std::any_of(legal.begin(), legal.end(),
                                    [](const Action &a) { return a.kind == Kind::PlayEnforcer; });
  ASSERT_TRUE(playable) << "an Enforcer should be playable in the main part of a turn";
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, legal, true);
  BoardButtons::Situation situation = BoardButtons::situationOf(state, legal, true);
  for (const auto &place : BoardButtons::layout(kWidth, kHeight, situation))
    if (place.button == BoardButtons::Button::Cards)
      screen.tap(static_cast<int>(place.x + place.size / 2),
                 static_cast<int>(place.y + place.size / 2), std::nullopt);
  EXPECT_EQ(screen.mode(), Mode::Cards);
  screen.press(BottomScreen::Key::Right); // (the first card)
  const auto played = screen.press(BottomScreen::Key::Confirm);
  EXPECT_EQ(played.play.has_value(), playable);
  if (played.play) {
    EXPECT_EQ(played.play->kind, Kind::PlayEnforcer);
  }
  screen.press(BottomScreen::Key::Back);
  EXPECT_EQ(screen.mode(), Mode::Board);
}

// The other players' tabs: Other players opens them on the first other
// player, the top screen told whose; a new look at the game (as holding it
// for the view brings) leaves them open; left and right go through them;
// Back closes them
TEST(BottomScreenTest, TheOtherPlayersTabsStayOpenUntilBack) {
  const State state = newGame(4, agreedRules(boardR3(), 4));
  const std::vector<Action> legal = Game::legalActions(state);
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, legal, true);
  for (const auto &place :
       BoardButtons::layout(kWidth, kHeight, BoardButtons::situationOf(state, legal, true)))
    if (place.button == BoardButtons::Button::OtherPlayers)
      screen.tap(static_cast<int>(place.x + place.size / 2),
                 static_cast<int>(place.y + place.size / 2), std::nullopt);
  EXPECT_EQ(screen.mode(), Mode::Others);
  EXPECT_EQ(screen.info(), BottomScreen::Info::Player);
  EXPECT_EQ(screen.infoPlayer(), 1);
  screen.update(state, legal, true); // (the start's placing would take the screen)
  EXPECT_EQ(screen.mode(), Mode::Others);
  screen.press(BottomScreen::Key::Right);
  EXPECT_EQ(screen.infoPlayer(), 2);
  screen.press(BottomScreen::Key::Back);
  EXPECT_EQ(screen.info(), BottomScreen::Info::None);
  EXPECT_NE(screen.mode(), Mode::Others);
}

// The trade room: Trade opens it on your trading; a bank trade touched in
// (a kind given, a kind got) and Trade ends the turn's offers to players,
// then (once the rules allow it) makes the trade
TEST(BottomScreenTest, TheTradeRoomMakesABankTrade) {
  State state = newGame(4, agreedRules(boardR3(), 4), 0);
  SeededRandom::Generator random(41);
  for (int guard = 0; guard < 4000; ++guard) {
    if (state.phase == Phase::Trading && state.turn == 0 && state.round > 2)
      break;
    Game::apply(state, Game::currentPlayer(state) == GameBots::kDealer
                           ? Game::dealerAction(state, random)
                           : Game::legalActions(state).front());
  }
  ASSERT_EQ(state.phase, Phase::Trading);
  state.player[0].resources = {6, 0, 0, 0, 0};
  BottomScreen screen(kWidth, kHeight);
  screen.update(state, Game::legalActions(state), true);
  for (const auto &place : BoardButtons::layout(
           kWidth, kHeight, BoardButtons::situationOf(state, Game::legalActions(state), true)))
    if (place.button == BoardButtons::Button::Trade)
      screen.tap(static_cast<int>(place.x + place.size / 2),
                 static_cast<int>(place.y + place.size / 2), std::nullopt);
  ASSERT_EQ(screen.mode(), Mode::Trade);
  // Minerals given (the first card of the give row, the lower), Food got
  // (the second of the get row, the upper): the cards' middles
  screen.tap(6 + 24, 102 + 20, std::nullopt);
  screen.tap(6 + 49 + 24, 34 + 20, std::nullopt);
  const auto first = screen.press(BottomScreen::Key::Confirm);
  ASSERT_TRUE(first.play.has_value());
  if (first.play) {
    EXPECT_EQ(first.play->kind, Kind::EndTrading);
    ASSERT_TRUE(Game::apply(state, *first.play));
  }
  screen.update(state, Game::legalActions(state), true);
  const auto next = screen.nextQueued();
  ASSERT_TRUE(next.has_value());
  if (next) {
    EXPECT_EQ(next->kind, Kind::Trade);
    EXPECT_TRUE(Game::apply(state, *next));
    EXPECT_EQ(state.player[0].resources[1], 1);
  }
}
