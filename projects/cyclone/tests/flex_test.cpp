// Tests for the adjustable-precision real (SAKBox addition).
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>

#include <type_traits>

using cyclone::real;
using Flex::Math::FlexReal;
using Flex::Math::Precision;
using Flex::Math::Rounding;
using Flex::Math::ScopedActivePrecision;

static_assert(std::is_same_v<real, FlexReal>, "cyclone_flex must build with FlexReal");
static_assert(std::is_trivially_copyable_v<FlexReal>, "FlexReal must stay usable in unions");

namespace {

// Sets the engine-wide precision for one test and restores it afterwards.
class ScopedPrecision : public ScopedActivePrecision {
public:
  ScopedPrecision(int bits, bool roundToNearest)
      : ScopedPrecision(bits, roundToNearest ? Rounding::Nearest : Rounding::TowardZero) {}
  ScopedPrecision(int bits, Rounding rounding)
      : ScopedActivePrecision(Precision{.fracBits = bits, .rounding = rounding}) {}
};

// Speed after `frames` 16 ms frames of drag alone, as in the flight sim.
double speedAfterDrag(double speed, int frames) {
  cyclone::Particle p;
  p.setMass(1.0);
  p.setPosition(0, 0, 0);
  p.setVelocity(speed, 0, 0);
  p.setAcceleration(0, 0, 0);
  p.setDamping(0.8);
  for (int i = 0; i < frames; ++i)
    p.integrate(0.016);
  return p.getVelocity().x;
}

// One second of free flight in 1/64 s steps, as the engine integrates it.
cyclone::Particle simulate(double vx, double damping, double gravity) {
  cyclone::Particle p;
  p.setMass(1.0);
  p.setPosition(0, 0, 0);
  p.setVelocity(vx, 0, 0);
  p.setAcceleration(0, -gravity, 0);
  p.setDamping(damping);
  for (int i = 0; i < 64; ++i)
    p.integrate(1.0 / 64.0);
  return p;
}

} // namespace

TEST(FlexRealTest, FullPrecisionMatchesDouble) {
  ScopedPrecision full(52, false);
  EXPECT_DOUBLE_EQ(FlexReal(0.1).value(), 0.1);
  EXPECT_DOUBLE_EQ((FlexReal(1.5) * 2.25).value(), 3.375);
}

TEST(FlexRealTest, SnapsEveryOperation) {
  ScopedPrecision coarse(2, false); // grid of 0.25, truncating
  EXPECT_DOUBLE_EQ(FlexReal(0.9).value(), 0.75);
  EXPECT_DOUBLE_EQ(FlexReal(-0.9).value(), -0.75);
  // 0.75 * 0.75 = 0.5625 snaps to 0.5
  EXPECT_DOUBLE_EQ((FlexReal(0.75) * FlexReal(0.75)).value(), 0.5);
  EXPECT_DOUBLE_EQ(real_sqrt(FlexReal(2.0)).value(), 1.25);
}

TEST(FlexRealTest, RoundsToNearestWhenAsked) {
  ScopedPrecision coarse(2, true);
  EXPECT_DOUBLE_EQ(FlexReal(0.9).value(), 1.0);
  EXPECT_DOUBLE_EQ(FlexReal(-0.9).value(), -1.0);
}

TEST(FlexRealTest, EngineMatchesAnalyticAtFullPrecision) {
  ScopedPrecision full(52, false);
  // Cyclone moves position with the old velocity, then updates velocity:
  // y = -g * dt^2 * (0 + 1 + ... + 63)
  cyclone::Particle p = simulate(0.0, 1.0, 10.0);
  EXPECT_NEAR(p.getPosition().y, -10.0 * (63.0 * 64.0 / 2.0) / 4096.0, 1e-9);
}

TEST(FlexRealTest, LowPrecisionChangesTheTrajectory) {
  double fullY, coarseY;
  {
    ScopedPrecision full(52, false);
    fullY = simulate(0.0, 1.0, 10.0).getPosition().y;
  }
  {
    ScopedPrecision coarse(6, false);
    coarseY = simulate(0.0, 1.0, 10.0).getPosition().y;
  }
  // Truncating each v * dt toward zero loses part of every step's fall
  EXPECT_GT(coarseY, fullY + 0.05);
}

TEST(FlexRealTest, DampingStopsWorkingAtLowPrecision) {
  {
    ScopedPrecision full(52, false);
    EXPECT_LT(simulate(1.0, 0.9, 0.0).getVelocity().x, 0.95);
  }
  {
    // Each frame's drag removes ~0.0016 from a speed of 1.0, less than
    // half the 1/64 step, so rounding to nearest restores 1.0 every
    // frame: the particle never slows down.
    ScopedPrecision coarse(6, true);
    EXPECT_DOUBLE_EQ(simulate(1.0, 0.9, 0.0).getVelocity().x, 1.0);
  }
}

// The time step and the drag factor keep full precision; only the result
// snaps. pow(0.8, 0.016) = 0.99644, so a speed of 10 becomes 9.9644, which
// truncates to 9.953125 on a 1/64 grid. Snapping the factor first (to
// 0.984375) gave 9.84375, four times the drag.
TEST(FlexRealTest, DragFactorIsNotSnapped) {
  ScopedPrecision coarse(6, Rounding::TowardZero);
  EXPECT_DOUBLE_EQ(speedAfterDrag(10.0, 1), 9.953125);
}

// Over one second (62 frames) of drag, stochastic rounding matches full
// precision on average even on a 1/16 grid, where truncation drifts low.
TEST(FlexRealTest, StochasticRoundingKeepsDragUnbiased) {
  double full;
  {
    ScopedPrecision exact(52, false);
    full = speedAfterDrag(10.0, 62); // ~8.0
  }
  double stochasticSum = 0;
  const int runs = 500;
  for (int seed = 0; seed < runs; ++seed) {
    Flex::Math::seedStochasticRounding(seed);
    ScopedPrecision coarse(4, Rounding::Stochastic);
    stochasticSum += speedAfterDrag(10.0, 62);
  }
  EXPECT_NEAR(stochasticSum / runs, full, 0.05);

  ScopedPrecision coarse(4, Rounding::TowardZero);
  EXPECT_LT(speedAfterDrag(10.0, 62), full - 1.0);
}
