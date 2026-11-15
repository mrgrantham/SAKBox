#include <ScratchPadd/EventTimer.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

// Waits up to `timeout` for `condition` to become true.
template <typename Condition>
bool eventually(Condition condition, std::chrono::milliseconds timeout = 2000ms) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!condition()) {
    if (std::chrono::steady_clock::now() > deadline)
      return false;
    std::this_thread::sleep_for(1ms);
  }
  return true;
}

} // namespace

TEST(EventTimerTest, RepeatingEventFiresRepeatedly) {
  ScratchPadd::EventTimer timer;
  std::atomic<int> count{0};
  timer.startRepeatingEvent([&] { ++count; }, 5ms);

  EXPECT_TRUE(timer.isActive());
  EXPECT_TRUE(eventually([&] { return count >= 3; }));
}

// The n-th tick comes at start + n intervals, so the rate is the one asked
// for: a late wake-up or a slow event doesn't push the ticks after it later.
// (Before, each interval was counted from the tick before: on macOS a 2 ms
// repeat ran every 3 ms.)
TEST(EventTimerTest, RepeatsAtTheRateAskedFor) {
  ScratchPadd::EventTimer timer;
  std::atomic<int> count{0};
  const auto start = std::chrono::steady_clock::now();
  timer.startRepeatingEvent([&] { ++count; }, 2ms);
  std::this_thread::sleep_for(500ms);
  timer.stop();
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  // 250 ticks expected; a loaded test machine may skip a few
  EXPECT_GE(count, static_cast<int>(seconds * 500 * 0.9)) << count << " in " << seconds << " s";
  EXPECT_LE(count, static_cast<int>(seconds * 500) + 1);
}

// A slow event: the ticks it ran over are skipped, not run in a burst
TEST(EventTimerTest, SkipsTicksAnEventRanOver) {
  ScratchPadd::EventTimer timer;
  std::atomic<int> count{0};
  std::vector<std::chrono::steady_clock::time_point> times(2);
  timer.startRepeatingEvent(
      [&] {
        const int n = count++;
        if (n < 2)
          times[static_cast<std::size_t>(n)] = std::chrono::steady_clock::now();
        if (n == 0)
          std::this_thread::sleep_for(35ms); // over three 10 ms ticks
      },
      10ms);
  ASSERT_TRUE(eventually([&] { return count >= 2; }));
  timer.stop();
  // The next tick on the timeline after it finished: no sooner than 35 ms
  EXPECT_GE(times[1] - times[0], 35ms);
  EXPECT_LT(times[1] - times[0], 60ms);
}

TEST(EventTimerTest, StopEndsRepeatingEvents) {
  ScratchPadd::EventTimer timer;
  std::atomic<int> count{0};
  timer.startRepeatingEvent([&] { ++count; }, 5ms);
  ASSERT_TRUE(eventually([&] { return count >= 1; }));

  timer.stop();
  EXPECT_FALSE(timer.isActive());

  // stop() joins the timer thread, so no event can run after it returns.
  const int countAfterStop = count;
  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(count, countAfterStop);
}

TEST(EventTimerTest, StopDoesNotWaitForInterval) {
  ScratchPadd::EventTimer timer;
  timer.startRepeatingEvent([] {}, 10s);

  const auto start = std::chrono::steady_clock::now();
  timer.stop();
  EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST(EventTimerTest, DestructorStopsTimer) {
  std::atomic<int> count{0};
  {
    ScratchPadd::EventTimer timer;
    timer.startRepeatingEvent([&] { ++count; }, 1ms);
    ASSERT_TRUE(eventually([&] { return count >= 1; }));
  }
  const int countAfterDestroy = count;
  std::this_thread::sleep_for(20ms);
  EXPECT_EQ(count, countAfterDestroy);
}

TEST(EventTimerTest, TimedEventFiresOnce) {
  ScratchPadd::EventTimer timer;
  std::atomic<int> count{0};
  timer.startTimedEvent([&] { ++count; }, 10);

  EXPECT_TRUE(eventually([&] { return count == 1; }));
  EXPECT_TRUE(eventually([&] { return !timer.isActive(); }));
  EXPECT_EQ(count, 1);
}

TEST(EventTimerTest, StopCancelsPendingTimedEvent) {
  ScratchPadd::EventTimer timer;
  std::atomic<int> count{0};
  timer.startTimedEvent([&] { ++count; }, 50);

  timer.stop();
  std::this_thread::sleep_for(100ms);
  EXPECT_EQ(count, 0);
}

TEST(EventTimerTest, StopFromInsideEventDoesNotDeadlock) {
  ScratchPadd::EventTimer timer;
  std::atomic<int> count{0};
  timer.startRepeatingEvent(
      [&] {
        ++count;
        timer.stop();
      },
      1ms);

  EXPECT_TRUE(eventually([&] { return !timer.isActive(); }));
  EXPECT_EQ(count, 1);
}
