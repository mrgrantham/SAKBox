#include <physics_sweep.h>

#include <gtest/gtest.h>

#include <vector>

namespace {

using physics_sweep::Result;
using physics_sweep::Setting;
using physics_sweep::Sweep;

// A run's Result with these figures over `seconds` whole seconds
Result run(Setting setting, double speed, double stretch, double framesShare = 1, double busy = 0.5,
           int seconds = 5) {
  Result result{.setting = setting};
  result.realSeconds = seconds;
  result.simulatedSeconds = speed * seconds;
  result.steppingSeconds = busy * seconds;
  result.steps = static_cast<int>(result.simulatedSeconds * setting.stepsPerSecond);
  result.framesDrawn = static_cast<int>(framesShare * setting.framesPerSecond * seconds);
  result.slowestSecond = speed;
  result.worstStretch = stretch;
  result.seconds = seconds;
  return result;
}

} // namespace

// Every combination, fewest steps first
TEST(PhysicsSweepTest, SweepsEveryCombination) {
  const std::vector<Setting> settings = physics_sweep::allSettings();
  ASSERT_EQ(settings.size(), 5u * 4u * 2u);
  EXPECT_EQ(settings.front().stepsPerSecond, 10);
  EXPECT_EQ(settings.back().stepsPerSecond, 60);
}

// A run ends once it has simulated its seconds, or after its real seconds
// if it can't keep up; then the next setting runs, and after the last the
// sweep is done
TEST(PhysicsSweepTest, RunsEndBySimulatedOrRealTime) {
  Sweep sweep({{30, 4, 30}, {60, 8, 60}});
  sweep.start();
  ASSERT_TRUE(sweep.running());
  for (int second = 0; second < 4; ++second)
    EXPECT_FALSE(sweep.addSecond(1, 1, 0.5, 30, 30, 0.001));
  EXPECT_TRUE(sweep.addSecond(1, 1, 0.5, 30, 30, 0.002)); // 5 s simulated
  EXPECT_EQ(sweep.current().stepsPerSecond, 60);
  // Simulating a quarter of a second each second: 10 real seconds
  for (int second = 0; second < 9; ++second)
    EXPECT_FALSE(sweep.addSecond(1, 0.25, 1, 15, 60, 0));
  EXPECT_TRUE(sweep.addSecond(1, 0.25, 1, 15, 60, 0));
  EXPECT_FALSE(sweep.running());
  EXPECT_TRUE(sweep.finished());

  ASSERT_EQ(sweep.results().size(), 2u);
  const Result &first = sweep.results()[0];
  EXPECT_DOUBLE_EQ(first.speed(), 1);
  EXPECT_DOUBLE_EQ(first.worstStretch, 0.002);
  EXPECT_DOUBLE_EQ(first.stepMilliseconds(), 1000 * 2.5 / 150);
  EXPECT_DOUBLE_EQ(sweep.results()[1].slowestSecond, 0.25);
}

// The slowest second counts, not the average: a run that falls behind for
// a second (the ragdoll lying awake) doesn't keep up
TEST(PhysicsSweepTest, OneSlowSecondIsNotKeepingUp) {
  Sweep sweep({{30, 4, 30}});
  sweep.start();
  sweep.addSecond(1, 1, 0.5, 30, 30, 0);
  sweep.addSecond(1, 0.5, 1, 15, 30, 0);
  for (int second = 0; second < 4; ++second)
    sweep.addSecond(1, 1, 0.5, 30, 30, 0);
  ASSERT_TRUE(sweep.finished());
  EXPECT_DOUBLE_EQ(sweep.results()[0].slowestSecond, 0.5);
  EXPECT_FALSE(physics_sweep::keepsUp(sweep.results()[0]));
}

// Stopped part way, it isn't finished and keeps what it had
TEST(PhysicsSweepTest, StoppingKeepsTheRunsDone) {
  Sweep sweep({{30, 4, 30}, {60, 8, 60}});
  sweep.start();
  for (int second = 0; second < 5; ++second)
    sweep.addSecond(1, 1, 0.5, 30, 30, 0);
  sweep.stop();
  EXPECT_FALSE(sweep.running());
  EXPECT_FALSE(sweep.finished());
  EXPECT_EQ(sweep.results().size(), 1u);
  EXPECT_FALSE(sweep.addSecond(1, 1, 0.5, 30, 30, 0));
}

// Keeping up with joints held comes first; then more frames a second, then
// less delay; those that don't keep up last, the closest first
TEST(PhysicsSweepTest, RanksKeepingUpThenFramesThenDelay) {
  const std::vector<Result> results = physics_sweep::ranked({
      run({60, 4, 60}, 0.5, 0.001),      // too slow
      run({10, 1, 60}, 1.0, 0.05),       // keeps up, but its joints stretch
      run({15, 4, 30}, 1.0, 0.002),      // keeps up at 30 frames
      run({20, 4, 60}, 1.0, 0.002),      // keeps up at 60 frames, 67 ms behind
      run({30, 2, 60}, 1.0, 0.002),      // keeps up at 60 frames, 50 ms behind
      run({30, 4, 60}, 1.0, 0.002, 0.8), // drops frames
      run({60, 8, 30}, 0.8, 0.0),        // too slow, but closer
  });
  std::vector<int> order;
  for (const Result &result : results)
    order.push_back(result.setting.stepsPerSecond * 100 + result.setting.subSteps * 10 +
                    result.setting.framesPerSecond / 30);
  EXPECT_EQ(order, (std::vector<int>{3022, 2042, 1541, 1012, 3042, 6081, 6042}));
}
