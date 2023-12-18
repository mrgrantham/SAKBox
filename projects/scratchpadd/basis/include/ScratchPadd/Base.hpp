#pragma once

#include <chrono>
#include <memory>
#include <thread>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#pragma clang diagnostic ignored "-Wambiguous-reversed-operator"
#include <boost/lockfree/queue.hpp>

#pragma clang diagnostic pop
#include <ScratchPadd/EventTimer.hpp>

#include <ScratchPadd/Helper.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/System.hpp>

namespace ScratchPadd {
using namespace std::chrono_literals;

class Base : public Logger {
protected:
  bool on_{false};
  std::thread workerThread_;
  std::chrono::duration<double> work_thread_sleep_interval_{500ms};
  EventTimer repeatingTimer_;
  System *system_;
  boost::lockfree::queue<std::function<void()> *> work_queue_{100};
  std::atomic<ssize_t> pendingWork_{0};
  std::condition_variable workCompletionConditionVariable_;
  std::mutex workCompletionMutex_;
  std::condition_variable scratchPaddConditionVariable_;

private:
  std::unordered_map<std::string, ControlTypeVariant> controlMap_;
  std::optional<std::chrono::duration<double>> repeatInterval_{std::nullopt};
  std::string name_{"Unnamed Padd"};
  bool shouldUseMainThread_{false};
  std::mutex scratchPaddMutex_;

public:
  Base(System *system) : system_(system) {}

  virtual ~Base() {
    // spdlog gets torn down before the destructor
    // need cout in order to know about destructors called
    logger().info("Base() Destroying: {}", name_);
  }

  bool isRunning() { return on_; }

  // This method takes the controlMap which has references to the actual
  // variables and create a snapshot which is just all the same info but has a
  // copy by value of the control values at that point in time
  ScratchPadd::MessageType::ControlSnapshot generateControlsSnapshot() {
    return ScratchPadd::MessageType::ControlSnapshot{.paddName = name_,
                                                     .controlMap = controlMap_};
  }

  // void receive(const MessageType::ControlRequest &request) {
  //   send(MakeMsg(generateControlsSnapshot()));
  // }

  ssize_t pendingWorkCount() { return pendingWork_; }

  void waitForWorkCompletion() {
    std::unique_lock<std::mutex> lk(workCompletionMutex_);
    workCompletionConditionVariable_.wait_for(lk, 3000ms, [this] {
      logger().info("waitForWorkCompletion in {}: {} jobs left", name_,
                    pendingWork_);
      return pendingWork_ == 0;
    });
  }

  // Every Padd needs to implement this so their name can be setup for use in
  // the system
  virtual std::string name() = 0;

  // Implementing this to true will execute the Padd work queue in whatever
  // thread the overall system was started in. Overriding this method to return
  // false will run the work queue in its own background thread
  virtual bool shouldUseMainThread() { return false; }

  // Overriding this to return a std::chrono::duration interval will setup
  // the padd to call the repeat() virtual method repeatedly on the interval
  // provided
  virtual std::optional<std::chrono::duration<double>> repeatInterval() {
    return std::nullopt;
  }

  // Overriding this method sets up a series of data structures that can be
  // broadcast to another Padd that is setup to expose them to a user in some
  // way. The values can then be updated here and used to impact logic in the
  // Padd in some as yet unspecified way.
  virtual ScratchPadd::ControlValueMap generateControls() = 0;

  void notifyStart() {
    on_ = true;
    scratchPaddConditionVariable_.notify_all();
  }

  void notifyStop() {
    on_ = false;
    scratchPaddConditionVariable_.notify_all();
  }

  void waitForStop() {
    std::unique_lock<std::mutex> lk(scratchPaddMutex_);
    logger().info("Waiting For {} to Stop...", name_);
    scratchPaddConditionVariable_.wait(lk, [this] { return !on_; });
  }

  void waitForStart() {
    std::unique_lock<std::mutex> lk(scratchPaddMutex_);
    logger().info("Waiting For {} to Start...", name_);
    scratchPaddConditionVariable_.wait(lk, [this] { return on_; });
    logger().info("Wait completed for starting {}...", name_);
  }

  // Initialize all compoments that require virtual methods implemented
  // in child class
  void initialize() {
    name_ = name();
    createLoggerIfNeeded(name_);
    repeatInterval_ = repeatInterval();
    shouldUseMainThread_ = shouldUseMainThread();
    controlMap_ = generateControls();
  }

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
  void prepareWrapper() { prepare(); }

  void cleanupWrapper() { cleanup(); }

  void startingWrapper() {
    logger().info("Starting {}", name_);
    starting();
    startRepeater();
    notifyStart();
  }

  void finishingWrapper() {
    logger().info("Finishing {}", name_);
    finishing();
  }

  virtual void repeat() {
    logger().info(
        "Base::repeat() called. Padd set a repeat interval of {}ms with "
        "no method override",
        repeatInterval_.value().count());
  }

  void runIfIndependentThread() {
    if (!shouldUseMainThread_) {
      logger().info("Start Independent Thread: {}", name_);
      workerThread_ = std::thread(&Base::run, this);
    }
  }

