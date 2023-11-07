#pragma once
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/System.hpp>
#include <condition_variable>
#include <spdlog/spdlog.h>
#include <tuple>

namespace ScratchPadd {

// NOTE: When using new clang upgrade to use
// std::enable_shared_from_this<System<Workers...>> instead of passing the
// sender as a raw pointer
template <typename... Workers> class SystemImplementation : public System {
private:
  std::tuple<std::unique_ptr<Workers>...> workers_;
  std::condition_variable scratchPaddSystemConditionVariable_;
  std::mutex scratchPaddSystemMutex_;
  bool on_{false};

public:
  const std::tuple<std::unique_ptr<Workers>...> &getWorkers() const {
    return workers_;
  }

  virtual void instantiate() override {
    ((std::get<std::unique_ptr<Workers>>(workers_) =
          std::make_unique<Workers>(this)),
     ...);
    std::apply([](auto const &...worker) { (worker->initialize(), ...); },
               workers_);
  }

  virtual void start() override {
    notifyStarted();
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
    spdlog::info("Waiting For Stop...");
    scratchPaddSystemConditionVariable_.wait(lk, [this] { return !on_; });
  }

  void waitForStart() {
    std::unique_lock<std::mutex> lk(scratchPaddSystemMutex_);
    spdlog::info("Waiting For Start...");
    scratchPaddSystemConditionVariable_.wait(lk, [this] { return on_; });
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
  void sendAnyway(ScratchPadd::Base *possibleReceiver, Message &message) {
    std::function<void()> *work =
        new std::function<void()>([=] { possibleReceiver->receive(message); });
    possibleReceiver->push(work);
  }
  void sendIfUnmatched(ScratchPadd::Base *sender, Base *possibleReceiver,
                       Message &message) {
    if (sender != possibleReceiver) {
      sendAnyway(possibleReceiver, message);
    }
  }
  virtual void send(ScratchPadd::Base *sender, Message message) override {
    spdlog::info("sending from {}", sender->name());
    (sendIfUnmatched(sender, std::get<std::unique_ptr<Workers>>(workers_).get(),
                     message),
     ...);
  }
  virtual void sendIncludeSender(ScratchPadd::Message message) override {
    (sendAnyway(std::get<std::unique_ptr<Workers>>(workers_).get(), message),
     ...);
  }
};

template <typename... Simp> static System *SystemBuilder() {
  return new SystemImplementation<Simp...>();
}
} // namespace ScratchPadd
