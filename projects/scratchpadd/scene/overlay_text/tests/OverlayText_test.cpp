// The overlay's text on desktops (stb_easy_font), without a window
#include <ScratchPadd/Scene/OverlayText.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace ScratchPadd::Scene;

// Text becomes coloured triangles where it was put, scaled, within the
// width and line height the backend gives
TEST(OverlayTextTest, TextBecomesTrianglesWhereItWasPut) {
  std::vector<Overlay::Vertex> triangles;
  appendTextTriangles({100, 50, "Hi", Colour{255, 0, 0}, 2}, triangles);
  ASSERT_FALSE(triangles.empty());
  EXPECT_EQ(triangles.size() % 3, 0u);
  float right = 0, bottom = 0;
  for (const Overlay::Vertex &v : triangles) {
    EXPECT_GE(v.x, 100);
    EXPECT_GE(v.y, 50);
    right = std::max(right, v.x);
    bottom = std::max(bottom, v.y);
    EXPECT_EQ(v.colour, (Colour{255, 0, 0}));
  }
  EXPECT_LE(right, 100 + Overlay::textWidth("Hi", 2));
  EXPECT_LE(bottom, 50 + Overlay::lineHeight(2));
  EXPECT_FLOAT_EQ(Overlay::textWidth("Hi", 2), 2 * Overlay::textWidth("Hi"));
  EXPECT_GT(Overlay::textWidth("Hello"), Overlay::textWidth("Hi"));
  EXPECT_FLOAT_EQ(Overlay::lineHeight(2), 2 * Overlay::lineHeight());
}

// A short text of the busiest letters keeps every stroke: each letter's
// quads (two triangles each) are as many alone as in a longer text. (The
// buffer was once sized by stb_easy_font's average, and "Rock" lost its k's
// upright.)
TEST(OverlayTextTest, BusyLettersKeepEveryStroke) {
  const auto triangleCount = [](const std::string &text) {
    std::vector<Overlay::Vertex> triangles;
    appendTextTriangles({0, 0, text, Colour{}, 1}, triangles);
    return triangles.size() / 3;
  };
  for (const char *letter : {"X", "k", "W", "M", "#", "@"})
    EXPECT_EQ(triangleCount(std::string(4, letter[0])), 4 * triangleCount(letter)) << letter;
  EXPECT_EQ(triangleCount("Rock"), 19u * 2);
}

// The letters as rectangles (what the DS fills into its text layer): on
// whole points at scale 1, placed where the text was put, within its width
// and line height, and the same rectangles the triangles are made of
TEST(OverlayTextTest, LettersAreRectanglesOnWholePoints) {
  const Overlay::Text text{10, 20, "Fuel", Colour{255, 255, 255}, 1};
  const std::vector<TextRectangle> rectangles = textRectangles(text);
  ASSERT_FALSE(rectangles.empty());
  for (const TextRectangle &r : rectangles) {
    EXPECT_EQ(r.x, std::floor(r.x));
    EXPECT_EQ(r.y, std::floor(r.y));
    EXPECT_GT(r.width, 0);
    EXPECT_GT(r.height, 0);
    EXPECT_GE(r.x, 10);
    EXPECT_GE(r.y, 20);
    EXPECT_LE(r.x + r.width, 10 + Overlay::textWidth("Fuel"));
    EXPECT_LE(r.y + r.height, 20 + Overlay::lineHeight());
  }
  std::vector<Overlay::Vertex> triangles;
  appendTextTriangles(text, triangles);
  EXPECT_EQ(triangles.size(), rectangles.size() * 6);
  // A new line starts a line height lower
  const auto two = textRectangles({0, 0, "a\na", Colour{}, 1});
  const auto lowest = std::ranges::max(two, {}, &TextRectangle::y);
  EXPECT_GE(lowest.y, Overlay::lineHeight());
}
