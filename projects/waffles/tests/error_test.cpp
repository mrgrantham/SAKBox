// Ex, Waffles' exception.
#include <gtest/gtest.h>

#include <WClasses/GError.h>

using namespace GClasses;

TEST(ExTest, MessageIsThePartsJoined) {
  EXPECT_STREQ(Ex("a", "b", "c").what(), "abc");
  GExpectException expected; // the other branch of Ex::setMessage
  EXPECT_STREQ(Ex("d", "e").what(), "de");
}

// parseNumber: the strict replacement for atoi and atof, which return 0 for
// "abc" and 12 for "12x" and overflow silently
TEST(ParseNumberTest, OnlyAWholeNumberInRange) {
  int i = 0;
  EXPECT_TRUE(parseNumber("42", &i));
  EXPECT_EQ(i, 42);
  EXPECT_TRUE(parseNumber("-7", &i));
  EXPECT_EQ(i, -7);
  for (const char *bad : {"", "abc", "12x", " 5", "+5", "1.5", "99999999999"})
    EXPECT_FALSE(parseNumber(bad, &i)) << "\"" << bad << "\"";
  unsigned int u = 0;
  EXPECT_FALSE(parseNumber("-1", &u));
  EXPECT_FALSE(parseNumber("4294967296", &u));
  double d = 0;
  for (const char *good : {"1.5", "-2E-3", "1e5", ".5", "-.25", "3"})
    EXPECT_TRUE(parseNumber(good, &d)) << good;
  EXPECT_DOUBLE_EQ(d, 3.0);
  EXPECT_TRUE(parseNumber("-2E-3", &d));
  EXPECT_DOUBLE_EQ(d, -0.002);
  for (const char *bad : {"", "1.5abc", "1.2.3", "abc", "nan", "inf", "1e999"})
    EXPECT_FALSE(parseNumber(bad, &d)) << "\"" << bad << "\"";
  // A range of characters, for numbers inside longer text
  const char *text = "[0.75]";
  EXPECT_TRUE(parseNumber(text + 1, text + 5, &d));
  EXPECT_DOUBLE_EQ(d, 0.75);
  // On failure the output is left alone
  i = 9;
  EXPECT_FALSE(parseNumber("x", &i));
  EXPECT_EQ(i, 9);
}
