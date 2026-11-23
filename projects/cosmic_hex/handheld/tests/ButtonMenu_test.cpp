// The title's and the pause menu's buttons (ButtonMenu) and their picture
// (MenuPicture): the focus kept on a button, a touch moving it there, an
// empty menu choosing nothing, and the focused button drawn outlined
#include <CosmicHex/Handheld/ButtonMenu.hpp>
#include <CosmicHex/Handheld/MenuPicture.hpp>
#include <CosmicHex/Handheld/Palette.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

using namespace CosmicHex::Handheld;

namespace {
// How many of the picture's pixels inside `button` are the highlight's
// colour (the focus's outline and words)
int highlightIn(const MenuPicture &picture, const ButtonMenu::Button &button) {
  int found = 0;
  const auto pixels = picture.pixels();
  for (int y = static_cast<int>(button.y); y < static_cast<int>(button.y + button.height); ++y)
    for (int x = static_cast<int>(button.x); x < static_cast<int>(button.x + button.width); ++x) {
      const auto &c =
          pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(picture.width()) +
                 static_cast<std::size_t>(x)];
      found += c.r == Palette::kHighlight.r && c.g == Palette::kHighlight.g &&
                       c.b == Palette::kHighlight.b
                   ? 1
                   : 0;
    }
  return found;
}
} // namespace

// The focus set out of range goes back to the first; A chooses the one in
// focus
TEST(ButtonMenuTest, TheFocusStaysOnAButton) {
  ButtonMenu menu({"Play", "Settings", "Quit"});
  menu.setFocus(2);
  EXPECT_EQ(menu.press(ButtonMenu::Key::Confirm), 2u);
  menu.setFocus(7);
  EXPECT_EQ(menu.focus(), 0u);
  menu.press(ButtonMenu::Key::Down);
  menu.press(ButtonMenu::Key::Down);
  menu.press(ButtonMenu::Key::Down); // (round to the top)
  EXPECT_EQ(menu.press(ButtonMenu::Key::Confirm), 0u);
}

// A touch on a button moves the focus there, so A then chooses it too
TEST(ButtonMenuTest, ATouchMovesTheFocus) {
  ButtonMenu menu({"Resume", "Restart", "Title", "Quit"});
  const auto buttons = menu.buttons(256, 192);
  const ButtonMenu::Button &title = buttons[2];
  EXPECT_EQ(menu.touch(static_cast<int>(title.x + 4), static_cast<int>(title.y + 4), 256, 192), 2u);
  EXPECT_EQ(menu.focus(), 2u);
  EXPECT_EQ(menu.press(ButtonMenu::Key::Confirm), 2u);
}

// A menu without buttons chooses nothing, by buttons or touch
TEST(ButtonMenuTest, AnEmptyMenuChoosesNothing) {
  ButtonMenu menu({});
  for (const auto key : {ButtonMenu::Key::Up, ButtonMenu::Key::Down, ButtonMenu::Key::Confirm})
    EXPECT_FALSE(menu.press(key).has_value());
  EXPECT_TRUE(menu.buttons(256, 192).empty());
  EXPECT_FALSE(menu.touch(128, 96, 256, 192).has_value());
}

// The picture: the button in focus outlined in the highlight, the others
// not, on each console's screen
TEST(MenuPictureTest, TheFocusedButtonIsOutlined) {
  for (const auto &[width, height] : {std::pair{256, 192}, std::pair{320, 240}}) {
    ButtonMenu menu({"Resume", "Restart", "Title", "Quit"});
    menu.setFocus(1);
    MenuPicture picture(width, height);
    picture.draw(menu);
    const auto buttons = menu.buttons(width, height);
    for (std::size_t i = 0; i < buttons.size(); ++i) {
      if (i == 1) {
        EXPECT_GT(highlightIn(picture, buttons[i]), 0) << width;
      } else {
        EXPECT_EQ(highlightIn(picture, buttons[i]), 0) << width << ": button " << i;
      }
    }
  }
}
