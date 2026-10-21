// WMath.
#include <gtest/gtest.h>

#include <WClasses/WMath.h>

using namespace WClasses;

// The normal approximation centres on n(n+1)/4, so a statistic equal to it
// gives p = 1. With n(n+1)/4 truncated (45 instead of 45.5 for n = 13) the
// p-value came out above 1.
TEST(WilcoxonTest, StatisticAtItsMeanGivesPOne) {
  EXPECT_NEAR(WMath::wilcoxonPValue(13, 13.0 * 14.0 / 4.0), 1.0, 1e-12);
  // (The statistic is the smaller rank sum, at or below the mean.)
  const double p = WMath::wilcoxonPValue(13, 30.0);
  EXPECT_GT(p, 0.0);
  EXPECT_LT(p, 1.0);
}
