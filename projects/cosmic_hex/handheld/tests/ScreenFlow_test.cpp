// The console's screens: the flow drawn in ScreenFlow.hpp, each
// event's step (start, stop, hold or quit), and events that mean nothing on
// a screen ignored; and the button menus the title and pause screens use
#include <CosmicHex/Handheld/ButtonMenu.hpp>
#include <CosmicHex/Handheld/ScreenFlow.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex::Handheld;
using Event = ScreenFlow::Event;
using Screen = ScreenFlow::Screen;

TEST(ScreenFlowTest, TitleSetupGameWonTitle) {
  ScreenFlow flow;
  EXPECT_EQ(flow.screen(), Screen::Title);
  EXPECT_FALSE(flow.on(Event::Start).changed); // (nothing to start yet)
  EXPECT_TRUE(flow.on(Event::Play).changed);
  EXPECT_EQ(flow.screen(), Screen::Setup);
  const auto start = flow.on(Event::Start);
  EXPECT_TRUE(start.startGame);
  EXPECT_EQ(flow.screen(), Screen::Game);
  flow.on(Event::GameWon);
  EXPECT_EQ(flow.screen(), Screen::Won);
  const auto back = flow.on(Event::Continue);
  EXPECT_TRUE(back.stopGame);
  EXPECT_EQ(flow.screen(), Screen::Title);
}

TEST(ScreenFlowTest, SetupGoesBack) {
  ScreenFlow flow;
  flow.on(Event::Play);
  const auto back = flow.on(Event::Back);
  EXPECT_FALSE(back.startGame);
  EXPECT_EQ(flow.screen(), Screen::Title);
}

TEST(ScreenFlowTest, ThePauseMenu) {
  ScreenFlow flow;
  flow.on(Event::Play);
  flow.on(Event::Start);
  const auto pause = flow.on(Event::Pause);
  EXPECT_TRUE(pause.holdChanged);
  EXPECT_TRUE(pause.holdGame);
  EXPECT_EQ(flow.screen(), Screen::Paused);
  const auto resume = flow.on(Event::Resume);
  EXPECT_TRUE(resume.holdChanged);
  EXPECT_FALSE(resume.holdGame);
  EXPECT_EQ(flow.screen(), Screen::Game);
  flow.on(Event::Pause);
  const auto restart = flow.on(Event::Restart);
  EXPECT_TRUE(restart.stopGame);
  EXPECT_TRUE(restart.startGame);
  EXPECT_EQ(flow.screen(), Screen::Game);
  flow.on(Event::Pause);
  const auto title = flow.on(Event::ToTitle);
  EXPECT_TRUE(title.stopGame);
  EXPECT_EQ(flow.screen(), Screen::Title);
  EXPECT_TRUE(flow.on(Event::Quit).quit); // (from the title)
}

TEST(ScreenFlowTest, QuittingFromThePauseMenu) {
  ScreenFlow flow;
  flow.on(Event::Play);
  flow.on(Event::Start);
  EXPECT_FALSE(flow.on(Event::Quit).quit); // (not from the game itself)
  flow.on(Event::Pause);
  EXPECT_TRUE(flow.on(Event::Quit).quit);
}

TEST(ButtonMenuTest, ChoosingByButtonsAndTouch) {
  ButtonMenu menu({"Resume", "Restart", "Title", "Quit"});
  EXPECT_EQ(menu.press(ButtonMenu::Key::Confirm), 0u);
  menu.press(ButtonMenu::Key::Up); // (round to the bottom)
  EXPECT_EQ(menu.focus(), 3u);
  menu.press(ButtonMenu::Key::Down);
  EXPECT_EQ(menu.focus(), 0u);
  for (const auto &[w, h] : {std::pair{320, 240}, std::pair{256, 192}}) {
    const auto buttons = menu.buttons(w, h);
    ASSERT_EQ(buttons.size(), 4u);
    for (std::size_t i = 0; i < buttons.size(); ++i) {
      const auto &b = buttons[i];
      EXPECT_GE(b.y, 0);
      EXPECT_LE(b.y + b.height, static_cast<float>(h));
      if (i > 0) {
        EXPECT_GE(b.y, buttons[i - 1].y + buttons[i - 1].height); // (no overlap)
      }
      const auto chosen = menu.touch(static_cast<int>(b.x + b.width / 2),
                                     static_cast<int>(b.y + b.height / 2), w, h);
      EXPECT_EQ(chosen, i);
    }
    EXPECT_FALSE(menu.touch(0, 0, w, h).has_value());
  }
}

// The settings: from the title, and back to it
TEST(ScreenFlowTest, TheSettingsFromTheTitleAndBack) {
  ScreenFlow flow;
  EXPECT_TRUE(flow.on(ScreenFlow::Event::Settings).changed);
  EXPECT_EQ(flow.screen(), ScreenFlow::Screen::Settings);
  EXPECT_FALSE(flow.on(ScreenFlow::Event::Start).changed); // (nothing to start here)
  EXPECT_TRUE(flow.on(ScreenFlow::Event::Back).changed);
  EXPECT_EQ(flow.screen(), ScreenFlow::Screen::Title);
}
