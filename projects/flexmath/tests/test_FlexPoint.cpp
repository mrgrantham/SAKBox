#include <gtest/gtest.h>
#include <FlexPoint.hpp>
#include <iostream>
#include "fmtlog/fmtlog.h"

using namespace Flex::Math;


TEST(FlexPointTest,TestAssignment) {
    fmt::print("VERSION IS {}\n",FMT_VERSION);
    FMTLOG(fmtlog::INF, "The answer is {}.\n", 42);
    fmtlog::poll();
    // FlexPoint value1(40);
    // FlexPoint value2(20);
    // EXPECT_TRUE(value1 == value2);
}

TEST(FlexPointTest,TestFlex) {
    FlexPoint value1(4.55555);

    FlexPoint value2(4.55);
    
    EXPECT_TRUE(value1 == value2);
}

TEST (FlexPointTest,TestEQ) {
    FlexPoint value1;
    FlexPoint value2;
    EXPECT_TRUE(value1 == value2);
}

TEST (FlexPointTest,TestGTE) {
    FlexPoint value1;
    FlexPoint value2;
    EXPECT_TRUE(value1 >= value2);
}

TEST (FlexPointTest,TestLTE) {
    FlexPoint value1;
    FlexPoint value2;
    EXPECT_TRUE(value1 <= value2);
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

TEST (FlexPointTest,TestPrecision) {
    FlexPoint value1(0xFFFF);
    value1.setFractionalComponent(16);
    EXPECT_TRUE(value1 == 0x000F);
}

TEST (FlexPointTest,TestMultiplication) {
    FlexPoint value1;
    FlexPoint value2;
    FlexPoint value3 = value1 * value2;
    EXPECT_TRUE(true);
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

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
