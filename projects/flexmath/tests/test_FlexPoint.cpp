#include <gtest/gtest.h>
#include <FlexPoint.hpp>
#include <iostream>
#include <random>
#include "fmtlog/fmtlog.h"

using namespace Flex::Math;


TEST(FlexPointTest,TestAssignment) {
    fmt::print("VERSION IS {}\n",FMT_VERSION);
    FMTLOG(fmtlog::INF, "The answer is {}.\n", 42);
    fmtlog::poll();
    FlexPoint value1(40);
    FlexPoint value2 = value1;
    EXPECT_TRUE(value1 == value2);
    EXPECT_EQ(fmt::format("{}", value2), "40.000000");
    EXPECT_EQ(fmt::format("{:x}", FlexPoint(1)), "0x100000000");
}

// Values keep full precision; reduced precision is applied when they are read
TEST(FlexPointTest,TestFlex) {
    FlexPoint value1(4.55555);
    FlexPoint value2(4.55);
    EXPECT_FALSE(value1 == value2);

    Precision quarters{.fracBits = 2};
    EXPECT_TRUE(value1.quantized(quarters) == value2.quantized(quarters));
    EXPECT_DOUBLE_EQ(value1.quantized(quarters).value(), 4.5);
}

TEST (FlexPointTest,TestEQ) {
    FlexPoint value1;
    FlexPoint value2;
    EXPECT_TRUE(value1 == value2);
    EXPECT_TRUE(FlexPoint(2.5) == FlexPoint(2.5));
}

TEST (FlexPointTest,TestGTE) {
    FlexPoint value1;
    FlexPoint value2;
    EXPECT_TRUE(value1 >= value2);
    EXPECT_TRUE(FlexPoint(3) >= FlexPoint(2.5));
}

TEST (FlexPointTest,TestLTE) {
    FlexPoint value1;
    FlexPoint value2;
    EXPECT_TRUE(value1 <= value2);
    EXPECT_TRUE(FlexPoint(-3) <= FlexPoint(2.5));
}

TEST (FlexPointTest,TestGT) {
    FlexPoint value1 = 1;
    FlexPoint value2 = 0;
    std::cout << fmt::format("Test > val1: {} val2: {}", value1, value2 );
    EXPECT_TRUE(value1 > value2);
}

TEST (FlexPointTest,TestLT) {
    FlexPoint value1 = 0;
    FlexPoint value2 = 1;
    std::cout << fmt::format("Test < val1: {} val2: {}", value1, value2 );
    EXPECT_TRUE(value1 < value2);
}

// Integer bits limit range: past them values saturate or wrap like a register
TEST (FlexPointTest,TestPrecision) {
    Precision nibble{.intBits = 4, .overflow = Overflow::Wrap};
    EXPECT_TRUE(FlexPoint(15).quantized(nibble) == 15);
    EXPECT_TRUE(FlexPoint(16).quantized(nibble) == -16);
    EXPECT_TRUE(FlexPoint(0xFFFF).quantized(nibble) == -1);

    Precision clamped{.fracBits = 4, .intBits = 4, .overflow = Overflow::Saturate};
    EXPECT_TRUE(FlexPoint(100).quantized(clamped) == FlexPoint(16 - 1.0 / 16));
    EXPECT_TRUE(FlexPoint(-100).quantized(clamped) == -16);
}

TEST (FlexPointTest,TestRounding) {
    Precision truncate{.fracBits = 1};
    Precision nearest{.fracBits = 1, .rounding = Rounding::Nearest};
    EXPECT_DOUBLE_EQ(FlexPoint(1.75).quantized(truncate).value(), 1.5);
    EXPECT_DOUBLE_EQ(FlexPoint(-1.75).quantized(truncate).value(), -1.5);
    EXPECT_DOUBLE_EQ(FlexPoint(1.8).quantized(nearest).value(), 2.0);
    EXPECT_DOUBLE_EQ(FlexPoint(-1.8).quantized(nearest).value(), -2.0);
    // Negative fractional bits are coarser than 1
    EXPECT_DOUBLE_EQ(FlexPoint(3.9).quantized(Precision{.fracBits = -1}).value(), 2.0);
}

TEST (FlexPointTest,TestMultiplication) {
    FlexPoint value1(1.5);
    FlexPoint value2(2.25);
    FlexPoint value3 = value1 * value2;
    EXPECT_DOUBLE_EQ(value3.value(), 3.375);
    EXPECT_DOUBLE_EQ((FlexPoint(-1.5) * FlexPoint(2)).value(), -3.0);
}

TEST (FlexPointTest,TestDivision) {
    EXPECT_DOUBLE_EQ((FlexPoint(3.375) / FlexPoint(1.5)).value(), 2.25);
    EXPECT_DOUBLE_EQ((FlexPoint(-7) / FlexPoint(2)).value(), -3.5);
}

TEST (FlexPointTest,TestAddition) {
    FlexPoint value1(0xFF00);
    FlexPoint value2(0x00FF);
    FlexPoint value3 = value1 + value2;
    EXPECT_TRUE(value3.value() == 0xFFFF);
}

TEST (FlexPointTest,TestSubtraction) {
    FlexPoint value1(0xFFFF);
    FlexPoint value2(0x00FF);
    FlexPoint value3 = value1 - value2;
    EXPECT_TRUE(value3 == 0xFF00);
}

// FlexPoint::quantized (integer math) and quantize() (double math) must agree
TEST (FlexPointTest,TestQuantizeMatchesDouble) {
    std::mt19937 rng(1);
    std::uniform_real_distribution<double> values(-1000.0, 1000.0);
    for (int bits : {-2, 0, 1, 5, 12, 24}) {
        for (Rounding rounding : {Rounding::TowardZero, Rounding::Nearest}) {
            for (Overflow overflow : {Overflow::None, Overflow::Saturate, Overflow::Wrap}) {
                Precision precision{bits, 6, rounding, overflow};
                for (int i = 0; i < 200; i++) {
                    FlexPoint point(values(rng));
                    EXPECT_DOUBLE_EQ(point.quantized(precision).value(),
                                     quantize(point.value(), precision))
                        << "value " << point.value() << " bits " << bits;
                }
            }
        }
    }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
