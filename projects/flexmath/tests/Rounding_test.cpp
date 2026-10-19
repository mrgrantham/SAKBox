#include <FlexPoint.hpp>
#include <FlexPrecision.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

using namespace Flex::Math;

namespace {

constexpr Precision kStochasticWholeNumbers{.fracBits = 0, .rounding = Rounding::Stochastic};
constexpr int kSamples = 100000;

// Average of kSamples stochastic roundings of `value`, via quantize() or
// FlexPoint::quantized().
double meanOfQuantize(double value, const Precision &precision) {
  double sum = 0;
  for (int i = 0; i < kSamples; ++i) {
    sum += quantize(value, precision);
  }
  return sum / kSamples;
}

double meanOfFlexPoint(double value, const Precision &precision) {
  double sum = 0;
  for (int i = 0; i < kSamples; ++i) {
    sum += FlexPoint(value).quantized(precision).value();
  }
  return sum / kSamples;
}

class StochasticRoundingTest : public ::testing::Test {
protected:
  void SetUp() override { seedStochasticRounding(1234); }
};

} // namespace

// Values already on the grid never move, so snapping twice is still the same
// as snapping once.
TEST_F(StochasticRoundingTest, OnGridValuesAreUnchanged) {
  const Precision quarters{.fracBits = 2, .rounding = Rounding::Stochastic};
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(quantize(1.25, quarters), 1.25);
    EXPECT_EQ(quantize(-3.75, quarters), -3.75);
    EXPECT_EQ(FlexPoint(1.25).quantized(quarters), FlexPoint(1.25));
  }
}

// Every result is one of the two grid values around the input.
TEST_F(StochasticRoundingTest, ResultIsANeighbouringGridValue) {
  std::mt19937 rng(7);
  std::uniform_real_distribution<double> values(-100.0, 100.0);
  for (int bits : {-1, 0, 3, 10}) {
    const Precision precision{.fracBits = bits, .rounding = Rounding::Stochastic};
    const double step = precision.step();
    for (int i = 0; i < 2000; ++i) {
      const double value = values(rng);
      const double below = std::floor(value / step) * step;
      const double viaDouble = quantize(value, precision);
      const double viaFlexPoint = FlexPoint(value).quantized(precision).value();
      EXPECT_TRUE(viaDouble == below || viaDouble == below + step) << value << " -> " << viaDouble;
      EXPECT_TRUE(viaFlexPoint == below || viaFlexPoint == below + step)
          << value << " -> " << viaFlexPoint;
    }
  }
}

// Rounds up with probability equal to the distance past the lower value:
// 1.25 becomes 2 a quarter of the time.
TEST_F(StochasticRoundingTest, RoundsUpInProportionToTheFraction) {
  int ups = 0;
  for (int i = 0; i < kSamples; ++i) {
    ups += quantize(1.25, kStochasticWholeNumbers) == 2.0;
  }
  EXPECT_NEAR(static_cast<double>(ups) / kSamples, 0.25, 0.01);
}

// The average of many roundings is the original value, for both signs, for
// both implementations, and even for values far smaller than the step.
TEST_F(StochasticRoundingTest, UnbiasedOnAverage) {
  for (double value : {1.3, -1.3, 0.001, -7.9}) {
    EXPECT_NEAR(meanOfQuantize(value, kStochasticWholeNumbers), value, 0.01) << value;
    EXPECT_NEAR(meanOfFlexPoint(value, kStochasticWholeNumbers), value, 0.01) << value;
  }
}

// The bias stochastic rounding exists to fix: repeatedly adding an amount
// smaller than the step. Truncation loses every increment; nearest rounding
// does too once the increment is under half a step; stochastic rounding keeps
// the right total on average.
TEST_F(StochasticRoundingTest, SmallIncrementsAccumulate) {
  const double increment = 0.001; // 1/64 of a 1/16 step
  auto accumulate = [&](Rounding rounding) {
    const Precision sixteenths{.fracBits = 4, .rounding = rounding};
    double total = 0;
    for (int i = 0; i < 10000; ++i) {
      total = quantize(total + increment, sixteenths);
    }
    return total;
  };
  EXPECT_EQ(accumulate(Rounding::TowardZero), 0.0);
  EXPECT_EQ(accumulate(Rounding::Nearest), 0.0);
  EXPECT_NEAR(accumulate(Rounding::Stochastic), 10.0, 1.0);
}

// The same seed gives the same sequence of roundings; a different seed
// gives a different one.
TEST_F(StochasticRoundingTest, SeedingIsReproducible) {
  auto sequence = [](std::uint64_t seed) {
    seedStochasticRounding(seed);
    std::vector<double> results;
    results.reserve(64);
    for (int i = 0; i < 64; ++i) {
      results.push_back(quantize(0.5, kStochasticWholeNumbers));
    }
    return results;
  };
  EXPECT_EQ(sequence(42), sequence(42));
  EXPECT_NE(sequence(42), sequence(43));
}

// Overflow still applies after stochastic rounding.
TEST_F(StochasticRoundingTest, OverflowStillApplies) {
  const Precision saturating{.fracBits = 0,
                             .intBits = 3,
                             .rounding = Rounding::Stochastic,
                             .overflow = Overflow::Saturate};
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(quantize(100.4, saturating), 7.0);
    EXPECT_EQ(FlexPoint(-100.4).quantized(saturating), FlexPoint(-8.0));
  }
}
