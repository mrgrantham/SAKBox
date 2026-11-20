// A baked motion plays back: at its start and end at the ends of time,
// through its points, and as fast or slow as its timing says
#include <ScratchPadd/Motion/BakedMotion.hpp>

#include <gtest/gtest.h>

#include <cmath>

using ScratchPadd::Motion::BakedMotion;

TEST(BakedMotionTest, PlaysAlongItsPathByItsTiming) {
  BakedMotion line;
  line.path = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
  line.timing = {0, 0.5f, 1}; // (half the way in the first half of the time)
  line.seconds = 2;
  EXPECT_FLOAT_EQ(line.at(0)[0], 0);
  EXPECT_FLOAT_EQ(line.at(2)[0], 3);
  EXPECT_FLOAT_EQ(line.at(5)[0], 3); // (held at the end)
  EXPECT_FLOAT_EQ(line.at(-1)[0], 0);
  EXPECT_NEAR(line.at(1)[0], 1.5f, 1e-5);         // (halfway along at halfway)
  EXPECT_NEAR(line.onPath(1.0f / 3)[0], 1, 1e-5); // (through its points)
  // A slow start: the same path, another timing
  line.timing = {0, 0.1f, 1};
  EXPECT_NEAR(line.at(1)[0], 0.3f, 1e-5);
}

TEST(BakedMotionTest, ACurveThroughItsPoints) {
  BakedMotion arc;
  arc.path = {{0, 0, 0}, {1, 1, 0}, {2, 0, 0}};
  arc.timing = {0, 1};
  arc.seconds = 1;
  const auto top = arc.onPath(0.5f);
  EXPECT_NEAR(top[0], 1, 1e-5);
  EXPECT_NEAR(top[1], 1, 1e-5);
  EXPECT_GT(arc.onPath(0.25f)[1], 0.5f); // (curving up to it, not a straight line)
}

// Turns between moments: halfway is half the turn, the shorter way round
TEST(BakedMotionTest, TurnsBetweenMoments) {
  BakedMotion spin;
  spin.seconds = 1;
  spin.timing = {0, 1};
  spin.path = {{0, 0, 0}, {1, 0, 0}};
  const float half = std::sqrt(0.5f);
  spin.turns = {{1, 0, 0, 0}, {-half, 0, -half, 0}}; // (90 degrees about y, as -q)
  const auto mid = spin.turnAt(0.5f);
  EXPECT_NEAR(mid[0], std::cos(3.14159265f / 8), 1e-3); // (45 degrees: cos 22.5)
  EXPECT_NEAR(mid[2], std::sin(3.14159265f / 8), 1e-3);
  EXPECT_FLOAT_EQ(BakedMotion{}.turnAt(0.3f)[0], 1);
}
