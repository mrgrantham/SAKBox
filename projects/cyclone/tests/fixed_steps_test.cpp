// The rigid-body demos' physics clock (src/demos/fixed_steps.h)
#include <gtest/gtest.h>

#include <fixed_steps.h>

#include <cmath>

namespace {

constexpr double kStep = FixedSteps::kDefaultStepSeconds;

// How many steps `frames` frames `seconds` long take in all
unsigned stepsOver(FixedSteps &clock, int frames, double seconds) {
  unsigned steps = 0;
  for (int i = 0; i < frames; ++i)
    steps += clock.stepsFor(seconds);
  return steps;
}

} // namespace

// At 60 Hz, frame times wobble (TimingData counts whole milliseconds: 16
// and 17): every frame still takes exactly one step
TEST(FixedStepsTest, OneStepAFrameAt60Hz) {
  FixedSteps clock;
  for (int i = 0; i < 600; ++i)
    ASSERT_EQ(clock.stepsFor(i % 3 == 0 ? 0.016 : 0.017), 1u) << "frame " << i;
}

// The steps keep up with time whatever the frame rate: a second of 144 Hz
// frames, or of 20 Hz ones, is 60 steps
TEST(FixedStepsTest, SixtyStepsASecondAtAnyFrameRate) {
  for (double hertz : {144.0, 120.0, 30.0, 20.0}) {
    SCOPED_TRACE(testing::Message() << hertz << " Hz");
    FixedSteps clock;
    const int frames = static_cast<int>(hertz * 10);
    const unsigned steps = stepsOver(clock, frames, 1 / hertz);
    EXPECT_NEAR(steps, 600, 1);
  }
}

// A stall counts as kLongestFrame: the simulation slows rather than taking
// a burst of steps to catch up
TEST(FixedStepsTest, AStallDoesntPileUpSteps) {
  FixedSteps clock;
  EXPECT_EQ(clock.stepsFor(2.0),
            static_cast<unsigned>(std::lround(FixedSteps::kLongestFrame / kStep)));
  EXPECT_EQ(clock.stepsFor(-1.0), 0u);
}

// The step length can change as it runs: 240 steps a second take four
// steps a 60 Hz frame, and time not yet stepped carries over
TEST(FixedStepsTest, StepLengthCanChange) {
  FixedSteps clock;
  EXPECT_EQ(clock.stepsFor(0.010), 0u); // 10 ms not yet a step
  clock.setStepSeconds(1.0 / 240);
  EXPECT_DOUBLE_EQ(clock.stepSeconds(), 1.0 / 240);
  EXPECT_EQ(clock.stepsFor(0.0), 2u); // the 10 ms are two of the new steps
  EXPECT_NEAR(stepsOver(clock, 60, 1.0 / 60), 240, 1);
  clock.setSubSteps(4);
  EXPECT_DOUBLE_EQ(clock.subStepSeconds(), 1.0 / 960);
}

TEST(FixedStepsTest, SubStepsSplitTheStep) {
  FixedSteps clock;
  EXPECT_EQ(clock.subSteps(), FixedSteps::kDefaultSubSteps);
  EXPECT_DOUBLE_EQ(clock.subStepSeconds(), kStep / FixedSteps::kDefaultSubSteps);
  clock.setSubSteps(1);
  EXPECT_DOUBLE_EQ(clock.subStepSeconds(), kStep);
  clock.setSubSteps(4);
  EXPECT_DOUBLE_EQ(clock.subStepSeconds(), kStep / 4);
  clock.setSubSteps(0);
  EXPECT_EQ(clock.subSteps(), 1u);
  clock.setSubSteps(1000);
  EXPECT_EQ(clock.subSteps(), FixedSteps::kMostSubSteps);
}

// How far the time is into the next step, for drawing between steps: 0
// just after a step (or while one came early), up to 1 just before the next.
TEST(FixedStepsTest, StepFractionIsHowFarIntoTheNextStep) {
  FixedSteps clock;
  EXPECT_EQ(clock.stepFraction(), 0);
  EXPECT_EQ(clock.stepsFor(kStep * 0.5), 0u);
  EXPECT_NEAR(clock.stepFraction(), 0.5, 1e-12);
  EXPECT_EQ(clock.stepsFor(kStep * 0.6), 1u);
  EXPECT_NEAR(clock.stepFraction(), 0.1, 1e-12);
  EXPECT_EQ(clock.stepsFor(kStep * 0.7), 1u); // due a quarter step early
  EXPECT_EQ(clock.stepFraction(), 0);
}
