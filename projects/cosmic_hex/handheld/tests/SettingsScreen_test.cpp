// The settings: Change opens the keyboard, Done gives the new name, Back
// leaves; an empty or unchanged name is kept
#include <CosmicHex/Handheld/SettingsScreen.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex::Handheld;
using Key = SettingsScreen::Key;
using Outcome = SettingsScreen::Outcome;

TEST(SettingsScreenTest, ChangeYourName) {
  SettingsScreen settings(256, 192);
  settings.open("James", true);
  EXPECT_EQ(settings.press(Key::Confirm, 0), Outcome::None); // (Change: the keyboard)
  ASSERT_TRUE(settings.typing());
  for (int i = 0; i < 5; ++i)
    settings.press(Key::Back, 0);  // (James deleted)
  settings.press(Key::Confirm, 0); // Q (the first letter a capital)
  EXPECT_EQ(settings.press(Key::Done, 0), Outcome::NameChanged);
  EXPECT_FALSE(settings.typing());
  EXPECT_EQ(settings.name(), "Q");
}

TEST(SettingsScreenTest, AnEmptyNameIsKeptAndBackLeaves) {
  SettingsScreen settings(256, 192);
  settings.open("Nova", false);
  settings.touch(60, 98, 0); // Change
  ASSERT_TRUE(settings.typing());
  for (int i = 0; i < 4; ++i)
    settings.press(Key::Back, 0);
  EXPECT_EQ(settings.press(Key::Done, 0), Outcome::None); // (nothing typed: kept)
  EXPECT_EQ(settings.name(), "Nova");
  EXPECT_EQ(settings.touch(60, 170, 0), Outcome::Back);
  EXPECT_EQ(settings.press(Key::Back, 0), Outcome::Back);
}

TEST(SettingsScreenTest, ItDraws) {
  SettingsScreen settings(320, 240);
  settings.open("James", true);
  ScratchPadd::Canvas canvas(320, 240);
  settings.draw(canvas, 0);
  settings.press(Key::Confirm, 0);
  settings.draw(canvas, 0);
  EXPECT_TRUE(settings.typing());
}
