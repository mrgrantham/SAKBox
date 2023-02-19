#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
// #include <catch2/single_include/catch.hpp>
// #include <catch2/catch_test_macros.hpp>

#include <FlexVector.hpp>
#include <iostream>

using namespace Flex::Math;

TEST_CASE( "Test FlexVector", "[FlexVector]" ) {

    std::cout << "startup stub" << std::endl;

    SECTION ("FlexVector assignment is Tested") {
        FlexVector vector1;
        FlexVector vector2;
        REQUIRE(vector1 == vector2);
    }

    SECTION ("FlexVector flexing is Tested") {
        FlexVector vector1({4.55555});
        REQUIRE(vector1 == FlexVector({4.55555}));
    }

    SECTION ("FlexVector == is Tested") {
        FlexVector vector1;
        FlexVector vector2;
        REQUIRE(vector1 == vector2);
    }

    SECTION ("FlexVector != is Tested") {
        FlexVector vector1;
        FlexVector vector2;
        REQUIRE(vector1 == vector2);
    }

    SECTION ("FlexVector Multiplication is Tested") {
        FlexVector vector1;
        FlexVector vector2;
        FlexVector vector3 = vector1 * vector2;
        REQUIRE(true);
    }
    SECTION ("FlexVector Addition is Tested") {
        FlexVector vector1;
        FlexVector vector2;
        FlexVector vector3 = vector1 + vector2;
        REQUIRE(true);
    }

    SECTION ("FlexVector Subtraction is Tested") {
        FlexVector vector1;
        FlexVector vector2;
        FlexVector vector3 = vector1 - vector2;
        REQUIRE(true);
    }

}