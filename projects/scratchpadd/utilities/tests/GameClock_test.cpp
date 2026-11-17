// The game clock: running with real time, stopped while paused, at another
// rate when asked, never jumping when paused or its rate changes
#include <ScratchPadd/GameClock.hpp>

#include <gtest/gtest.h>

using ScratchPadd::GameClock;
using namespace std::chrono_literals;

TEST(GameClockTest, RunsPausesAndChangesRate) {
  const auto start = GameClock::RealClock::time_point{} + 100s;
  GameClock clock(start);
  EXPECT_DOUBLE_EQ(clock.seconds(start + 2s), 2);
  clock.setPaused(true, start + 2s);
  EXPECT_DOUBLE_EQ(clock.seconds(start + 10s), 2); // (stopped)
  clock.setPaused(false, start + 10s);
  EXPECT_DOUBLE_EQ(clock.seconds(start + 11s), 3); // (going on from where it stopped)
  clock.setRate(2, start + 11s);
  EXPECT_DOUBLE_EQ(clock.seconds(start + 11s), 3); // (no jump)
  EXPECT_DOUBLE_EQ(clock.seconds(start + 12s), 5);
  clock.setPaused(true, start + 12s);
  clock.setRate(0.5, start + 13s); // (while paused: still stopped)
  EXPECT_DOUBLE_EQ(clock.seconds(start + 20s), 5);
  clock.setPaused(false, start + 20s);
  EXPECT_DOUBLE_EQ(clock.seconds(start + 22s), 6);
  clock.setPaused(false, start + 22s); // (already running: nothing changes)
  EXPECT_DOUBLE_EQ(clock.seconds(start + 22s), 6);
}
