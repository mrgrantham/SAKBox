// GTime.
#include <gtest/gtest.h>

#include <WClasses/GTime.h>

using namespace GClasses;

// GTime::fromString's numbers (read with atoi and atof before parseNumber)
TEST(TimeFromStringTest, ParsesAndRejects) {
  double t = 0;
  EXPECT_TRUE(GTime::fromString(&t, "1970-01-02 00:00:00", "YYYY-MM-DD hh:mm:ss"));
  EXPECT_DOUBLE_EQ(t, 86400.0);
  EXPECT_TRUE(GTime::fromString(&t, "1970-01-01 00:00:01.5", "YYYY-MM-DD hh:mm:ssss"));
  EXPECT_DOUBLE_EQ(t, 1.5);
  for (const char *bad : {"1970-13-01 00:00:00", "19x0-01-01 00:00:00",
                          "1970-01-01 00:00:-1"})
    EXPECT_FALSE(GTime::fromString(&t, bad, "YYYY-MM-DD hh:mm:ss")) << bad;
}
