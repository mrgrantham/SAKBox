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
#include <ScratchPadd/System.hpp>
#include <ScratchPadd/Messages/Message.hpp>

namespace ScratchPadd {

class Base {
protected:
  bool on_{true};
  std::string paddName_;
  std::thread workerThread_;
  int repeating_interval_{0};
  int work_thread_sleep_interval_{500};
  EventTimer repeatingTimer_;
  System *system_;
  boost::lockfree::queue<std::function<void()> *> work_queue_{100};

private:
  std::unordered_map<std::string, ControlTypeVariant> controlMap_;

public:
  Base(System *system) : system_(system) { paddName_ = __CLASS_NAME__; 
  }

  virtual ~Base() {
    // spdlog gets torn down before the destructor
    // need cout in order to know about destructors called
    // spdlog::info("Destroying: {}", __CLASS_NAME__);
    std::cout << "Base() Destroying: " << paddName_ << std::endl;
  }

  bool isRunning() { return on_; }

  void start() {
    spdlog::info("Start: {}", paddName_);
    controlMap_ = generateControls();
    if (!runOnMainThread()) {
      workerThread_ = std::thread(&Base::run, this);
    }
    startRepeater();
  }

  std::string getName() { return paddName_; }

  virtual std::unordered_map<std::string, ControlTypeVariant> generateControls() = 0;

  // This method takes the controlMap which has references to the actual variables
  // and create a snapshot which is just all the same info but has a copy by value
  // of the control values at that point in time
  ScratchPadd::MessageType::ControlSnapshot generateControlsSnapshot() {
    return  ScratchPadd::MessageType::ControlSnapshot{.paddName = paddName_, .controlMap = controlMap_};
  }

  void receive(const MessageType::ControlRequest &request) {
      send(MakeMsg(generateControlsSnapshot()));
  }

  virtual bool runOnMainThread() { return false; }
  virtual void config() {}
  virtual void prepare() {}
  virtual void cleanup() {}
  virtual void starting() {}
  virtual void finishing() {}
  virtual void repeat() {
    spdlog::info("Base::repeat() called. Padd set a repeat interval of {} with "
                 "no method override",
                 repeating_interval_);
  }

  void runIfMainThread() {
    if (runOnMainThread()) {
      run_once();
    }
  }

  void startingIfMainThread() {
    if (runOnMainThread()) {
      starting();
    }
  }

  void finishingIfMainThread() {
    if (runOnMainThread()) {
      finishing();
    }
  }

  void run() {
    starting();
    while (on_) {
      loop();
    }
    finishing();
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
      delete work;
    }
    // spdlog::warn("sleeping work_queue_ {} for {}ms",paddName_,
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
      spdlog::info("repeat() interval set to {}", repeating_interval_);
      repeatingTimer_.startRepeatingEvent(
          [=, this] {
            std::function<void()> *work =
                new std::function<void()>([=, this] { this->repeat(); });
            work_queue_.push(work);
          },
          repeating_interval_);
    } else {
      spdlog::info("No repeat() interval set");
    }
  }

  void push(std::function<void()> *work) { work_queue_.push(work); }

  virtual void receive(Message message) = 0;

  void stop() {
    spdlog::info("Stopping: {}", paddName_);
    on_ = false;
    repeatingTimer_.stop();
    if (workerThread_.joinable()) {
      workerThread_.join();
    }
  }
  void send(Message &&message) { system_->send(this, message); }

  template <typename UnderlyingMessageContents>
  void send(UnderlyingMessageContents messageContents) { system_->send(this, MakeMsg(messageContents)); }
  void sendIncludeSender(Message &message) {
    system_->sendIncludeSender(message);
  }
};

} // namespace ScratchPadd