  void runIfMainThread() {
    if (shouldUseMainThread_) {
      run_once();
    }
  }

  void startingIfMainThread() {
    if (shouldUseMainThread_) {
      startingWrapper();
    }
  }

  void finishingIfMainThread() {
    if (shouldUseMainThread_) {
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
        logger().error("work is null");
      work->operator()();
      pendingWork_--;
      delete work;
    }
    workCompletionConditionVariable_.notify_all();
    // logger().warn("sleeping work_queue_ {} for {}ms",name_,
    // work_thread_sleep_interval_);
    std::this_thread::sleep_for(work_thread_sleep_interval_);
  }

  // If you want some method to repeat at a regular interval, you
  // can implement the repeat() method and set a repeat interval.
  // A lambda for the repeating work will be added to the work queue
  // and processed immediately.
  void startRepeater() {
    if (repeatInterval_) {
      logger().info("repeat() interval set to {}ms for {}",
                    repeatInterval_.value().count(), name_);
      repeatingTimer_.startRepeatingEvent(
          [=, this] {
            std::function<void()> *work =
                new std::function<void()>([=, this] { this->repeat(); });
            push(work);
          },
          repeatInterval_.value());
    } else {
      logger().info("No repeat() interval set for: {}", name_);
    }
  }

  void push(std::function<void()> *work) {
    pendingWork_++;
    work_queue_.push(work);
  }

  // virtual void receive(Message message) {
  //     ScratchPadd::MessageVariant &messageVariant = *message.get();
  //   std::visit([](const auto & messageContents){ receive(messageContents); },
  //   messageVariant);
  // }

  bool receive(ScratchPadd::MessageType::ControlChange &controlChange) {
    logger().info("{} Control controlName: {}", this->name(),
                  controlChange.controlName);
    if (controlChange.paddName == name()) {
      updateControl(controlChange);
    }
    send(generateControlsSnapshot());
    return true;
  }

  bool receive(ScratchPadd::MessageType::ControlRequest &controlRequest) {
    logger().info("{} Control request padd name: {}", name(),
                  controlRequest.paddName.value_or("None"));
    send(generateControlsSnapshot());
    return true;
  }

  virtual void handleControlSnapshot(
      ScratchPadd::MessageType::ControlSnapshot &controlSnapshot) {
    logger().error("handleControlSnapshot not implemented");
  }

  bool receive(ScratchPadd::MessageType::ControlSnapshot &controlSnapshot) {
    handleControlSnapshot(controlSnapshot);
    return true;
  }

  bool receive(auto &&message) {
    logger().error("no inner receive for this message type");
    return false;
  }

  virtual void receive(Message message) = 0;

  bool innerReceive(Message message) {
    logger().info("inside innerReceive start");
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    return std::visit(
        [&](auto &&messageContents) -> bool {
          logger().info("inside innerReceive std::visit");
          return receive(messageContents);
        },
        messageVariant);
    logger().info("inside innerReceive end");
  }

  void receiveWrapper(Message message) {
    logger().info("inside receiveWrapper start");
    bool innerReceiverUsed = innerReceive(message);
    if (!innerReceiverUsed) {
      logger().info("Trying to use outer message");
      receive(message);
    }
  }

  // void f(const C & c) { std::visit([](const auto & x){ f(x); }, c); }

  void stop() {
    logger().info("Stopping: {}", name_);
    repeatingTimer_.stop(); // might need to flush the queue
    notifyStop();
    if (workerThread_.joinable()) {
      workerThread_.join();
    }
  }

  // Sends message to be consumed by all other Padds except
  // the one that sent it
  void send(Message &&message) { system_->send(this, message); }

  // Convenience method to package the message contents in
  // a shared pointer by default
  template <typename UnderlyingMessageContents>
  void send(UnderlyingMessageContents messageContents) {
    logger().info("send({}) from {}", TypeName(messageContents), name_);
    system_->send(this, MakeMsg(messageContents));
  }

  // Sends message to be consumed by all Padds including
  // the one that sent it
  void sendIncludeSender(Message &message) {
    system_->sendIncludeSender(message);
  }

  // Convenience method to package the message contents in
  // a shared pointer by default
  template <typename UnderlyingMessageContents>
  void sendIncludeSender(UnderlyingMessageContents messageContents) {
    system_->sendIncludeSender(this, MakeMsg(messageContents));
  }

  void updateControl(ScratchPadd::MessageType::ControlChange &controlChange) {
    if (controlMap_.contains(controlChange.controlName)) {
      auto &controlTypeVariant = controlMap_.at(controlChange.controlName);
      if (VariantsHoldSameType(controlChange.controlTypeValue,
                               controlTypeVariant)) {
        controlTypeVariant = controlChange.controlTypeValue;
      } else {
        logger().error("In {}, ControlChange holds different value type than "
                       "ControlMap for {}",
                       name_, controlChange.controlName);
      }
    } else {
      logger().error("No contol named {}", controlChange.controlName);
    }
  }
};

} // namespace ScratchPadd