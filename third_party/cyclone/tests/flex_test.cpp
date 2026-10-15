// Tests for the adjustable-precision real (SAKBox addition).
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>

#include <type_traits>

using Flex::Math::FlexReal;
using Flex::Math::Precision;
using Flex::Math::Rounding;
using Flex::Math::ScopedActivePrecision;
using cyclone::real;

static_assert(std::is_same_v<real, FlexReal>, "cyclone_flex must build with FlexReal");
static_assert(std::is_trivially_copyable_v<FlexReal>, "FlexReal must stay usable in unions");

namespace {

// Sets the engine-wide precision for one test and restores it afterwards.
class ScopedPrecision : public ScopedActivePrecision {
public:
    ScopedPrecision(int bits, bool roundToNearest)
        : ScopedActivePrecision(Precision{
              .fracBits = bits,
              .rounding = roundToNearest ? Rounding::Nearest : Rounding::TowardZero}) {}
};

// One second of free flight in 1/64 s steps, as the engine integrates it.
cyclone::Particle simulate(double vx, double damping, double gravity)
{
    cyclone::Particle p;
    p.setMass(1.0);
    p.setPosition(0, 0, 0);
    p.setVelocity(vx, 0, 0);
    p.setAcceleration(0, -gravity, 0);
    p.setDamping(damping);
    for (int i = 0; i < 64; ++i) p.integrate(1.0 / 64.0);
    return p;
}

} // namespace

TEST(FlexRealTest, FullPrecisionMatchesDouble)
{
    ScopedPrecision full(52, false);
    EXPECT_DOUBLE_EQ(FlexReal(0.1).value(), 0.1);
    EXPECT_DOUBLE_EQ((FlexReal(1.5) * 2.25).value(), 3.375);
}

TEST(FlexRealTest, SnapsEveryOperation)
{
    ScopedPrecision coarse(2, false); // grid of 0.25, truncating
    EXPECT_DOUBLE_EQ(FlexReal(0.9).value(), 0.75);
    EXPECT_DOUBLE_EQ(FlexReal(-0.9).value(), -0.75);
    // 0.75 * 0.75 = 0.5625 snaps to 0.5
    EXPECT_DOUBLE_EQ((FlexReal(0.75) * FlexReal(0.75)).value(), 0.5);
    EXPECT_DOUBLE_EQ(real_sqrt(FlexReal(2.0)).value(), 1.25);
}

TEST(FlexRealTest, RoundsToNearestWhenAsked)
{
    ScopedPrecision coarse(2, true);
    EXPECT_DOUBLE_EQ(FlexReal(0.9).value(), 1.0);
    EXPECT_DOUBLE_EQ(FlexReal(-0.9).value(), -1.0);
}

TEST(FlexRealTest, EngineMatchesAnalyticAtFullPrecision)
{
    ScopedPrecision full(52, false);
    // Cyclone moves position with the old velocity, then updates velocity:
    // y = -g * dt^2 * (0 + 1 + ... + 63)
    cyclone::Particle p = simulate(0.0, 1.0, 10.0);
    EXPECT_NEAR(p.getPosition().y, -10.0 * (63.0 * 64.0 / 2.0) / 4096.0, 1e-9);
}

TEST(FlexRealTest, LowPrecisionChangesTheTrajectory)
{
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

TEST(FlexRealTest, DampingStopsWorkingAtLowPrecision)
{
    {
        ScopedPrecision full(52, false);
        EXPECT_LT(simulate(1.0, 0.9, 0.0).getVelocity().x, 0.95);
    }
    {
        // pow(damping, 1/64) is ~0.9985, which rounds to exactly 1.0 on a
        // 1/64 grid: the particle never slows down.
        ScopedPrecision coarse(6, true);
        EXPECT_DOUBLE_EQ(simulate(1.0, 0.9, 0.0).getVelocity().x, 1.0);
    }
}
