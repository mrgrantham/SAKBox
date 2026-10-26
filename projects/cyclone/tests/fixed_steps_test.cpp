// The rigid-body demos' physics clock (src/demos/fixed_steps.h)
#include <gtest/gtest.h>

#include <fixed_steps.h>

#include <cmath>

namespace {

constexpr double kStep = FixedSteps::kStepSeconds;

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

TEST(FixedStepsTest, SubStepsSplitTheStep) {
  FixedSteps clock;
  EXPECT_EQ(clock.subSteps(), 1u);
  EXPECT_DOUBLE_EQ(clock.subStepSeconds(), kStep);
  clock.setSubSteps(4);
  EXPECT_DOUBLE_EQ(clock.subStepSeconds(), kStep / 4);
  clock.setSubSteps(0);
  EXPECT_EQ(clock.subSteps(), 1u);
  clock.setSubSteps(1000);
  EXPECT_EQ(clock.subSteps(), FixedSteps::kMostSubSteps);
}
