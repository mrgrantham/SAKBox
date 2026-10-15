#pragma once
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/System.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include <set>
#include <tuple>

namespace ScratchPadd {

// NOTE: When using new clang upgrade to use
// std::enable_shared_from_this<System<Workers...>> instead of passing the
// sender as a raw pointer
template <typename... Workers>
class SystemImplementation : public System, public Logger {

private:
  std::tuple<std::unique_ptr<Workers>...> workers_;
  std::condition_variable scratchPaddSystemConditionVariable_;
  std::mutex scratchPaddSystemMutex_;
  // Read by the main-thread run loop and written by stop() from any thread.
  std::atomic<bool> on_{false};
  // Set by stop(); start() then shuts the system down on its own thread.
  std::atomic<bool> stopRequested_{false};

  // Shortest work_thread_sleep_interval_ among the main-thread padds.
  std::chrono::duration<double> mainThreadSleepInterval() {
    auto shortest = std::chrono::duration<double>::max();
    std::apply(
        [&](auto const &...worker) {
          (
              [&] {
                if (worker->usesMainThread())
                  shortest = std::min(shortest, worker->workSleepInterval());
              }(),
              ...);
        },
        workers_);
    return shortest;
  }

  // Sleeps for `duration`, waking early if stop() is called.
  void sleepUnlessStopRequested(std::chrono::duration<double> duration) {
    std::unique_lock<std::mutex> lk(scratchPaddSystemMutex_);
    scratchPaddSystemConditionVariable_.wait_for(
        lk, duration, [this] { return stopRequested_.load(); });
  }

  // The main-thread padds share this loop: each pass runs every one's queued
  // work, then sleeps once for the shortest of their sleep intervals. With no
  // main-thread padds it just waits for stop().
  void runMainThreadPadds() {
    const bool anyMainThreadPadds = std::apply(
        [](auto const &...worker) { return (worker->usesMainThread() || ...); },
        workers_);
    if (!anyMainThreadPadds) {
      std::unique_lock<std::mutex> lk(scratchPaddSystemMutex_);
      scratchPaddSystemConditionVariable_.wait(
          lk, [this] { return stopRequested_.load(); });
      return;
    }

    std::apply(
        [](auto const &...worker) { (worker->startingIfMainThread(), ...); },
        workers_);
    while (!stopRequested_) {
      std::apply(
          [](auto const &...worker) { (worker->runIfMainThread(), ...); },
          workers_);
      sleepUnlessStopRequested(mainThreadSleepInterval());
    }
  }

  // Runs on start()'s thread once stop() is requested: stops every padd
  // (joining background threads after their finishing()), then runs
  // finishing() for main-thread padds and cleanup() for every padd, and
  // finally marks the system stopped so waitForStop() returns.
  void shutdown() {
    // Ends each padd's repeat timer and work loop. A background padd runs
    // finishing() on its own thread before stop() joins it.
    std::apply([](auto const &...worker) { (worker->stop(), ...); }, workers_);
    std::apply(
        [](auto const &...worker) { (worker->finishingIfMainThread(), ...); },
        workers_);
    std::apply([](auto const &...worker) { (worker->cleanupWrapper(), ...); },
               workers_);
    notifyStopped();
  }

public:
  const std::tuple<std::unique_ptr<Workers>...> &getWorkers() const {
    return workers_;
  }

  // Check that all workers are implemented with unique strings used for their
  // names, as these are used to send messages to specific padds or avoid
  // others.
  bool workerNamesUnique() {
    std::vector<std::string> workerNamesVector;
    std::apply(
        [&](auto const &...worker) {
          (workerNamesVector.push_back(worker->name()), ...);
        },
        workers_);

    std::set<std::string> workerNamesSet(workerNamesVector.begin(),
                                         workerNamesVector.end());
    return workerNamesSet.size() == workerNamesVector.size();
  }

  virtual void instantiate() override {
    createLoggerIfNeeded("scratchpadd");
    ((std::get<std::unique_ptr<Workers>>(workers_) =
          std::make_unique<Workers>(this)),
     ...);
    std::apply([](auto const &...worker) { (worker->initialize(), ...); },
               workers_);
    assert(workerNamesUnique());
  }

