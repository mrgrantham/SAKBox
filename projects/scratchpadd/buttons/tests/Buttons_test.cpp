#include <ScratchPadd/Buttons/Buttons.hpp>

#include <gtest/gtest.h>

using namespace ScratchPadd;
using Buttons::Area;
using Buttons::Direction;
using Buttons::Panel;

namespace {
// Three in a row, and one under the middle one
Panel aPanel() {
  Panel panel;
  panel.set({{.id = 1, .area = {10, 10, 20, 20}, .label = "A"},
             {.id = 2, .area = {40, 10, 20, 20}, .label = "B"},
             {.id = 3, .area = {70, 10, 20, 20}, .label = "C", .usable = false},
             {.id = 4, .area = {40, 40, 20, 20}, .label = "D"}});
  return panel;
}
} // namespace

// A touch finds the button under it, usable or not; nothing between them
TEST(ButtonsTest, ATouchFindsItsButton) {
  const Panel panel = aPanel();
  EXPECT_EQ(panel.at(15, 15), 1);
  EXPECT_EQ(panel.at(75, 15), 3);
  EXPECT_FALSE(panel.at(35, 15).has_value());
  EXPECT_FALSE(panel.at(30, 15).has_value()); // (the right edge is outside)
}

// The D-pad: first the first usable button, then the nearest usable one
// each way, skipping the unusable, staying put at an edge
TEST(ButtonsTest, TheDpadMovesToTheNearestUsableButton) {
  Panel panel = aPanel();
  EXPECT_FALSE(panel.focus().has_value());
  panel.move(Direction::Right);
  EXPECT_EQ(panel.focus(), 1);
  panel.move(Direction::Right);
  EXPECT_EQ(panel.focus(), 2);
  panel.move(Direction::Right); // (C can't be used: nothing usable that way)
  EXPECT_EQ(panel.focus(), 2);
  panel.move(Direction::Down);
  EXPECT_EQ(panel.focus(), 4);
  panel.move(Direction::Up);
  EXPECT_EQ(panel.focus(), 2);
  panel.move(Direction::Left);
  EXPECT_EQ(panel.focus(), 1);
  panel.move(Direction::Left);
  EXPECT_EQ(panel.focus(), 1);
  // New buttons keep the focus while its id is there, and drop it when not
  panel.set({{.id = 1, .area = {0, 0, 5, 5}}});
  EXPECT_EQ(panel.focus(), 1);
  panel.set({{.id = 9, .area = {0, 0, 5, 5}}});
  EXPECT_FALSE(panel.focus().has_value());
}

// Feedback: a flash and a message, until cleared
TEST(ButtonsTest, AUsedButtonFlashesAndSays) {
  Panel panel = aPanel();
  EXPECT_FALSE(panel.showingFeedback());
  panel.flash(3, "C: not yet");
  EXPECT_EQ(panel.flashed(), 3);
  EXPECT_EQ(panel.message(), "C: not yet");
  panel.clearFeedback();
  EXPECT_FALSE(panel.showingFeedback());
}

// Drawn: each button's face, a focused one's outline in the focus colour,
// and the message's band only when there's a message
TEST(ButtonsTest, DrawsFacesFocusAndMessage) {
  Panel panel = aPanel();
  panel.setMessageArea(Area{0, 80, 100, 14});
  Canvas canvas(100, 100);
  const Buttons::Style style;
  canvas.clear({0, 0, 0, 255});
  panel.draw(canvas, style);
  EXPECT_EQ(canvas.at(12, 25), style.face);
  EXPECT_EQ(canvas.at(10, 10), style.edge);
  EXPECT_EQ(canvas.at(50, 90), (Canvas::Colour{0, 0, 0, 255})); // (no message: no band)
  panel.move(Direction::Right);
  panel.say("hello");
  panel.draw(canvas, style);
  EXPECT_EQ(canvas.at(10, 10), style.focus);
  EXPECT_EQ(canvas.at(2, 92), style.band);
  // Everything it draws on is in its areas
  const auto areas = panel.areas();
  EXPECT_EQ(areas.size(), 5u);
}
