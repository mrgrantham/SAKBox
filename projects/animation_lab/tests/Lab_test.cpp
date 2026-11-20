// The lab's three ways each make a motion that does what was asked, flat
// too, and each bakes into a small motion that stays close to it
#include <AnimationLab/Lab.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace AnimationLab;

namespace {
double gap(const Point &a, const Point &b) {
  return std::hypot(double(a[0]) - b[0], double(a[1]) - b[1], double(a[2]) - b[2]);
}
} // namespace

TEST(LabTest, ASolvedThrowLandsWhereAsked) {
  const Recording thrown = solveThrow({.from = {0, 0, 0}, .to = {4, 1, -2}, .seconds = 1.2});
  EXPECT_NEAR(thrown.seconds(), 1.2, 1e-9);
  EXPECT_LT(gap(thrown.positions.back(), {4, 1, -2}), 1e-3);
  // (an arc: up above both ends on the way)
  float highest = -1e9f;
  for (const Point &p : thrown.positions)
    highest = std::max(highest, p[1]);
  EXPECT_GT(highest, 1.5f);
}

TEST(LabTest, FlatStaysFlat) {
  const Recording thrown =
      solveThrow({.from = {0, 0, 5}, .to = {3, 0, 9}, .seconds = 1, .flat = true});
  for (const Point &p : thrown.positions)
    EXPECT_FLOAT_EQ(p[2], 0);
  EXPECT_LT(gap(thrown.positions.back(), {3, 0, 0}), 1e-3);
}

TEST(LabTest, AGuidedMotionLagsOvershootsAndSettles) {
  // (its swing dies away as e^(-damping t / 2): 1.5 s to settle)
  Guide guide{
      .keys = {{0, {0, 0, 0}}, {0.5, {2, 0, 0}}}, .stiffness = 150, .damping = 6, .settle = 1.5};
  const Recording guided = guideAlong(guide);
  // (behind the guide on the way, past the end once, then settled on it)
  const auto halfway = static_cast<std::size_t>(std::lround(0.275 / guided.step));
  EXPECT_LT(guided.positions[halfway][0], 2 * 0.275f / 0.5f);
  float furthest = 0;
  for (const Point &p : guided.positions)
    furthest = std::max(furthest, p[0]);
  EXPECT_GT(furthest, 2.02f);
  EXPECT_LT(gap(guided.positions.back(), {2, 0, 0}), 0.05);
}

TEST(LabTest, ASearchFindsAThrowWithDrag) {
  const Search search{.goal = {.from = {0, 1, 0}, .to = {5, 0, 0}, .seconds = 1.5}, .drag = 0.5};
  const Found found = searchThrow(search);
  EXPECT_LT(found.miss, 0.02);
}

TEST(LabTest, ASearchFindsAThrowThatBounces) {
  // (to land on a point after bouncing off the floor: no formula for it)
  const Search search{.goal = {.from = {0, 1, 0}, .to = {5, 0.3f, 0}, .seconds = 1.5},
                      .drag = 0.8,
                      .floor = 0,
                      .bounce = 0.6,
                      .rounds = 20,
                      .triesARound = 60};
  const Found found = searchThrow(search);
  EXPECT_LT(found.miss, 0.05);
  bool bounced = false;
  for (const Point &p : found.recording.positions)
    bounced = bounced || p[1] <= 0;
  EXPECT_TRUE(bounced);
}

TEST(LabTest, BakedMotionsStayCloseToTheirRecordings) {
  const Recording thrown = solveThrow({.from = {0, 0, 0}, .to = {4, 0, 0}, .seconds = 1});
  const auto baked = bake(thrown);
  EXPECT_EQ(baked.path.size(), 24u);
  EXPECT_EQ(baked.timing.size(), 24u);
  EXPECT_LT(bakeError(thrown, baked), 0.03); // (of a 4-unit throw)
  Guide guide{.keys = {{0, {0, 0, 0}}, {0.4, {1, 1, 0}}, {0.8, {2, 0, 0}}}};
  const Recording guided = guideAlong(guide);
  EXPECT_LT(bakeError(guided, bake(guided, 32, 32)), 0.05); // (its turns at the keys are sharper)
}

// Saved and read back the same
TEST(LabTest, ABakedMotionSavesAsText) {
  const auto baked = bake(solveThrow({.from = {0, 0, 0}, .to = {2, 1, 0}, .seconds = 0.8}), 8, 8);
  const auto read = readText(writeText(baked));
  ASSERT_TRUE(read.has_value());
  const ScratchPadd::Motion::BakedMotion &again = read.value();
  EXPECT_FLOAT_EQ(again.seconds, baked.seconds);
  ASSERT_EQ(again.path.size(), baked.path.size());
  ASSERT_EQ(again.timing.size(), baked.timing.size());
  EXPECT_NEAR(again.at(0.4f)[1], baked.at(0.4f)[1], 1e-5);
  EXPECT_FALSE(readText("nonsense 1 2 3").has_value());
}
