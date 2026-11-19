// divide32 and divide64 give C++'s / and % (rounding towards zero), for
// every sign and at the ends of each range. (This checks the portable
// version; the DS's, on its hardware divider, is checked in melonDS by the
// recipe drawings hashing the same.)
#include <IntegerDivide.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

using Flex::Math::divide32;
using Flex::Math::divide64;

TEST(IntegerDivideTest, ThirtyTwoBitsAsCpp) {
  const std::vector<std::int32_t> values{0,
                                         1,
                                         -1,
                                         7,
                                         -7,
                                         255,
                                         -255,
                                         65536,
                                         -65536,
                                         1000003,
                                         -999983,
                                         std::numeric_limits<std::int32_t>::max(),
                                         std::numeric_limits<std::int32_t>::min() + 1};
  for (const std::int32_t n : values)
    for (const std::int32_t d : values) {
      if (d == 0)
        continue;
      const auto [quotient, remainder] = divide32(n, d);
      EXPECT_EQ(quotient, n / d) << n << " / " << d;
      EXPECT_EQ(remainder, n % d) << n << " % " << d;
    }
}

TEST(IntegerDivideTest, SixtyFourBitsAsCpp) {
  const std::vector<std::int64_t> values{0,
                                         1,
                                         -1,
                                         3,
                                         -3,
                                         std::int64_t{1} << 40,
                                         -(std::int64_t{1} << 40),
                                         123456789012345,
                                         -98765432109,
                                         std::numeric_limits<std::int64_t>::max(),
                                         std::numeric_limits<std::int64_t>::min() + 1};
  for (const std::int64_t n : values)
    for (const std::int64_t d : values) {
      if (d == 0)
        continue;
      const auto [quotient, remainder] = divide64(n, d);
      EXPECT_EQ(quotient, n / d) << n << " / " << d;
      EXPECT_EQ(remainder, n % d) << n << " % " << d;
    }
}
