#include <catch2/catch_test_macros.hpp>

#include <FlexVector.hpp>
#include <iostream>
#include <stdexcept>

using namespace Flex::Math;

TEST_CASE("Test FlexVector", "[FlexVector]") {

  SECTION("FlexVector assignment is Tested") {
    FlexVector vector1({1, 2, 3});
    FlexVector vector2;
    vector2 = vector1;
    REQUIRE(vector1 == vector2);
  }

  SECTION("FlexVector flexing is Tested") {
    FlexVector vector1({4.55555, -1.3});
    Precision quarters{.fracBits = 2};
    REQUIRE_FALSE(vector1 == FlexVector({4.5, -1.25}));
    REQUIRE(vector1.quantized(quarters) == FlexVector({4.5, -1.25}));
  }

  SECTION("FlexVector == is Tested") {
    REQUIRE(FlexVector() == FlexVector());
    REQUIRE(FlexVector({1, 2}) == FlexVector({1, 2}));
  }

  SECTION("FlexVector != is Tested") {
    REQUIRE(FlexVector({1, 2}) != FlexVector({1, 3}));
    REQUIRE(FlexVector({1, 2}) != FlexVector({1, 2, 3}));
  }

  SECTION("FlexVector Multiplication is Tested") {
    REQUIRE(FlexVector({1, 2, 3}) * FlexVector({4, 5, 6}) == FlexVector({4, 10, 18}));
    REQUIRE(FlexVector({1, 2, 3}) * FlexPoint(0.5) == FlexVector({0.5, 1, 1.5}));
  }

  SECTION("FlexVector Addition is Tested") {
    FlexVector vector3 = FlexVector({1, 2}) + FlexVector({0.5, -2});
    REQUIRE(vector3 == FlexVector({1.5, 0}));
  }

  SECTION("FlexVector Subtraction is Tested") {
    FlexVector vector3 = FlexVector({1, 2}) - FlexVector({0.5, -2});
    REQUIRE(vector3 == FlexVector({0.5, 4}));
  }

  SECTION("FlexVector Dot Product is Tested") {
    REQUIRE(FlexVector({1, 2, 3}).dot(FlexVector({4, 5, 6})) == 32);
  }

  SECTION("FlexVector size mismatch throws") {
    REQUIRE_THROWS_AS(FlexVector({1, 2}) + FlexVector({1}), std::invalid_argument);
  }
}
