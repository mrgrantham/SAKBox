// Typing on the keyboard: by touch and by pad, capitals, spaces, deleting,
// the most letters, and Done
#include <ScratchPadd/OnScreenKeyboard/OnScreenKeyboard.hpp>

#include <gtest/gtest.h>

using ScratchPadd::OnScreenKeyboard;

namespace {
// The middle of a key on a DS's screen (the keyboard's layout)
std::pair<int, int> keyAt(int row, int column) {
  return {8 + 12 * row + 24 * column + 11, 44 + 30 * row + 13};
}
} // namespace

TEST(OnScreenKeyboardTest, TouchTypesAName) {
  OnScreenKeyboard keyboard;
  EXPECT_TRUE(keyboard.shifted());   // (the first letter a capital)
  const auto [jx, jy] = keyAt(1, 6); // J
  keyboard.touch(jx, jy, 0);
  const auto [ox, oy] = keyAt(0, 8); // O
  keyboard.touch(ox, oy, 0);
  EXPECT_EQ(keyboard.text(), "Jo");
  EXPECT_FALSE(keyboard.shifted());
  keyboard.touch(170, 150, 0); // Del
  EXPECT_EQ(keyboard.text(), "J");
  keyboard.touch(220, 150, 0); // Done
  EXPECT_EQ(keyboard.outcome(), OnScreenKeyboard::Outcome::Done);
}

TEST(OnScreenKeyboardTest, ThePadMovesAndTypes) {
  OnScreenKeyboard keyboard("A", 3);
  keyboard.press(OnScreenKeyboard::Key::Confirm, 0); // (Q, focused first)
  keyboard.press(OnScreenKeyboard::Key::Right, 0);
  keyboard.press(OnScreenKeyboard::Key::Confirm, 0); // W
  EXPECT_EQ(keyboard.text(), "Aqw");
  keyboard.press(OnScreenKeyboard::Key::Confirm, 0); // (three at most)
  EXPECT_EQ(keyboard.text(), "Aqw");
  keyboard.press(OnScreenKeyboard::Key::Back, 0);
  EXPECT_EQ(keyboard.text(), "Aq");
  keyboard.press(OnScreenKeyboard::Key::Done, 0);
  EXPECT_EQ(keyboard.outcome(), OnScreenKeyboard::Outcome::Done);
}

TEST(OnScreenKeyboardTest, SpacesInsideButNotAtTheEnds) {
  OnScreenKeyboard keyboard;
  keyboard.touch(100, 150, 0); // Space, before anything: nothing
  EXPECT_EQ(keyboard.text(), "");
  const auto [ax, ay] = keyAt(1, 0); // A
  keyboard.touch(ax, ay, 0);
  keyboard.touch(100, 150, 0);
  EXPECT_EQ(keyboard.text(), "A ");
  keyboard.touch(220, 150, 0); // Done drops the space at the end
  EXPECT_EQ(keyboard.text(), "A");
}

TEST(OnScreenKeyboardTest, ItDraws) {
  OnScreenKeyboard keyboard("Nova");
  ScratchPadd::Canvas canvas(256, 192);
  keyboard.draw(canvas, {}, 0);
  EXPECT_NE(canvas.pixels()[20 * 256 + 16], canvas.pixels()[0]); // (the field's outline)
}
