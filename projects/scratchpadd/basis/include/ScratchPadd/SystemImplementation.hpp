#pragma once
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/System.hpp>
#include <condition_variable>

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
  bool on_{false};

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

  virtual void start() override {
    notifyStarted();
    logger().info("Starting system");
    // Prepare all workers
    std::apply([](auto const &...worker) { (worker->prepareWrapper(), ...); },
               workers_);

    // Start workers on independant threads
    std::apply(
        [](auto const &...worker) { (worker->runIfIndependentThread(), ...); },
        workers_);

    // Start the workers on the main threads
    std::apply(
        [this](auto const &...worker) {
          (worker->startingIfMainThread(), ...);
          while (on_) {
            (worker->runIfMainThread(), ...);
          }
          (worker->finishingIfMainThread(), ...);
        },
        workers_);
    waitForStop();
  }

  virtual void stop() override {
    std::apply([](auto const &...worker) { (worker->stop(), ...); }, workers_);
    std::apply([](auto const &...worker) { (worker->cleanupWrapper(), ...); },
               workers_);
    notifyStopped();
  }

  void waitForStop() {
    std::unique_lock<std::mutex> lk(scratchPaddSystemMutex_);
    logger().info("Waiting For Stop...");
    scratchPaddSystemConditionVariable_.wait(lk, [this] { return !on_; });
  }

  void waitForStart() {
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

  void notifyStopped() {
    on_ = false;
    scratchPaddSystemConditionVariable_.notify_all();
  }
  void notifyStarted() {
    on_ = true;
    scratchPaddSystemConditionVariable_.notify_all();
  }
  void sendWork(ScratchPadd::Base *receiver, Message &message) {
    logger().info("sendWork() to {}", receiver->name());
    std::function<void()> *work =
        new std::function<void()>([=] { receiver->receiveWrapper(message); });
    receiver->push(work);
  }

  // Send to receiver if they are node same as the sender
  void sendIfUnmatched(ScratchPadd::Base *sender, Base *possibleReceiver,
                       Message &message) {
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
