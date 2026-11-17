// The start menu (round 87): the rows by buttons and by touch, three players
// with their own boards, Play and Watch; and its picture (written out to
// look at, as the board's is)
#include <CosmicHex/Handheld/MenuPicture.hpp>
#include <CosmicHex/Handheld/StartMenu.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Key = StartMenu::Key;
using Outcome = StartMenu::Outcome;
using Row = StartMenu::Row;

namespace {
void writeOut(const MenuPicture &picture, const char *name) {
  const char *folder = std::getenv("TEST_UNDECLARED_OUTPUTS_DIR");
  if (!folder)
    return;
  std::ofstream file(std::string(folder) + "/" + name, std::ios::binary);
  file << "P6\n" << picture.width() << " " << picture.height() << "\n255\n";
  for (const auto &c : picture.pixels())
    file.put(static_cast<char>(c.r)).put(static_cast<char>(c.g)).put(static_cast<char>(c.b));
}
// The middle of the first button for this row and step (or Play/Watch)
std::pair<int, int> middleOf(const StartMenu &menu, Row row, int step, Outcome starts, int w,
                             int h) {
  for (const auto &b : menu.buttons(w, h))
    if (b.row == row && b.step == step && b.starts == starts)
      return {static_cast<int>(b.x + b.width / 2), static_cast<int>(b.y + b.height / 2)};
  ADD_FAILURE() << "no such button";
  return {0, 0};
}
} // namespace

TEST(StartMenuTest, StartsOnTheDefaults) {
  const StartMenu menu;
  EXPECT_EQ(menu.players(), 4);
  EXPECT_EQ(menu.boardName(), "R3");
  EXPECT_TRUE(menu.finishRound());
  EXPECT_EQ(menu.focus(), Row::Players);
}

TEST(StartMenuTest, ButtonsMoveAndChange) {
  StartMenu menu;
  EXPECT_EQ(menu.press(Key::Right), Outcome::None); // (players: 3)
  EXPECT_EQ(menu.players(), 3);
  EXPECT_EQ(menu.boardName(), "T4"); // (three players' default)
  menu.press(Key::Down);
  menu.press(Key::Right);
  EXPECT_EQ(menu.boardName(), "T5");
  menu.press(Key::Right);
  EXPECT_EQ(menu.boardName(), "T4"); // (round again)
  menu.press(Key::Down);
  menu.press(Key::Left);
  EXPECT_FALSE(menu.finishRound());
  menu.press(Key::Down); // (the last row: Play or Watch)
  EXPECT_EQ(menu.focus(), Row::Start);
  menu.press(Key::Right);
  EXPECT_TRUE(menu.watchChosen());
  EXPECT_EQ(menu.press(Key::Confirm), Outcome::Watch);
  menu.press(Key::Down); // (round to the top)
  EXPECT_EQ(menu.focus(), Row::Players);
  EXPECT_EQ(menu.press(Key::Confirm), Outcome::Play); // (A plays from any other row)
  EXPECT_EQ(menu.press(Key::Back), Outcome::Watch);   // (B watches)
  // Back to four players: R3 again
  menu.press(Key::Left);
  EXPECT_EQ(menu.players(), 4);
  EXPECT_EQ(menu.boardName(), "R3");
}

TEST(StartMenuTest, TouchesChangeRowsAndStart) {
  for (const auto &[w, h] : {std::pair{320, 240}, std::pair{256, 192}}) {
    StartMenu menu;
    auto [x, y] = middleOf(menu, Row::Board, +1, Outcome::None, w, h);
    EXPECT_EQ(menu.touch(x, y, w, h), Outcome::None);
    EXPECT_EQ(menu.boardName(), "S5");
    EXPECT_EQ(menu.focus(), Row::Board);
    std::tie(x, y) = middleOf(menu, Row::Start, 0, Outcome::Watch, w, h);
    EXPECT_EQ(menu.touch(x, y, w, h), Outcome::Watch);
    std::tie(x, y) = middleOf(menu, Row::Start, 0, Outcome::Play, w, h);
    EXPECT_EQ(menu.touch(x, y, w, h), Outcome::Play);
    EXPECT_EQ(menu.touch(0, 0, w, h), Outcome::None); // (a margin: nothing)
    // Every button on the screen, none overlapping another
    const auto buttons = menu.buttons(w, h);
    for (std::size_t i = 0; i < buttons.size(); ++i) {
      const auto &a = buttons[i];
      EXPECT_GE(a.x, 0);
      EXPECT_LE(a.x + a.width, static_cast<float>(w));
      EXPECT_LE(a.y + a.height, static_cast<float>(h));
      for (std::size_t j = i + 1; j < buttons.size(); ++j) {
        const auto &b = buttons[j];
        const bool apart = a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y ||
                           b.y + b.height <= a.y;
        EXPECT_TRUE(apart) << i << " and " << j;
      }
    }
  }
}

TEST(StartMenuTest, ThePictureDraws) {
  StartMenu menu;
  MenuPicture big(320, 240), small(256, 192);
  big.draw(menu);
  small.draw(menu);
  writeOut(big, "menu_3ds.ppm");
  writeOut(small, "menu_ds.ppm");
  menu.press(Key::Right);
  menu.press(Key::Down);
  menu.press(Key::Down);
  menu.press(Key::Down);
  big.draw(menu);
  writeOut(big, "menu_3ds_three_players.ppm");
  // Something's drawn in the chosen colour (the focused row's words)
  bool amber = false;
  for (const auto &c : big.pixels())
    amber = amber || (c.r == 255 && c.g == 214 && c.b == 90);
  EXPECT_TRUE(amber);
}