  // Runs the system on the calling thread until stop() is called, then shuts
  // it down on this thread: every padd is stopped (background padds run
  // finishing() on their own threads), then finishing() runs for main-thread
  // padds, then cleanup() for every padd.
  virtual void start() override {
    logger().info("Starting system");
    // Prepare all workers
    std::apply([](auto const &...worker) { (worker->prepareWrapper(), ...); },
               workers_);

    // Start workers on independant threads
    std::apply(
        [](auto const &...worker) { (worker->runIfIndependentThread(), ...); },
        workers_);

    // Only report started once every padd thread exists, so a stop() from
    // another thread after waitForStart() sees them all.
    notifyStarted();

    runMainThreadPadds();
    shutdown();
  }

  // Asks start() to shut the system down and returns immediately; start()
  // returns once every padd has finished. Safe to call from any thread,
  // including from inside a padd.
  virtual void stop() override {
    {
      std::lock_guard<std::mutex> lk(scratchPaddSystemMutex_);
      stopRequested_ = true;
    }
    scratchPaddSystemConditionVariable_.notify_all();
  }

  // Waits until start() has finished shutting the system down.
  void waitForStop() {
    std::unique_lock<std::mutex> lk(scratchPaddSystemMutex_);
    logger().info("Waiting For Stop...");
    scratchPaddSystemConditionVariable_.wait(lk, [this] { return !on_; });
  }

  // Waits until start() has launched every padd and each one is running.
  void waitForStart() {
    {
      std::unique_lock<std::mutex> lk(scratchPaddSystemMutex_);
      scratchPaddSystemConditionVariable_.wait(lk, [this] { return on_.load(); });
    }
    std::apply([](auto const &...worker) { (worker->waitForStart(), ...); },
               workers_);
  }

  // For testing only. We assume that the work will settle
  // on its own.
  // TODO: Update to ensure that all lambda's have been completed
  void waitForWorkCompletion() {
    int pendingWorkCount = 0;
    do {
      pendingWorkCount = std::apply(
          [](auto const &...worker) {
            return (worker->pendingWorkCount() + ...);
          },
          workers_);
      // logger().info("pendingWorkCount: {}",pendingWorkCount);
    } while (pendingWorkCount);
  }

  virtual bool isRunning() override { return on_; }

  // on_ is changed while holding scratchPaddSystemMutex_ so waitForStop()
  // can't check it and then miss the notify.
  void notifyStopped() {
    {
      std::lock_guard<std::mutex> lk(scratchPaddSystemMutex_);
      on_ = false;
    }
    scratchPaddSystemConditionVariable_.notify_all();
  }
  void notifyStarted() {
    {
      std::lock_guard<std::mutex> lk(scratchPaddSystemMutex_);
      on_ = true;
    }
    scratchPaddSystemConditionVariable_.notify_all();
  }
  // Queues `message` for `receiver` if it has a handler for the message's
  // type; padds that don't handle it never see it.
  void sendWork(ScratchPadd::Base *receiver, const Message &message) {
    if (!receiver->handles(message->type())) {
      return;
    }
    logger().info("sendWork() to {}", receiver->name());
    std::function<void()> *work =
        new std::function<void()>([=] { receiver->deliver(message); });
    receiver->push(work);
  }

  // Send to receiver if they are not the same as the sender
  void sendIfUnmatched(ScratchPadd::Base *sender, Base *possibleReceiver,
                       const Message &message) {
    if (sender != possibleReceiver) {
      sendWork(possibleReceiver, message);
    }
  }
  virtual void send(ScratchPadd::Base *sender, Message message) override {
    logger().info("sending from {}", sender->name());
    (sendIfUnmatched(sender, std::get<std::unique_ptr<Workers>>(workers_).get(),
                     message),
     ...);
  }
  virtual void sendIncludeSender(ScratchPadd::Message message) override {
    (sendWork(std::get<std::unique_ptr<Workers>>(workers_).get(), message),
     ...);
  }
};

template <typename... Simp> static System *SystemBuilder() {
  return new SystemImplementation<Simp...>();
}
} // namespace ScratchPadd
