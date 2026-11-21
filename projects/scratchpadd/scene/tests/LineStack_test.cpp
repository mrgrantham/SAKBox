#include <ScratchPadd/Scene/LineStack.hpp>

#include <gtest/gtest.h>

#include <array>

namespace ScratchPadd::Scene {
namespace {

// A made-up font, its lines 10 points tall at scale 1 (the arithmetic's
// the same in any font)
float tenAScale(float scale) { return 10 * scale; }

TEST(LineStackTest, AStacksHeightIsItsLinesAndTheGapsBetween) {
  EXPECT_FLOAT_EQ(stackHeight(std::array{1.0f, 2.0f, 1.5f}, 4, tenAScale), 10 + 20 + 15 + 2 * 4);
  EXPECT_FLOAT_EQ(stackHeight(std::array{2.0f}, 4, tenAScale), 20);
}

TEST(LineStackTest, StackingDownStartsAtTheTopAndLeavesTheGap) {
  const auto lines = stackDown(5, std::array{1.0f, 2.0f}, 3, tenAScale);
  EXPECT_FLOAT_EQ(lines[0].top, 5);
  EXPECT_FLOAT_EQ(lines[0].bottom, 15);
  EXPECT_FLOAT_EQ(lines[1].top, 18);
  EXPECT_FLOAT_EQ(lines[1].bottom, 38);
  EXPECT_FLOAT_EQ(lines[1].middle(), 28);
  EXPECT_FLOAT_EQ(lines[1].height(), 20);
}

TEST(LineStackTest, StackingUpEndsAtTheBottomInReadingOrder) {
  const auto lines = stackUp(100, std::array{1.5f, 1.0f}, 2, tenAScale);
  EXPECT_FLOAT_EQ(lines[1].bottom, 100);
  EXPECT_FLOAT_EQ(lines[1].top, 90);
  EXPECT_FLOAT_EQ(lines[0].bottom, 88);
  EXPECT_FLOAT_EQ(lines[0].top, 73);
}

TEST(LineStackTest, StackingAroundAMiddleCentresTheWholeStack) {
  const auto lines = stackAround(50, std::array{1.0f, 3.0f}, 4, tenAScale);
  EXPECT_FLOAT_EQ(lines[0].top, 50 - 22);
  EXPECT_FLOAT_EQ(lines[1].bottom, 50 + 22);
}

TEST(LineStackTest, LinesNeverOverlapWhateverTheirScales) {
  const auto lines = stackDown(0, std::array{3.0f, 1.0f, 2.5f, 1.0f}, 0, tenAScale);
  for (std::size_t i = 1; i < lines.size(); ++i)
    EXPECT_GE(lines[i].top, lines[i - 1].bottom) << i;
}

} // namespace
} // namespace ScratchPadd::Scene
