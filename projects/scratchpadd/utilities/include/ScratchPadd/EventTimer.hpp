#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace ScratchPadd {

// Runs an event on a background thread, either repeatedly at an interval or
// once after a delay. stop() (also called by the destructor) wakes the thread
// immediately and joins it, so once it returns the event will not run again.
class EventTimer {
  // Both are shared with the timer thread.
  std::atomic<bool> is_active_{false};
  std::atomic<bool> should_stop_{false};
  std::mutex mutex_;
  std::condition_variable stop_condition_variable_;
  std::thread timer_thread_;
  // Set by the timer thread itself, so stop() can tell it is being called from
  // inside the event without touching timer_thread_ from that thread.
  std::atomic<std::thread::id> timer_thread_id_;

  // Sleeps for `duration` unless stop() is called first. Returns true if the
  // full duration elapsed.
  template <typename Duration> bool sleepUnlessStopped(Duration duration) {
    std::unique_lock<std::mutex> lk(mutex_);
    return !stop_condition_variable_.wait_for(lk, duration, [this] { return should_stop_.load(); });
  }

  template <typename Body> void startThread(Body body) {
    stop(); // a timer runs one event at a time
    should_stop_ = false;
    is_active_ = true;
    timer_thread_ = std::thread([this, body] {
      timer_thread_id_ = std::this_thread::get_id();
      body();
      is_active_ = false;
    });
  }

public:
  EventTimer() = default;
  EventTimer(const EventTimer &) = delete;
  EventTimer &operator=(const EventTimer &) = delete;
  ~EventTimer() { stop(); }

  template <typename Event, typename Duration>
  void startRepeatingEvent(const Event &event, Duration interval_between_events) {
    startThread([=, this] {
      while (sleepUnlessStopped(interval_between_events)) {
        event();
      }
    });
  }

  template <typename Event> void startTimedEvent(const Event &event, int delay_until_event) {
    startThread([=, this] {
      if (sleepUnlessStopped(std::chrono::milliseconds(delay_until_event))) {
        event();
      }
    });
  }

  void stop() {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      should_stop_ = true;
    }
    stop_condition_variable_.notify_all();
    // Called from inside the event, the thread can't join itself; it exits
    // after the event returns and is joined by the destructor instead.
    if (timer_thread_id_.load() == std::this_thread::get_id()) {
      return;
    }
    if (timer_thread_.joinable()) {
      timer_thread_.join();
    }
    is_active_ = false;
  }

  bool isActive() { return is_active_; }
};
} // namespace ScratchPadd
