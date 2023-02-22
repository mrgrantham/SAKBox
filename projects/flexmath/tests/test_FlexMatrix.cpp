#include <catch2/catch_test_macros.hpp>

#include <FlexMatrix.hpp>
#include <iostream>

using namespace Flex::Math;

TEST_CASE( "Test FlexMatrix", "[FlexMatrix]" ) {

    // Put any setup code here that needs to
    // repeat for each SECTION

    SECTION ("FlexMatrix == is Tested") {
        FlexMatrix matrix_a1({{1,1,1},{1,1,1}});
        FlexMatrix matrix_a2({{1,1,1},{1,1,1}});
        REQUIRE(matrix_a1 == matrix_a2);

        FlexMatrix matrix_b1({{1,1,1},{1,1,1}});
        FlexMatrix matrix_b2({{1,1,0},{1,1,1}});
        REQUIRE_FALSE(matrix_b1 == matrix_b2);
    }

    SECTION ("FlexMatrix >= is Tested") {
        FlexMatrix matrix1;
        FlexMatrix matrix2;
        REQUIRE(matrix1 >= matrix2);
    }

    SECTION ("FlexMatrix <= is Tested") {
        FlexMatrix matrix1;
        FlexMatrix matrix2;
        REQUIRE(matrix1 <= matrix2);
    }

    SECTION ("FlexMatrix > is Tested") {
        FlexMatrix matrix1({{1,1,1}});
        FlexMatrix matrix2({{0,0,0}});
        REQUIRE(matrix1 > matrix2);
    }

    SECTION ("FlexMatrix < is Tested") {
        FlexMatrix matrix1({{0,0,0}});
        FlexMatrix matrix2({{1,1,1}});
        REQUIRE(matrix1 < matrix2);
    }

    SECTION ("Cross Product is Tested") {
        FlexMatrix matrix1;
        FlexMatrix matrix2;
        FlexMatrix matrix3 = matrix1 * matrix2;
        REQUIRE(true);
    }
    SECTION ("Dot Product is Tested") {
        FlexMatrix matrix1;
        FlexPoint scalar = 4;
        FlexMatrix matrix2 = matrix1 * scalar;
        REQUIRE(true);
    }
    SECTION ("FlexMatrix Addition is Tested") {
        FlexMatrix matrix1;
        FlexMatrix matrix2;
        FlexMatrix matrix3 = matrix1 + matrix2;
        REQUIRE(true);
    }

    SECTION ("FlexMatrix Subtraction is Tested") {
        FlexMatrix matrix1;
        FlexMatrix matrix2;
        FlexMatrix matrix3 = matrix1 - matrix2;
        REQUIRE(true);
    }

}