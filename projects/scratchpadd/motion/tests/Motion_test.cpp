// Motion: curves from 0 to 1 that keep their shape; tracks carrying a value
// on the caller's clock; scripts timing steps one after another or
// together; fixed steps that catch up, but not without limit
#include <ScratchPadd/Motion/Motion.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using namespace ScratchPadd::Motion;

TEST(MotionTest, CurvesGoFromNoneToAllAndKeepTheirShape) {
  for (Curve curve :
       {Curve::Linear, Curve::EaseIn, Curve::EaseOut, Curve::EaseInOut, Curve::Pull}) {
    EXPECT_FLOAT_EQ(along(curve, 0), 0);
    EXPECT_FLOAT_EQ(along(curve, 1), 1);
    EXPECT_FLOAT_EQ(along(curve, -1), 0); // (clamped)
    EXPECT_FLOAT_EQ(along(curve, 2), 1);
    float last = 0;
    for (int i = 1; i <= 100; ++i) { // never going back
      const float now = along(curve, static_cast<float>(i) / 100);
      EXPECT_GE(now, last);
      last = now;
    }
  }
  // Slow at the ends where they should be, and the in-and-outs symmetric
  EXPECT_LT(along(Curve::EaseIn, 0.25f), 0.25f);
  EXPECT_GT(along(Curve::EaseOut, 0.25f), 0.25f);
  for (Curve curve : {Curve::EaseInOut, Curve::Pull}) {
    EXPECT_FLOAT_EQ(along(curve, 0.5f), 0.5f);
    EXPECT_NEAR(along(curve, 0.2f), 1 - along(curve, 0.8f), 1e-6);
    EXPECT_LT(along(curve, 0.1f), 0.1f);
  }
}

// Settle goes past the end, by about a tenth, and comes back to it
TEST(MotionTest, SettleOvershootsThenSettles) {
  EXPECT_FLOAT_EQ(along(Curve::Settle, 0), 0);
  EXPECT_NEAR(along(Curve::Settle, 1), 1, 1e-6);
  float most = 0;
  for (int i = 0; i <= 100; ++i)
    most = std::max(most, along(Curve::Settle, static_cast<float>(i) / 100));
  EXPECT_GT(most, 1.08f);
  EXPECT_LT(most, 1.12f);
}

TEST(MotionTest, ATrackCarriesAValueOnTheCallersClock) {
  const Track<float> rise{
      .from = 192, .to = 148, .start = 10, .seconds = 0.5f, .curve = Curve::Linear};
  EXPECT_FLOAT_EQ(rise.at(9), 192);     // not started
  EXPECT_FLOAT_EQ(rise.at(10.25), 170); // half way
  EXPECT_FLOAT_EQ(rise.at(11), 148);    // done, and staying there
  EXPECT_FALSE(rise.done(10.4));
  EXPECT_TRUE(rise.done(10.5));
  const Track<std::array<float, 2>> slide{
      .from = {0, 0}, .to = {10, -20}, .start = 0, .seconds = 2, .curve = Curve::Linear};
  const auto middle = slide.at(1);
  EXPECT_FLOAT_EQ(middle[0], 5);
  EXPECT_FLOAT_EQ(middle[1], -10);
  // No time at all: there at once
  const Track<float> jump{.from = 0, .to = 1, .start = 0, .seconds = 0};
  EXPECT_FLOAT_EQ(jump.at(0), 1);
}

TEST(MotionTest, AScriptTimesItsSteps) {
  // A card: up for 0.25 s, then a turn of 0.2 s; a glow with the turn, a
  // tenth of a second after it starts
  Script card;
  const auto up = card.add(0.25f);
  const auto turn = card.after(up, 0.2f);
  const auto glow = card.with(turn, 0.3f, 0.1f);
  EXPECT_FLOAT_EQ(card.startOf(turn), 0.25f);
  EXPECT_FLOAT_EQ(card.startOf(glow), 0.35f);
  EXPECT_FLOAT_EQ(card.seconds(), 0.65f);
  EXPECT_FLOAT_EQ(card.progress(up, 0.125f), 0.5f);
  EXPECT_FLOAT_EQ(card.progress(turn, 0.1f), 0); // not yet
  EXPECT_FLOAT_EQ(card.progress(turn, 0.35f), 0.5f);
  EXPECT_FALSE(card.done(0.6f));
  EXPECT_TRUE(card.done(0.65f));
}

TEST(MotionTest, FixedStepsCatchUpButNotWithoutLimit) {
  FixedSteps physics(0.1);
  EXPECT_EQ(physics.due(0.05, 4), 0);
  EXPECT_EQ(physics.due(0.25, 4), 2); // 0.1 and 0.2 due
  EXPECT_EQ(physics.due(0.31, 4), 1);
  // A long stall: only 4, and it doesn't owe the rest afterwards
  EXPECT_EQ(physics.due(5.0, 4), 4);
  EXPECT_LE(physics.due(5.1, 4), 1);
}

// Gentle: from none to all, half way at the middle, and softer than
// EaseInOut at both ends
TEST(MotionTest, GentleIsSofterAtBothEnds) {
  using ScratchPadd::Motion::along;
  using ScratchPadd::Motion::Curve;
  EXPECT_FLOAT_EQ(along(Curve::Gentle, 0), 0);
  EXPECT_FLOAT_EQ(along(Curve::Gentle, 1), 1);
  EXPECT_NEAR(along(Curve::Gentle, 0.5f), 0.5f, 1e-6f);
  EXPECT_LT(along(Curve::Gentle, 0.1f), along(Curve::EaseInOut, 0.1f));
  EXPECT_GT(along(Curve::Gentle, 0.9f), along(Curve::EaseInOut, 0.9f));
}

// Anticipate: back a little first (about a tenth), then all the way
TEST(MotionTest, AnticipateWindsUpFirst) {
  using ScratchPadd::Motion::along;
  using ScratchPadd::Motion::Curve;
  EXPECT_FLOAT_EQ(along(Curve::Anticipate, 0), 0);
  EXPECT_NEAR(along(Curve::Anticipate, 1), 1, 1e-5f);
  float lowest = 0;
  for (float t = 0; t <= 1; t += 0.01f)
    lowest = std::min(lowest, along(Curve::Anticipate, t));
  EXPECT_LT(lowest, -0.08f);
  EXPECT_GT(lowest, -0.12f);
}

// An arc: from its start to its end, its middle pushed out by the bulge
TEST(MotionTest, AnArcBulgesAtItsMiddle) {
  using ScratchPadd::Motion::arc;
  EXPECT_FLOAT_EQ(arc(0.0f, 10.0f, 3.0f, 0), 0);
  EXPECT_FLOAT_EQ(arc(0.0f, 10.0f, 3.0f, 1), 10);
  EXPECT_FLOAT_EQ(arc(0.0f, 10.0f, 3.0f, 0.5f), 8);     // (5, the middle, plus 3)
  EXPECT_FLOAT_EQ(arc(0.0f, 10.0f, 0.0f, 0.25f), 2.5f); // (no bulge: a straight line)
}

// Showing: on from its start, how long since, off again
TEST(MotionTest, ShowingCountsSinceItStarted) {
  ScratchPadd::Motion::Showing showing;
  EXPECT_FALSE(showing.on());
  showing.start(4.0);
  EXPECT_TRUE(showing.on());
  EXPECT_DOUBLE_EQ(showing.seconds(6.5), 2.5);
  showing.stop();
  EXPECT_FALSE(showing.on());
}
