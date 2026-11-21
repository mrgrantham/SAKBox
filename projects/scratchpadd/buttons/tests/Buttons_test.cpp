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

// Feedback: a press glows a moment (in, then out), and a message stays
// until cleared
TEST(ButtonsTest, APressGlowsAMomentAndSays) {
  Panel panel = aPanel();
  EXPECT_FALSE(panel.showingFeedback());
  panel.press(3, 10.0, "C: not yet");
  EXPECT_EQ(panel.pressed(), 3);
  EXPECT_EQ(panel.message(), "C: not yet");
  EXPECT_FLOAT_EQ(panel.pressGlow(10.0), 0);
  EXPECT_NEAR(panel.pressGlow(10.0 + Panel::kPressSeconds * 0.2), 1, 1e-5);
  EXPECT_GT(panel.pressGlow(10.0 + Panel::kPressSeconds * 0.6), 0);
  EXPECT_LT(panel.pressGlow(10.0 + Panel::kPressSeconds * 0.6), 1);
  EXPECT_FALSE(panel.pressing(10.0 + Panel::kPressSeconds));
  panel.clearFeedback();
  EXPECT_FALSE(panel.showingFeedback());
}

// The glow is a see-through white over the button's inside; a toggle
// that's on keeps its outline
TEST(ButtonsTest, AGlowLightensAndAnOnToggleIsOutlined) {
  Panel panel;
  panel.set({{.id = 1, .area = {10, 10, 20, 20}, .label = ""},
             {.id = 2, .area = {50, 10, 20, 20}, .label = "", .on = true}});
  const Buttons::Style style;
  Canvas canvas(100, 40);
  canvas.clear({0, 0, 0, 255});
  panel.draw(canvas, style);
  const Buttons::Colour face = canvas.pixels()[20 * 100 + 20];
  panel.press(1, 0.0);
  panel.setTime(Panel::kPressSeconds * 0.2);
  panel.draw(canvas, style);
  const Buttons::Colour glowing = canvas.pixels()[20 * 100 + 20];
  EXPECT_GT(glowing.r, face.r);
  EXPECT_GT(glowing.b, face.b);
  EXPECT_EQ(canvas.pixels()[9 * 100 + 60], style.on); // (the on outline, just outside)
}

// Drawn: each button's face, a focused one's outline in the focus colour,
// and the message's band only when there's a message
TEST(ButtonsTest, DrawsFacesFocusAndMessage) {
  Panel panel = aPanel();
  panel.setMessageArea(Area{0, 80, 100, 14});
  Canvas canvas(100, 100);
  Buttons::Style style;
  style.bevel = Buttons::Bevel::Flat; // (an edge all round, in the edge colour)
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

// Raised (the default look): lit along the top and left, shaded along the
// bottom and right, the drop under the face, all inside the button's area
TEST(ButtonsTest, ARaisedButtonIsLitFromTheTopLeft) {
  Panel panel;
  panel.set({{.id = 1, .area = {10, 10, 20, 20}}});
  const Buttons::Style style;
  Canvas canvas(40, 40);
  canvas.clear({0, 0, 0, 255});
  panel.draw(canvas, style);
  const auto brightness = [](Canvas::Colour c) { return c.r + c.g + c.b; };
  const Canvas::Colour top = canvas.at(20, 10), left = canvas.at(10, 20);
  const Canvas::Colour bottom = canvas.at(20, 27), right = canvas.at(29, 20);
  EXPECT_GT(brightness(top), brightness(style.edge)); // (lighter than the edge colour)
  EXPECT_EQ(left, top);
  EXPECT_LT(brightness(bottom), brightness(style.face));
  EXPECT_EQ(right, bottom);
  EXPECT_EQ(canvas.at(20, 20), style.face);
  EXPECT_EQ(canvas.at(20, 28), style.drop); // (two pixels of drop under the face)
  EXPECT_EQ(canvas.at(20, 29), style.drop);
  EXPECT_EQ(canvas.at(20, 30), (Canvas::Colour{0, 0, 0, 255})); // (nothing outside it)
}

// Pressed, it sinks a pixel (the drop's colour above it) and the edges
// swap: shaded along the top, lit along the bottom; once the press is
// over it rises again
TEST(ButtonsTest, APressedRaisedButtonSinks) {
  Panel panel;
  panel.set({{.id = 1, .area = {10, 10, 20, 20}}});
  const Buttons::Style style;
  Canvas canvas(40, 40);
  canvas.clear({0, 0, 0, 255});
  panel.press(1, 0.0);
  panel.setTime(Panel::kPressSeconds * 0.5);
  panel.draw(canvas, style);
  const auto brightness = [](Canvas::Colour c) { return c.r + c.g + c.b; };
  EXPECT_EQ(canvas.at(20, 10), style.drop); // (where its top was)
  const Canvas::Colour top = canvas.at(20, 11), bottom = canvas.at(20, 28);
  EXPECT_LT(brightness(top), brightness(bottom));
  EXPECT_EQ(canvas.at(20, 29), style.drop); // (one pixel of drop left)
  panel.setTime(Panel::kPressSeconds);
  panel.draw(canvas, style);
  EXPECT_GT(brightness(canvas.at(20, 10)), brightness(canvas.at(20, 27)));
}

// The D-pad prefers a button within 45 degrees of the way pressed: right
// from a key on a staggered keyboard is the next key along, not the nearer
// one on the row below
TEST(ButtonsTest, TheDPadPrefersTheWayPressed) {
  Panel panel;
  panel.set({{.id = 1, .area = {0, 0, 20, 20}},     // Q
             {.id = 2, .area = {24, 0, 20, 20}},    // W, along the row
             {.id = 3, .area = {12, 30, 20, 20}}}); // A, below and a little right
  panel.setFocus(1);
  panel.move(Direction::Right);
  EXPECT_EQ(panel.focus(), 2);
  panel.setFocus(1);
  panel.move(Direction::Down);
  EXPECT_EQ(panel.focus(), 3);
}

// A button part shown (fading in or out) is drawn see-through over what's
// under it; one fading in answers a touch at once, one fading out (no
// longer usable) lets it pass
TEST(ButtonsTest, AFadingButtonIsSeeThrough) {
  Panel panel;
  panel.set({{.id = 1, .area = {10, 10, 20, 20}, .usable = false, .shown = 0.5f}});
  EXPECT_FALSE(panel.at(15, 15).has_value());
  Canvas canvas(40, 40);
  const Buttons::Style style;
  canvas.clear({0, 0, 0, 255});
  panel.draw(canvas, style);
  const Canvas::Colour half = canvas.at(15, 25);
  EXPECT_GT(half.b, 0);
  EXPECT_LT(half.b, style.face.b); // (half the face over black)
  panel.set({{.id = 1, .area = {10, 10, 20, 20}, .shown = 0.2f}});
  EXPECT_EQ(panel.at(15, 15), 1); // (fading in)
}
