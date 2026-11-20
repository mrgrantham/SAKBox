// The settings: Change opens the keyboard, Done gives the new name, Back
// leaves; an empty or unchanged name is kept
#include <CosmicHex/Handheld/SettingsScreen.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex::Handheld;
using Key = SettingsScreen::Key;
using Outcome = SettingsScreen::Outcome;

TEST(SettingsScreenTest, ChangeYourName) {
  SettingsScreen settings(256, 192);
  settings.open("James", true, Animations::Full);
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
  settings.open("Nova", false, Animations::Full);
  settings.touch(60, 98, 0); // Change
  ASSERT_TRUE(settings.typing());
  for (int i = 0; i < 4; ++i)
    settings.press(Key::Back, 0);
  EXPECT_EQ(settings.press(Key::Done, 0), Outcome::None); // (nothing typed: kept)
  EXPECT_EQ(settings.name(), "Nova");
  EXPECT_EQ(settings.touch(60, 174, 0), Outcome::Back);
  EXPECT_EQ(settings.press(Key::Back, 0), Outcome::Back);
}

TEST(SettingsScreenTest, ItDraws) {
  SettingsScreen settings(320, 240);
  settings.open("James", true, Animations::Full);
  ScratchPadd::Canvas canvas(320, 240);
  settings.draw(canvas, 0);
  settings.press(Key::Confirm, 0);
  settings.draw(canvas, 0);
  EXPECT_TRUE(settings.typing());
}

// Animations: Full, Quick, Off and round again, by touch or A; its words
// on the card read back
TEST(SettingsScreenTest, AnimationsGoRound) {
  SettingsScreen settings(256, 192);
  settings.open("James", true, Animations::Full);
  EXPECT_EQ(settings.touch(60, 140, 0), Outcome::AnimationsChanged);
  EXPECT_EQ(settings.animations(), Animations::Quick);
  EXPECT_EQ(settings.press(Key::Confirm, 0), Outcome::AnimationsChanged); // (still focused)
  EXPECT_EQ(settings.animations(), Animations::Off);
  settings.press(Key::Confirm, 0);
  EXPECT_EQ(settings.animations(), Animations::Full);
  for (const Animations a : {Animations::Full, Animations::Quick, Animations::Off})
    EXPECT_EQ(animationsFrom(wordOf(a)), a);
  EXPECT_FALSE(animationsFrom("Sometimes").has_value());
  EXPECT_DOUBLE_EQ(rateOf(Animations::Quick), 2);
}
