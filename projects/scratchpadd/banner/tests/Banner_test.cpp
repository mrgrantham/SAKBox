// A banner: the band fading in and out, the words in
// from the right to the middle, held, then off to the left; over after its
// time, and not before it starts
#include <ScratchPadd/Banner/Banner.hpp>

#include <gtest/gtest.h>

using ScratchPadd::Banner;

TEST(BannerTest, InHoldOut) {
  Banner banner(256);
  EXPECT_FALSE(banner.at(0).has_value()); // (not started)
  banner.start(10, "YOUR TURN", "roll the dice", {235, 70, 70});
  const auto first = banner.at(10);
  ASSERT_TRUE(first.has_value());
  EXPECT_FLOAT_EQ(first->bandAlpha, 0);
  EXPECT_GT(first->wordsX, 256); // (off to the right)
  // Held in the middle, the band full
  const auto held = banner.at(10 + 1.0);
  ASSERT_TRUE(held.has_value());
  EXPECT_FLOAT_EQ(held->wordsX, 128);
  EXPECT_FLOAT_EQ(held->bandAlpha, 1);
  EXPECT_EQ(held->title, "YOUR TURN");
  // Going: left of the middle, the band fading
  const auto going = banner.at(10 + banner.seconds() - 0.1);
  ASSERT_TRUE(going.has_value());
  EXPECT_LT(going->wordsX, 0);
  EXPECT_LT(going->bandAlpha, 1);
  EXPECT_FALSE(banner.at(10 + banner.seconds() + 0.01).has_value());
  EXPECT_GT(banner.seconds(), 2.0f);
  EXPECT_LT(banner.seconds(), 3.0f);
}
