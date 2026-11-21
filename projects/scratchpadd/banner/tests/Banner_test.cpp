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

// Slides on one band: the first's words go off as the second's come in; a
// waiting slide holds (the band up, the words in the middle) until it's
// released, and only then does the band go
TEST(BannerTest, SlidesAndOneThatWaits) {
  Banner banner(256);
  banner.start(0, {{"YOUR TURN", ""}, {"ROLL THE DICE", "touch Roll", true}}, {235, 70, 70});
  EXPECT_EQ(banner.at(1.0)->title, "YOUR TURN");
  EXPECT_TRUE(banner.moving(1.0));
  const auto held = banner.at(10);
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(held->title, "ROLL THE DICE");
  EXPECT_FLOAT_EQ(held->wordsX, 128);
  EXPECT_FLOAT_EQ(held->bandAlpha, 1);
  EXPECT_TRUE(banner.waiting(10));
  EXPECT_FALSE(banner.moving(10));
  EXPECT_TRUE(banner.at(1000).has_value()); // (still there)
  banner.release(1000);
  EXPECT_TRUE(banner.moving(1000.1));
  EXPECT_LT(banner.at(1000.3)->wordsX, 128); // (going)
  EXPECT_FALSE(banner.at(1001).has_value()); // (gone, the band faded)
}

// A prompt added while the turn's banner shows comes on the same band;
// after it's gone, a new banner. B (hurry) goes straight to the prompt
TEST(BannerTest, ThenOnTheSameBandAndHurry) {
  Banner banner(256);
  banner.start(0, "YOUR TURN", "", {235, 70, 70});
  banner.then(0.5, {"ROLL THE DICE", "", true}, {235, 70, 70});
  EXPECT_EQ(banner.at(1.0)->title, "YOUR TURN");
  EXPECT_EQ(banner.at(5)->title, "ROLL THE DICE");
  banner.hurry(0.5);
  EXPECT_EQ(banner.at(0.5)->title, "ROLL THE DICE");
  EXPECT_TRUE(banner.waiting(0.5));
  EXPECT_FLOAT_EQ(banner.at(0.5)->bandAlpha, 1);
  Banner plain(256);
  plain.start(0, "YOUR TURN", "", {235, 70, 70});
  plain.hurry(0.2); // (nothing waits: gone)
  EXPECT_FALSE(plain.showing(0.2));
  plain.then(5, {"ROLL THE DICE", "", true}, {235, 70, 70}); // (after: a banner of its own)
  EXPECT_TRUE(plain.showing(5));
  EXPECT_FALSE(plain.waiting(5)); // (its words still coming in)
  EXPECT_TRUE(plain.waiting(6));
  EXPECT_NEAR(plain.remaining(5), Banner::kWordsInAfter + Banner::kWordsIn, 1e-9);
}
