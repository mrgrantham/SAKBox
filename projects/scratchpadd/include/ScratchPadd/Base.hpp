#pragma once

#include <chrono>
#include <memory>
#include <thread>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <spdlog/spdlog.h>

#pragma clang diagnostic ignored "-Wambiguous-reversed-operator"
#include <boost/lockfree/queue.hpp>

#pragma clang diagnostic pop
#include <ScratchPadd/EventTimer.hpp>

#include <ScratchPadd/Helper.hpp>
#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/System.hpp>

namespace ScratchPadd {
using namespace std::chrono_literals;

class Base {
protected:
  bool on_{true};
  std::thread workerThread_;
  int repeating_interval_{0};
  int work_thread_sleep_interval_{500};
  EventTimer repeatingTimer_;
  System *system_;
  boost::lockfree::queue<std::function<void()> *> work_queue_{100};
  std::atomic<ssize_t> pendingWork_{0};
  std::condition_variable workCompletionConditionVariable_;
  std::mutex workCompletionMutex_;

private:
  std::unordered_map<std::string, ControlTypeVariant> controlMap_;

public:
  Base(System *system) : system_(system) {}

  virtual ~Base() {
    // spdlog gets torn down before the destructor
    // need cout in order to know about destructors called
    std::cout << "Base() Destroying: " << __CLASS_NAME__ << std::endl;
  }

  bool isRunning() { return on_; }

  virtual std::unordered_map<std::string, ControlTypeVariant>
  generateControls() = 0;

  // This method takes the controlMap which has references to the actual
  // variables and create a snapshot which is just all the same info but has a
  // copy by value of the control values at that point in time
  ScratchPadd::MessageType::ControlSnapshot generateControlsSnapshot() {
    return ScratchPadd::MessageType::ControlSnapshot{.paddName = name(),
                                                     .controlMap = controlMap_};
  }

  void receive(const MessageType::ControlRequest &request) {
    send(MakeMsg(generateControlsSnapshot()));
  }

  ssize_t pendingWorkCount() { return pendingWork_; }

  void waitForWorkCompletion() {
    std::unique_lock<std::mutex> lk(workCompletionMutex_);
    workCompletionConditionVariable_.wait_for(
        lk, 3000ms, [this] { return pendingWork_ == 0; });
  }

  // Every Padd needs to implement this so their name can be setup for use in
  // the system
  virtual std::string name() = 0;
  virtual bool runOnMainThread() { return false; }
  // Any setup where order of for Padds may be necessary
  // Called from primary thread used by System instance before start
  // Padd thread (if applicable)
  virtual void prepare() {}

  // Any setup where order of for Padds may be necessary
  // Called from primary thread used by System instance after stopping
  // the Padd thread (if applicable)
  virtual void cleanup() {}

  // Any setup that is necessary in the Padd thread should
  // be added by overriding this method
  virtual void starting() {}

  // Any cleanup that is necessary in the Padd thread should
  // be added by overriding this method
  virtual void finishing() {}

  // These wrappers will allow tracking calls and adding metrics
  // as well as handling any eventual additional internal setup
  // associated with Padd lifecycle
  void prepareWrapper() {
    controlMap_ = generateControls();
    prepare();
  }

  void cleanupWrapper() { cleanup(); }

  void startingWrapper() {
    spdlog::info("Starting {}", name());
    starting();
    startRepeater();
  }

  void finishingWrapper() {
    spdlog::info("Finishing {}", name());
    finishing();
  }

  virtual void repeat() {
    spdlog::info("Base::repeat() called. Padd set a repeat interval of {} with "
                 "no method override",
                 repeating_interval_);
  }

  void runIfIndependentThread() {
    if (!runOnMainThread()) {
      spdlog::info("Start Independent Thread: {}", name());
      workerThread_ = std::thread(&Base::run, this);
    }
  }

  void runIfMainThread() {
    if (runOnMainThread()) {
      run_once();
    }
  }

  void startingIfMainThread() {
    if (runOnMainThread()) {
      startingWrapper();
    }
  }

  void finishingIfMainThread() {
    if (runOnMainThread()) {
      finishingWrapper();
    }
  }

  void run() {
    startingWrapper();
    while (on_) {
      loop();
    }
    finishingWrapper();
  }

  void run_once() {
    if (on_) {
      loop();
    }
  }

  void loop() {
    std::function<void()> *work = nullptr;
    while (work_queue_.pop(work) && on_) {
      if (!work)
        spdlog::error("work is null");
      work->operator()();
      pendingWork_--;
      delete work;
    }
    workCompletionConditionVariable_.notify_all();
    // spdlog::warn("sleeping work_queue_ {} for {}ms",name(),
    // work_thread_sleep_interval_);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(work_thread_sleep_interval_));
  }

  void setRepeatInterval(int interval) { repeating_interval_ = interval; }

  // If you want some method to repeat at a regular interval, you
  // can implement the repeat() method and set a repeat interval.
  // A lambda for the repeating work will be added to the work queue
  // and processed immediately.
  void startRepeater() {
    if (repeating_interval_) {
      spdlog::info("repeat() interval set to {} for {}", repeating_interval_,
                   name());
      repeatingTimer_.startRepeatingEvent(
          [=, this] {
            std::function<void()> *work =
                new std::function<void()>([=, this] { this->repeat(); });
            work_queue_.push(work);
          },
          repeating_interval_);
    } else {
      spdlog::info("No repeat() interval set for: {}", name());
    }
  }

  void push(std::function<void()> *work) {
    pendingWork_++;
    work_queue_.push(work);
  }

  virtual void receive(Message message) = 0;

  void stop() {
    spdlog::info("Stopping: {}", name());
    on_ = false;
    repeatingTimer_.stop();
    if (workerThread_.joinable()) {
      workerThread_.join();
    }
  }
  void send(Message &&message) { system_->send(this, message); }

  template <typename UnderlyingMessageContents>
  void send(UnderlyingMessageContents messageContents) {
    system_->send(this, MakeMsg(messageContents));
  }
  void sendIncludeSender(Message &message) {
    system_->sendIncludeSender(message);
  }
};

} // namespace ScratchPadd