#pragma once
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/System.hpp>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <unordered_map>

namespace ScratchPadd {

// NOTE: When using new clang upgrade to use
// std::enable_shared_from_this<System<Workers...>> instead of passing the
// sender as a raw pointer
template <typename... Workers> class SystemImplementation : public System, public Logger {

private:
  std::tuple<std::unique_ptr<Workers>...> workers_;
  std::condition_variable scratchPaddSystemConditionVariable_;
  std::mutex scratchPaddSystemMutex_;
  // Read by the main-thread run loop and written by stop() from any thread.
  std::atomic<bool> on_{false};
  // Set by stop(); start() then shuts the system down on its own thread.
  std::atomic<bool> stopRequested_{false};
  // Shared by the main-thread padds' queues: a push to any of them, or
  // stop(), wakes the main-thread loop.
  WakeSignal mainThreadSignal_;
  // Each padd by its class and by its name, for sending to one padd. Built
  // by instantiate() and never changed after, so any thread may read them.
  std::unordered_map<std::type_index, Base *> paddsByType_;
  std::map<std::string, Base *, std::less<>> paddsByName_;

  // The main-thread padds share this loop: each pass runs every one's queued
  // work in list order, then sleeps until work is pushed to any of them (their
  // queues all notify mainThreadSignal_) or stop() is called. With no
  // main-thread padds it just waits for stop().
  void runMainThreadPadds() {
    const auto stopping = [this] { return stopRequested_.load(); };
    const bool anyMainThreadPadds = std::apply(
        [](auto const &...worker) { return (worker->usesMainThread() || ...); }, workers_);
    if (!anyMainThreadPadds) {
      while (!stopping()) {
        mainThreadSignal_.waitForChange(mainThreadSignal_.generation(), stopping);
      }
      return;
    }

    std::apply([](auto const &...worker) { (worker->startingIfMainThread(), ...); }, workers_);
    while (!stopping()) {
      // Read before draining, so work pushed meanwhile (often by these same
      // padds) means the wait below returns at once.
      const std::uint64_t seen = mainThreadSignal_.generation();
      std::apply([](auto const &...worker) { (worker->runIfMainThread(), ...); }, workers_);
      mainThreadSignal_.waitForChange(seen, stopping);
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
    std::apply([](auto const &...worker) { (worker->finishingIfMainThread(), ...); }, workers_);
    std::apply([](auto const &...worker) { (worker->cleanupWrapper(), ...); }, workers_);
    notifyStopped();
  }

public:
  const std::tuple<std::unique_ptr<Workers>...> &getWorkers() const { return workers_; }

  // Check that all workers are implemented with unique strings used for their
  // names, as these are used to send messages to specific padds or avoid
  // others.
  bool workerNamesUnique() {
    std::vector<std::string> workerNamesVector;
    std::apply(
        [&](auto const &...worker) { (workerNamesVector.push_back(worker->paddName()), ...); },
        workers_);

    std::set<std::string> workerNamesSet(workerNamesVector.begin(), workerNamesVector.end());
    return workerNamesSet.size() == workerNamesVector.size();
  }

  void instantiate() override {
    createLoggerIfNeeded("scratchpadd");
    ((std::get<std::unique_ptr<Workers>>(workers_) = std::make_unique<Workers>(this)), ...);
    std::apply([](auto const &...worker) { (worker->initialize(), ...); }, workers_);
    assert(workerNamesUnique());
    (paddsByType_.emplace(std::type_index(typeid(Workers)),
                          std::get<std::unique_ptr<Workers>>(workers_).get()),
     ...);
    std::apply(
        [this](auto const &...worker) {
          (paddsByName_.emplace(worker->paddName(), worker.get()), ...);
        },
        workers_);
    // Main-thread padds share one loop, so they share its wake-up signal.
    std::apply(
        [this](auto const &...worker) {
          ((worker->usesMainThread() ? worker->useWakeSignal(mainThreadSignal_) : void()), ...);
        },
        workers_);
  }

  // Runs the system on the calling thread until stop() is called, then shuts
  // it down on this thread: every padd is stopped (background padds run
  // finishing() on their own threads), then finishing() runs for main-thread
  // padds, then cleanup() for every padd.
  void start() override {
    logger().info("Starting system");
    // Prepare all workers
    std::apply([](auto const &...worker) { (worker->prepareWrapper(), ...); }, workers_);

    // Start workers on independant threads
    std::apply([](auto const &...worker) { (worker->runIfIndependentThread(), ...); }, workers_);

    // Only report started once every padd thread exists, so a stop() from
    // another thread after waitForStart() sees them all.
    notifyStarted();

    runMainThreadPadds();
    shutdown();
  }

  // Asks start() to shut the system down and returns immediately; start()
  // returns once every padd has finished. Safe to call from any thread,
  // including from inside a padd.
  void stop() override {
    stopRequested_ = true;
    mainThreadSignal_.notify(); // wakes the main-thread loop to shut down
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
    std::apply([](auto const &...worker) { (worker->waitForStart(), ...); }, workers_);
  }

  // For testing only. We assume that the work will settle
  // on its own.
  // TODO: Update to ensure that all lambda's have been completed
  void waitForWorkCompletion() {
    int pendingWorkCount = 0;
    do {
      pendingWorkCount = std::apply(
          [](auto const &...worker) { return (worker->pendingWorkCount() + ...); }, workers_);
      // logger().info("pendingWorkCount: {}",pendingWorkCount);
    } while (pendingWorkCount);
  }

  bool isRunning() override { return on_; }

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
  // Queues `message` (from `sender`) for `receiver` if it has a handler for
  // the message's type; padds that don't handle it never see it.
  void sendWork(Base *sender, Base *receiver, const Message &message) {
    if (receiver->enqueue(message, PaddAddress(sender))) {
      logger().debug("sendWork() to {}", receiver->paddName());
    }
  }

  // Send to receiver if they are not the same as the sender
  void sendIfUnmatched(Base *sender, Base *possibleReceiver, const Message &message) {
    if (sender != possibleReceiver) {
      sendWork(sender, possibleReceiver, message);
    }
  }
  void send(Base *sender, Message message) override {
    logger().debug("sending from {}", sender->paddName());
    (sendIfUnmatched(sender, std::get<std::unique_ptr<Workers>>(workers_).get(), message), ...);
  }
  void sendIncludeSender(Base *sender, Message message) override {
    (sendWork(sender, std::get<std::unique_ptr<Workers>>(workers_).get(), message), ...);
  }

  PaddAddress paddOfType(std::type_index type) override {
    auto found = paddsByType_.find(type);
    return found == paddsByType_.end() ? PaddAddress() : PaddAddress(found->second);
  }

  PaddAddress paddNamed(std::string_view name) override {
    auto found = paddsByName_.find(name);
    return found == paddsByName_.end() ? PaddAddress() : PaddAddress(found->second);
  }
};

template <typename... Simp> static System *SystemBuilder() {
  return new SystemImplementation<Simp...>();
}
} // namespace ScratchPadd
