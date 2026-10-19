#include <FlexReal.hpp>
#include <gtest/gtest.h>
#include <type_traits>

using namespace Flex::Math;

static_assert(std::is_trivially_copyable_v<FlexReal>, "FlexReal must stay usable in unions");

TEST(FlexRealTest, FullPrecisionByDefault) {
  EXPECT_DOUBLE_EQ(FlexReal(0.1).value(), 0.1);
  EXPECT_DOUBLE_EQ((FlexReal(1.5) * 2.25).value(), 3.375);
}

TEST(FlexRealTest, SnapsEveryOperation) {
  ScopedActivePrecision coarse(Precision{.fracBits = 2});
  EXPECT_DOUBLE_EQ(FlexReal(0.9).value(), 0.75);
  EXPECT_DOUBLE_EQ(FlexReal(-0.9).value(), -0.75);
  // 0.75 * 0.75 = 0.5625 snaps to 0.5
  EXPECT_DOUBLE_EQ((FlexReal(0.75) * FlexReal(0.75)).value(), 0.5);
  EXPECT_DOUBLE_EQ((FlexReal(0.75) * 0.75).value(), 0.5);
  EXPECT_DOUBLE_EQ(flex_sqrt(FlexReal(2.0)).value(), 1.25);
}

TEST(FlexRealTest, ScopesNestAndRestore) {
  {
    ScopedActivePrecision outer(Precision{.fracBits = 4});
    {
      ScopedActivePrecision inner(Precision{.fracBits = 0});
      EXPECT_DOUBLE_EQ(FlexReal(2.7).value(), 2.0);
    }
    EXPECT_DOUBLE_EQ(FlexReal(2.7).value(), 2.6875);
  }
  EXPECT_DOUBLE_EQ(FlexReal(2.7).value(), 2.7);
}

TEST(FlexRealTest, QuantizeEdgeCases) {
  // Non-finite values pass through
  EXPECT_TRUE(std::isinf(quantize(INFINITY, Precision{.fracBits = 2})));
  // Snapping twice is the same as once
  Precision p{.fracBits = 3, .rounding = Rounding::Nearest};
  for (double v = -5.0; v < 5.0; v += 0.0137) {
    EXPECT_DOUBLE_EQ(quantize(quantize(v, p), p), quantize(v, p));
  }
  // Snapping never reorders values
  double previous = quantize(-5.0, p);
  for (double v = -5.0; v < 5.0; v += 0.0137) {
    EXPECT_LE(previous, quantize(v, p));
    previous = quantize(v, p);
  }
}
