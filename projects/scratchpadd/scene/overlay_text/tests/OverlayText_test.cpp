// The overlay's text on desktops (stb_easy_font), without a window
#include <ScratchPadd/Scene/OverlayText.hpp>
#include <stb/stb_easy_font.h>

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

// A text's ink (its letters' own pixels) sits inside its width and line
// height; centring by it puts the letters' middle where it was asked
// (round 100: centring by the width left numbers off centre)
TEST(OverlayTextTest, CentredByItsInk) {
  const Overlay::Ink ink = Overlay::inkOf("7", 3);
  EXPECT_GT(ink.width, 0);
  EXPECT_GT(ink.height, 0);
  EXPECT_LE(ink.left + ink.width, Overlay::textWidth("7", 3));
  EXPECT_LE(ink.top + ink.height, Overlay::lineHeight(3));
  Overlay overlay;
  overlay.textCentred(100, 50, "7", Colour{255, 255, 255}, 3);
  float left = 1e9f, right = -1e9f, top = 1e9f, bottom = -1e9f;
  for (const TextRectangle &r : textRectangles(overlay.texts().at(0))) {
    left = std::min(left, r.x);
    right = std::max(right, r.x + r.width);
    top = std::min(top, r.y);
    bottom = std::max(bottom, r.y + r.height);
  }
  EXPECT_FLOAT_EQ((left + right) / 2, 100);
  EXPECT_FLOAT_EQ((top + bottom) / 2, 50);
  EXPECT_EQ(Overlay::inkOf("").width, 0);
}

// The whole-number walk (fontRectangles, round 105) makes exactly what
// stb_easy_font_print does, for every character it has, over two lines
TEST(OverlayTextTest, FontRectanglesMatchStbEasyFont) {
  std::string all;
  for (char c = ' '; c <= '~'; ++c)
    all += c;
  all += "\nRock 12 + 7";
  std::vector<ScratchPadd::Scene::FontRectangle> ours;
  ScratchPadd::Scene::fontRectangles(all, ours);
  struct Vertex {
    float x, y, z;
    unsigned char colour[4];
  };
  std::vector<Vertex> quads(all.size() * 11 * 4);
  std::string copy = all;
  const int count = stb_easy_font_print(0, 0, copy.data(), nullptr, quads.data(),
                                        static_cast<int>(quads.size() * sizeof(Vertex)));
  ASSERT_EQ(static_cast<int>(ours.size()), count);
  for (int q = 0; q < count; ++q) {
    const Vertex *corner = &quads[static_cast<std::size_t>(q) * 4];
    const auto &r = ours[static_cast<std::size_t>(q)];
    EXPECT_EQ(r.x, corner[0].x) << q;
    EXPECT_EQ(r.y, corner[0].y) << q;
    EXPECT_EQ(r.width, corner[2].x - corner[0].x) << q;
    EXPECT_EQ(r.height, corner[2].y - corner[0].y) << q;
  }
}
