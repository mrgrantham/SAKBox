// The overlay's text on desktops (stb_easy_font), without a window
#include "../src/Backend.hpp"

#include <gtest/gtest.h>

#include <algorithm>

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
