// FixedPoint.hpp: FlexPoint16 (Q16.16) and FlexPoint (Q32.32) arithmetic,
// mixing with other numbers, the maths functions, quantizing a FlexPoint16,
// and the portable 128-bit arithmetic FlexPoint uses where there's no
// __int128 (checked against __int128 here).
#include <FlexPoint.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <random>

using namespace Flex::Math;

TEST(FixedPointTest, FlexPoint16StepAndRange) {
  EXPECT_EQ(FlexPoint16::epsilon().value(), std::ldexp(1.0, -16));
  EXPECT_NEAR(FlexPoint16::max().value(), 32768, 1e-4);
  EXPECT_EQ(FlexPoint16::lowest().value(), -32768);
  EXPECT_EQ(sizeof(FlexPoint16), 4u);
  EXPECT_EQ(sizeof(FlexPoint), 8u);
}

// Values round to the nearest step; * rounds toward negative infinity and /
// toward zero, in the last place
TEST(FixedPointTest, FlexPoint16Arithmetic) {
  EXPECT_EQ(FlexPoint16(1.5) + FlexPoint16(2.25), FlexPoint16(3.75));
  EXPECT_EQ(FlexPoint16(1.5) * FlexPoint16(-2), FlexPoint16(-3));
  EXPECT_EQ(FlexPoint16(7) / FlexPoint16(2), FlexPoint16(3.5));
  const FlexPoint16 step = FlexPoint16::epsilon();
  EXPECT_EQ(step * FlexPoint16(0.5), FlexPoint16());                         // half a step, down
  EXPECT_EQ(-step * FlexPoint16(0.5), -step);                                // toward -infinity
  EXPECT_EQ(FlexPoint16(1) / FlexPoint16(3), FlexPoint16::fromFixed(21845)); // 65536 / 3
  EXPECT_EQ(FlexPoint16(-1) / FlexPoint16(3), FlexPoint16::fromFixed(-21845));
  EXPECT_EQ(FlexPoint16(0.1).valueFixed(), 6554); // 6553.6, to the nearest
}

// Mixed with int, float or double on either side, as a real typedef needs
TEST(FixedPointTest, MixesWithOtherNumbers) {
  const FlexPoint16 x = 2.5;
  EXPECT_EQ(x * 2, FlexPoint16(5));
  EXPECT_EQ(2 * x, FlexPoint16(5));
  EXPECT_EQ(x + 0.5f, FlexPoint16(3));
  EXPECT_EQ(1.0 - x, FlexPoint16(-1.5));
  EXPECT_TRUE(x > 2);
  EXPECT_TRUE(0.5 < x);
  EXPECT_TRUE(x == 2.5);
  const double asDouble = x; // converts implicitly, as FlexReal does
  EXPECT_EQ(asDouble, 2.5);
}

// The square root is exact: the largest step whose square is no more than
// the value
TEST(FixedPointTest, FlexPoint16SquareRootIsExact) {
  std::mt19937 random(7);
  std::uniform_int_distribution<std::int32_t> raw(1, std::numeric_limits<std::int32_t>::max());
  for (int i = 0; i < 10000; i++) {
    const FlexPoint16 x = FlexPoint16::fromFixed(raw(random));
    const std::uint64_t root = flex_sqrt(x).valueFixed();
    const std::uint64_t scaled = static_cast<std::uint64_t>(x.valueFixed()) << 16;
    ASSERT_LE(root * root, scaled) << x.valueFixed();
    ASSERT_GT((root + 1) * (root + 1), scaled) << x.valueFixed();
  }
  EXPECT_EQ(flex_sqrt(FlexPoint16(4)), FlexPoint16(2));
  EXPECT_EQ(flex_sqrt(FlexPoint16()), FlexPoint16());
}

TEST(FixedPointTest, MathsFunctions) {
  EXPECT_EQ(flex_abs(FlexPoint16(-2.5)), FlexPoint16(2.5));
  EXPECT_EQ(flex_fmod(FlexPoint16(7.5), FlexPoint16(2)), FlexPoint16(1.5));
  EXPECT_EQ(flex_fmod(FlexPoint16(-7.5), FlexPoint16(2)), FlexPoint16(-1.5)); // fmod's sign
  // Through double, to the nearest step
  EXPECT_EQ(flex_sin(FlexPoint16(0.5)), FlexPoint16(std::sin(0.5)));
  const FlexPoint16 damping = 0.95; // 0.949997: the nearest step
  EXPECT_EQ(flex_pow(damping, FlexPoint16(0.5)), FlexPoint16(std::pow(damping.value(), 0.5)));
  EXPECT_EQ(flex_atan2(FlexPoint16(1), FlexPoint16(-1)), FlexPoint16(std::atan2(1.0, -1.0)));
  EXPECT_NEAR(flex_sqrt(FlexPoint(2)).value(), std::sqrt(2.0), 1e-9);
}

// quantized works on a FlexPoint16 as on a FlexPoint: snapping the fraction,
// then saturating or wrapping the integer range
TEST(FixedPointTest, FlexPoint16Quantized) {
  const FlexPoint16 x = 5.3;
  EXPECT_EQ(x.quantized(Precision{.fracBits = 1}), FlexPoint16(5)); // toward zero
  EXPECT_EQ(x.quantized(Precision{.fracBits = 1, .rounding = Rounding::Nearest}), FlexPoint16(5.5));
  EXPECT_EQ(x.quantized(Precision{.fracBits = 16, .intBits = 2, .overflow = Overflow::Saturate}),
            FlexPoint16(4) - FlexPoint16::epsilon());
  // 5.25 wraps round from 4 to -4, landing on -2.75
  EXPECT_EQ(x.quantized(Precision{.fracBits = 4, .intBits = 2, .overflow = Overflow::Wrap}),
            FlexPoint16(-2.75));
}

#if defined(FLEXMATH_HAS_INT128)
// The portable versions (for 32-bit targets and MSVC) agree with __int128
// over random values of every size and sign
TEST(FixedPointTest, PortableArithmeticMatchesInt128) {
  std::mt19937_64 random(11);
  for (int i = 0; i < 100000; i++) {
    const int bits = 1 + static_cast<int>(random() % 63);
    const std::int64_t a = static_cast<std::int64_t>(random()) >> (64 - bits);
    const std::int64_t b = static_cast<std::int64_t>(random()) >> (64 - 1 - random() % 63);
    ASSERT_EQ(detail::multiplyShiftedPortable(a, b, 32), detail::multiplyShifted(a, b, 32))
        << a << " x " << b;
    if (b != 0 && bits <= 31) { // (quotients that fit 64 bits)
      ASSERT_EQ(detail::divideShiftedPortable(a, 32, b), detail::divideShifted(a, 32, b))
          << a << " / " << b;
    }
  }
}
#endif
