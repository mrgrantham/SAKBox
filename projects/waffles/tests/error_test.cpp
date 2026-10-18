// Ex, Waffles' exception.
#include <gtest/gtest.h>

#include <WClasses/GError.h>

using namespace GClasses;

TEST(ExTest, MessageIsThePartsJoined) {
  EXPECT_STREQ(Ex("a", "b", "c").what(), "abc");
  GExpectException expected; // the other branch of Ex::setMessage
  EXPECT_STREQ(Ex("d", "e").what(), "de");
}
