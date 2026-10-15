// The code examples from README.md and the header comments, with the results
// they document. If an example's behaviour changes, update the docs too.
#include <catch2/catch_test_macros.hpp>

#include <FlexController.hpp>
#include <FlexMatrix.hpp>
#include <fmt/format.h>
#include <cmath>
#include <stdexcept>

using namespace Flex::Math;

TEST_CASE( "README quick start", "[examples]" ) {
    FlexController precision;
    FlexGroup &flight = precision.addGroup("Flight", Precision{.fracBits = 1});
    precision.addGroup("Render", Precision{.fracBits = 9});

    FlexPoint x = 4.3;
    REQUIRE(flight.view(x) == FlexPoint(4.0));

    FlexOverride focus = flight.push(Precision{.fracBits = 5});
    REQUIRE(flight.view(x) == FlexPoint(4.28125));
    focus.release();
    REQUIRE(flight.view(x) == FlexPoint(4.0));
}

TEST_CASE( "README quantize examples", "[examples]" ) {
    REQUIRE(quantize(4.3, Precision{.fracBits = 1}) == 4.0);
    REQUIRE(quantize(-1.75, Precision{.fracBits = 1}) == -1.5);
    REQUIRE(quantize(1.8, Precision{.fracBits = 1, .rounding = Rounding::Nearest}) == 2.0);
    REQUIRE(quantize(3.9, Precision{.fracBits = -1}) == 2.0);
    REQUIRE(quantize(16.0, Precision{.intBits = 4, .overflow = Overflow::Wrap}) == -16.0);
    REQUIRE(quantize(100.0, Precision{.fracBits = 4, .intBits = 4, .overflow = Overflow::Saturate}) ==
            15.9375);
}

TEST_CASE( "README stochastic rounding example", "[examples]" ) {
    auto accumulate = [](Rounding rounding) {
        Precision sixteenths{.fracBits = 4, .rounding = rounding};
        double total = 0;
        for (int i = 0; i < 10000; ++i)
            total = quantize(total + 0.001, sixteenths);
        return total;
    };
    seedStochasticRounding(1);
    REQUIRE(std::abs(accumulate(Rounding::Stochastic) - 10.0) < 1.0);
    REQUIRE(accumulate(Rounding::TowardZero) == 0.0);
    REQUIRE(accumulate(Rounding::Nearest) == 0.0);

    // 1.25 at 0 bits is 2 a quarter of the time and 1 otherwise
    int ups = 0;
    for (int i = 0; i < 100000; ++i)
        ups += quantize(1.25, Precision{.fracBits = 0, .rounding = Rounding::Stochastic}) == 2.0;
    REQUIRE(std::abs(ups / 100000.0 - 0.25) < 0.01);
}

TEST_CASE( "README policy pattern", "[examples]" ) {
    FlexPoint x = 1.3, velocity = 1, dt = 0.5;

    FlexGroup view("Flight", Precision{.fracBits = 1}, Policy::View);
    REQUIRE(view.view(x) == FlexPoint(1.0));
    REQUIRE(view.commit(x + velocity * dt) == FlexPoint(1.8));

    FlexGroup lossy("Flight", Precision{.fracBits = 1}, Policy::Lossy);
    REQUIRE(lossy.commit(x + velocity * dt) == FlexPoint(1.5));
}

TEST_CASE( "README controller example", "[examples]" ) {
    FlexController precision;
    precision.addGroup("Hull", Precision{.fracBits = 2}, Policy::View);
    FlexOverride phase = precision.group("Hull").push(Precision{.fracBits = -1});
    REQUIRE(precision.group("Hull").precision().fracBits == -1);
    REQUIRE_THROWS_AS(precision.addGroup("Hull"), std::invalid_argument);
    REQUIRE_THROWS_AS(precision.group("Render"), std::out_of_range);
}

TEST_CASE( "README FlexPoint example", "[examples]" ) {
    FlexPoint a = 1.5, b = 2;
    FlexPoint c = a * b + 0.25;
    REQUIRE(c == FlexPoint(3.25));
    REQUIRE(c.quantized(Precision{.fracBits = 1}) == FlexPoint(3.0));
    REQUIRE(fmt::format("{} {:x}", c, c) == "3.250000 0x340000000");
    REQUIRE(FlexPoint::fromFixed(c.valueFixed()) == c);
}

TEST_CASE( "README FlexReal example", "[examples]" ) {
    {
        ScopedActivePrecision coarse(Precision{.fracBits = 2});
        FlexReal x = 0.9;
        FlexReal y = x * x;
        REQUIRE(x.value() == 0.75);
        REQUIRE(y.value() == 0.5);
    }
    FlexReal z = 0.9;
    REQUIRE(z.value() == 0.9);

    ScopedActivePrecision coarse(Precision{.fracBits = 4});
    REQUIRE(FlexReal(0.1).value() == 0.0625); // ScopedActivePrecision header example
}

TEST_CASE( "Header examples: FlexVector and FlexMatrix", "[examples]" ) {
    FlexVector a({1, 2, 3}), b({0.5, 0.5, 0.5});
    FlexVector sum = a + b;
    REQUIRE(sum == FlexVector({1.5, 2.5, 3.5}));
    REQUIRE(a.dot(b) == 3);
    REQUIRE(sum.quantized(Precision{.fracBits = 0}) == FlexVector({1, 2, 3}));

    FlexMatrix m({{1, 2}, {3, 4}});
    FlexMatrix n({{5, 6}, {7, 8}});
    REQUIRE(m * n == FlexMatrix({{19, 22}, {43, 50}}));
    REQUIRE(m * FlexPoint(0.5) == FlexMatrix({{0.5, 1}, {1.5, 2}}));
}
