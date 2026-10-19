#include <catch2/catch_test_macros.hpp>

#include <FlexMatrix.hpp>
#include <iostream>
#include <stdexcept>

using namespace Flex::Math;

TEST_CASE( "Test FlexMatrix", "[FlexMatrix]" ) {

    SECTION ("FlexMatrix == is Tested") {
        FlexMatrix matrix_a1({{1,1,1},{1,1,1}});
        FlexMatrix matrix_a2({{1,1,1},{1,1,1}});
        REQUIRE(matrix_a1 == matrix_a2);

        FlexMatrix matrix_b1({{1,1,1},{1,1,1}});
        FlexMatrix matrix_b2({{1,1,0},{1,1,1}});
        REQUIRE_FALSE(matrix_b1 == matrix_b2);
        REQUIRE(matrix_b1 != matrix_b2);
    }

    SECTION ("FlexMatrix Product is Tested") {
        FlexMatrix matrix1({{1,2},{3,4}});
        FlexMatrix matrix2({{5,6},{7,8}});
        REQUIRE(matrix1 * matrix2 == FlexMatrix({{19,22},{43,50}}));

        FlexMatrix row({{1,2,3}});
        FlexMatrix column({{4},{5},{6}});
        REQUIRE(row * column == FlexMatrix({{32}}));
        REQUIRE((column * row).rows() == 3);
        REQUIRE((column * row).columns() == 3);
    }

    SECTION ("FlexMatrix Scaling is Tested") {
        FlexMatrix matrix1({{1,2},{3,4}});
        FlexPoint scalar = 4;
        REQUIRE(matrix1 * scalar == FlexMatrix({{4,8},{12,16}}));
    }

    SECTION ("FlexMatrix Addition is Tested") {
        FlexMatrix matrix3 = FlexMatrix({{1,2},{3,4}}) + FlexMatrix({{1,1},{1,1}});
        REQUIRE(matrix3 == FlexMatrix({{2,3},{4,5}}));
    }

    SECTION ("FlexMatrix Subtraction is Tested") {
        FlexMatrix matrix3 = FlexMatrix({{1,2},{3,4}}) - FlexMatrix({{1,1},{1,1}});
        REQUIRE(matrix3 == FlexMatrix({{0,1},{2,3}}));
    }

    SECTION ("FlexMatrix flexing is Tested") {
        FlexMatrix matrix1({{0.3, 1.9}});
        REQUIRE(matrix1.quantized(Precision{.fracBits = 0}) == FlexMatrix({{0, 1}}));
    }

    SECTION ("FlexMatrix dimension mismatch throws") {
        REQUIRE_THROWS_AS(FlexMatrix({{1,2},{3}}), std::invalid_argument);
        REQUIRE_THROWS_AS(FlexMatrix({{1,2}}) * FlexMatrix({{1,2}}), std::invalid_argument);
    }
}
