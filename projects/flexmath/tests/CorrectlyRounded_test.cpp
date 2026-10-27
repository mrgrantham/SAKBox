// CorrectlyRounded.hpp: CORE-MATH's functions give the correctly rounded
// result, the same on every platform. Each input is one of CORE-MATH's hard
// cases (its *.wc files), where the exact result lies very close to halfway
// between two doubles; the expected result is the double nearest the exact
// one, worked out with 200-bit arithmetic (mpmath). Apple's libm gives the
// double next to it for every one of these.
#include <gtest/gtest.h>

#include <CorrectlyRounded.hpp>

#include <cstdlib>

namespace CR = Flex::Math::CorrectlyRounded;

namespace {
double hex(const char *text) { return std::strtod(text, nullptr); }
} // namespace

TEST(CorrectlyRoundedTest, HardCasesRoundCorrectly) {
  EXPECT_EQ(CR::sin(hex("0x1.005023d32fee5p+1")), hex("0x1.d109ad145c88fp-1"));
  EXPECT_EQ(CR::cos(hex("0x1.00147eec5cfa5p+0")), hex("0x1.148001d3ec044p-1"));
  EXPECT_EQ(CR::exp(hex("0x1.00091a4a0dae5p+2")), hex("0x1.b50726fd1ccb3p+5"));
  EXPECT_EQ(CR::log(hex("0x1.bbfff9457d5c3p+0")), hex("0x1.19ee63842b127p-1"));
  EXPECT_EQ(CR::pow(hex("0x1.c4269c893fd34p+50"), hex("0x1.4p-2")), hex("0x1.d79ca618b9632p+15"));
  EXPECT_EQ(CR::atan2(hex("0x1.c06af99d1f8aep+59"), hex("0x1.e62db8902179bp+52")),
            hex("0x1.8ff49b402d68fp+0"));
}

// And the easy cases, like any maths library
TEST(CorrectlyRoundedTest, EasyCases) {
  EXPECT_EQ(CR::sin(0), 0);
  EXPECT_EQ(CR::cos(0), 1);
  EXPECT_EQ(CR::exp(0), 1);
  EXPECT_EQ(CR::log(1), 0);
  EXPECT_EQ(CR::pow(2, 10), 1024);
  EXPECT_EQ(CR::asin(1), CR::acos(0));
  EXPECT_EQ(CR::atan2(1, 1) * 4, CR::acos(-1));
}
