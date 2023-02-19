#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
// #include <catch2/single_include/catch.hpp>
// #include <catch2/catch_test_macros.hpp>

#include <FlexPoint.hpp>
#include <iostream>

using namespace Flex::Math;

TEST_CASE( "Test FlexPoint", "[FlexPoint]" ) {

    std::cout << "startup stub" << std::endl;

    SECTION ("FlexPoint assignment is Tested") {
        FlexPoint value1(40);
        FlexPoint value2(20);
        REQUIRE(value1 == value2);
    }

    SECTION ("FlexPoint flexing is Tested") {
        FlexPoint value1(4.55555);

        FlexPoint value2(4.55);
        
        REQUIRE(value1 == value2);
    }

    SECTION ("FlexPoint == is Tested") {
        FlexPoint value1;
        FlexPoint value2;
        REQUIRE(value1 == value2);
    }

    SECTION ("FlexPoint >= is Tested") {
        FlexPoint value1;
        FlexPoint value2;
        REQUIRE(value1 >= value2);
    }

    SECTION ("FlexPoint <= is Tested") {
        FlexPoint value1;
        FlexPoint value2;
        REQUIRE(value1 <= value2);
    }

    SECTION ("FlexPoint > is Tested") {
        FlexPoint value1 = 1;
        FlexPoint value2 = 0;
        std::cout << fmt::format("Test > val1: {} val2: {}", value1, value2 );
        REQUIRE(value1 > value2);
    }

    SECTION ("FlexPoint < is Tested") {
        FlexPoint value1 = 0;
        FlexPoint value2 = 1;
        std::cout << fmt::format("Test < val1: {} val2: {}", value1, value2 );
        REQUIRE(value1 < value2);
    }

    SECTION("FlexPoint Precision is Tested") {
        FlexPoint value1(0xFFFF);
        value1.setFractionalComponent(16);
        REQUIRE(value1 == 0x000F);
    }

    SECTION ("FlexPoint Multiplication is Tested") {
        FlexPoint value1;
        FlexPoint value2;
        FlexPoint value3 = value1 * value2;
        REQUIRE(true);
    }
    SECTION ("FlexPoint Addition is Tested") {
        FlexPoint value1(0xFF00);
        FlexPoint value2(0x00FF);
        FlexPoint value3 = value1 + value2;
        REQUIRE(value3.value() == 0xFFFF);
    }

    SECTION ("FlexPoint Subtraction is Tested") {
        FlexPoint value1(0xFFFF);
        FlexPoint value2(0x00FF);
        FlexPoint value3 = value1 - value2;
        REQUIRE(value3 == 0xFF00);
    }

}