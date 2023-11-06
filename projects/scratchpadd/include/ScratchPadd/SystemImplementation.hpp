#pragma once
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
  bool stopped_{false};
  bool on_{false};

public:

  const std::tuple<std::unique_ptr<Workers>...>& getWorkers() const {
    return workers_;
  }

  virtual void instantiate() override {
    ((std::get<std::unique_ptr<Workers>>(workers_) =
          std::make_unique<Workers>(this)),
     ...);
  }

  virtual void start() override {
    on_ = true;
    // Start workers on independant threads
    std::apply(
        [](auto const &...worker) {
          (worker->prepare(), ...);
          (worker->start(), ...);
        },
        workers_);

    // Start the workers on the main threads

    std::apply(
        [this](auto const &...worker) {
          (worker->startingIfMainThread(), ...);
          while (!stopped_) {
            (worker->runIfMainThread(), ...);
          }
          (worker->finishingIfMainThread(), ...);
        },
        workers_);
    waitForStop();
  }

  virtual void stop() override {
    std::apply([](auto const &...worker) { (worker->stop(), ...); }, workers_);
    notifyStopped();
    on_ = false;
  }
  void waitForStop() {
    std::unique_lock<std::mutex> lk(scratchPaddSystemMutex_);
    spdlog::info("Waiting For Stop...");
    scratchPaddSystemConditionVariable_.wait(lk, [this] { return stopped_; });
  }

  virtual bool isRunning() override { return on_; }

  void notifyStopped() {
    stopped_ = true;
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
    std::cout << "sending" << std::endl;
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
