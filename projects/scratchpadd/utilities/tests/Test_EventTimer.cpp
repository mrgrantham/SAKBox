#include <ScratchPadd/EventTimer.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

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
