// WTime.
#include <gtest/gtest.h>

#include <WClasses/WTime.h>

#include <ctime>
#include <string>

using namespace WClasses;

// WTime::fromString's numbers (read with atoi and atof before parseNumber)
TEST(TimeFromStringTest, ParsesAndRejects) {
  double t = 0;
  EXPECT_TRUE(WTime::fromString(&t, "1970-01-02 00:00:00", "YYYY-MM-DD hh:mm:ss"));
  EXPECT_DOUBLE_EQ(t, 86400.0);
  EXPECT_TRUE(WTime::fromString(&t, "1970-01-01 00:00:01.5", "YYYY-MM-DD hh:mm:ssss"));
  EXPECT_DOUBLE_EQ(t, 1.5);
  for (const char *bad : {"1970-13-01 00:00:00", "19x0-01-01 00:00:00", "1970-01-01 00:00:-1"})
    EXPECT_FALSE(WTime::fromString(&t, bad, "YYYY-MM-DD hh:mm:ss")) << bad;
}

// HTTP dates (the Date and Last-Modified headers) are always in English, in
// GMT, with a two-digit day. They were made from asctime, which pads days
// under 10 with a space, and that space was dropped: "Thu, 5 Nov 2026".
TEST(TimeHttpDateTest, Rfc1123Format) {
  EXPECT_EQ(WTime::httpDate(0), "Thu, 01 Jan 1970 00:00:00 GMT");
  EXPECT_EQ(WTime::httpDate(1793862489), "Thu, 05 Nov 2026 07:08:09 GMT");
  EXPECT_EQ(WTime::httpDate(951868799), "Tue, 29 Feb 2000 23:59:59 GMT");
}

// asciiTime gives what asctime gives, without its newline (checked against
// strftime with asctime's format). It no longer uses asctime, gmtime or
// localtime, which share one buffer between all threads.
#ifndef WINDOWS
TEST(TimeAsciiTimeTest, SameAsAsctime) {
  for (bool gmt : {true, false}) {
    auto expected = [gmt](time_t t) {
      struct tm parts;
      if (gmt)
        gmtime_r(&t, &parts);
      else
        localtime_r(&t, &parts);
      char buf[64]; // asctime's format, in the C locale tests run in
      strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Y", &parts);
      return std::string(buf);
    };
    const time_t before = time(nullptr);
    char out[64];
    const std::string got = WTime::asciiTime(out, sizeof(out), gmt);
    const time_t after = time(nullptr); // (in case a second ticked over)
    EXPECT_TRUE(got == expected(before) || got == expected(after)) << got;
  }
}
#endif
