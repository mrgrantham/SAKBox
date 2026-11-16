#pragma once

// A padd's work queue, and the signal that wakes whoever processes it.
//
// Padds don't poll: an idle padd's thread blocks on a WakeSignal and is woken
// when work is pushed (or the padd is stopped). Main-thread padds share one
// loop, so their queues share one WakeSignal, and a push to any of them wakes
// that loop.

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>

namespace ScratchPadd {

// Wakes threads waiting for something to change. A generation counter makes
// it race-free: read generation() before checking for work, then
// waitForChange() with that value. If anything was notified in between, the
// generation has already moved on and the wait returns at once, so a wake-up
// can't be missed.
class WakeSignal {
public:
  // Records that something changed and wakes every waiter.
  void notify() {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      ++generation_;
    }
    condition_.notify_all();
  }

  std::uint64_t generation() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return generation_;
  }

  // Blocks until the generation differs from `seen` or `stopping()` returns
  // true. Whoever makes `stopping()` true must call notify() afterwards.
  template <typename Stopping> void waitForChange(std::uint64_t seen, Stopping stopping) {
    std::unique_lock<std::mutex> lk(mutex_);
    condition_.wait(lk, [&] { return generation_ != seen || stopping(); });
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::uint64_t generation_ = 0;
};

// A first-in, first-out queue of jobs, safe to push to from any thread.
// What a queued job is for, so MessageStats can count each kind.
enum class JobKind {
  Message, // a message for one of the padd's handlers
  Repeat,  // the padd's repeat(), on its timer
  Pushed,  // other work the padd queued for itself (Base::push)
};

class WorkQueue {
public:
  using Clock = std::chrono::steady_clock;
  struct Job {
    std::function<void()> run;
    JobKind kind = JobKind::Pushed;
    // When it was queued, if MessageStats was counting then (for how long
    // it waited); otherwise left at the clock's start
    Clock::time_point queuedAt{};
  };

  explicit WorkQueue(WakeSignal *signal) : signal_(signal) {}

  // The signal notified on every push. Only change it before the queue is in
  // use (the system does this when it sets up main-thread padds).
  void setSignal(WakeSignal *signal) { signal_ = signal; }
  WakeSignal &signal() const { return *signal_; }

  void push(Job job) {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      jobs_.push_back(std::move(job));
    }
    signal_->notify();
  }

  // Removes and returns every queued job, oldest first. The caller runs them
  // without holding the lock, so a job can push more work.
  std::deque<Job> takeAll() {
    std::deque<Job> jobs;
    std::lock_guard<std::mutex> lk(mutex_);
    jobs.swap(jobs_);
    return jobs;
  }

private:
  WakeSignal *signal_;
  std::mutex mutex_;
  std::deque<Job> jobs_;
};

} // namespace ScratchPadd
