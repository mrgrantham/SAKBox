// An explorer badge as the mockup has it: a star inside a hexagon's
// outline, both yellow (it was a plain filled hexagon), the star clear of
// the outline at the HUD's size on a DS and a 3DS and the panels' size
#include <CosmicHex/Handheld/Hud.hpp>

#include <ScratchPadd/Scene/Overlay.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;

namespace {
constexpr float kX = 40, kY = 30;
float fromMiddle(float x, float y) { return std::hypot(x - kX, y - kY); }
} // namespace

TEST(HudTest, ABadgesStarIsInsideItsHexagon) {
  for (const float radius : {kBadgeRadius, 5.5f, kBadgeRadius * 400 / 256}) {
    // (the hexagon's inner circle, less its 1-point outline)
    const float inside = radius * std::cos(3.14159265f / 6) - 1;
    float furthest = 0;
    for (const ScreenPoint &corner : badgeStar(kX, kY, radius))
      furthest = std::max(furthest, fromMiddle(corner.x, corner.y));
    EXPECT_LT(furthest, inside) << "radius " << radius;
    // (and big enough to see: its points over half the hexagon's size)
    EXPECT_GT(furthest, 0.5f * radius) << "radius " << radius;
  }
}

TEST(HudTest, AnEarnedBadgeIsAStarInAHexagonBothYellow) {
  Scene::Overlay overlay;
  drawBadge(overlay, kX, kY, kBadgeRadius, true);
  int nearTheMiddle = 0;
  for (const Scene::Overlay::Vertex &v : overlay.triangles()) {
    EXPECT_EQ(v.colour, Palette::kHighlight);
    EXPECT_LE(fromMiddle(v.x, v.y), kBadgeRadius + 1); // (nothing outside the hexagon)
    if (fromMiddle(v.x, v.y) < 1)
      ++nearTheMiddle; // (the star's triangles meet in the middle)
  }
  EXPECT_GE(nearTheMiddle, 10);
}

TEST(HudTest, AnEmptySlotIsADimOutline) {
  Scene::Overlay overlay;
  drawBadge(overlay, kX, kY, kBadgeRadius, false);
  ASSERT_FALSE(overlay.triangles().empty());
  for (const Scene::Overlay::Vertex &v : overlay.triangles()) {
    EXPECT_EQ(v.colour, kEmptyBadge);
    EXPECT_GT(fromMiddle(v.x, v.y), kBadgeRadius - 2); // (an outline: nothing in the middle)
  }
}
