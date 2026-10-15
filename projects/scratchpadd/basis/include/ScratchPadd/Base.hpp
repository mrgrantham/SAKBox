#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
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
  // Read by the work loop and written from other threads (e.g. stop()).
  std::atomic<bool> on_{false};
  std::thread workerThread_;
  // How long the work loop sleeps once its queue is empty, which is also how
  // long a new message can wait to be picked up. Padds that need lower
  // latency can shorten it in their constructor or prepare().
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
  // Message handlers by message type. Only changed before the padd starts.
  std::unordered_map<std::type_index, std::function<void(const MessageBase &)>> handlers_;

public:
  Base(System *system) : system_(system) {
    // Every padd with controls answers control requests (all of them, or
    // those naming it) with a snapshot.
    receive<MessageType::ControlRequest>([this](const MessageType::ControlRequest &request) {
      if (controlMap_.empty() || (request.paddName && *request.paddName != name_)) {
        return;
      }
      send(generateControlsSnapshot());
    });
    // A change names the padd it's for; every other padd ignores it. The owner
    // applies it (if valid) and always replies with a snapshot, so a panel
    // shows the value the owner actually holds, e.g. after clamping.
    receive<MessageType::ControlChange>([this](const MessageType::ControlChange &change) {
      if (change.paddName != name_) {
        return;
      }
      logger().info("{} control change: {}", name_, change.controlName);
      if (updateControl(change)) {
        controlChanged(change.controlName);
      }
      send(generateControlsSnapshot());
    });
  }

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

  // This padd's control `name`, as control type C (e.g.
  // control<ControlType::Double>("speed").value). Throws std::out_of_range for
  // an unknown name and std::bad_variant_access for the wrong type. Call from
  // the padd's own thread, where changes are applied.
  template <typename C> const C &control(const std::string &name) const {
    return std::get<C>(controlMap_.at(name));
  }

  // Called on the padd's thread after a ControlChange to control `name` has
  // been applied. Override to react to edits from a control panel.
  virtual void controlChanged(const std::string &name) {}

  ssize_t pendingWorkCount() { return pendingWork_; }

  void waitForWorkCompletion() {
    std::unique_lock<std::mutex> lk(workCompletionMutex_);
    workCompletionConditionVariable_.wait_for(lk, 3000ms, [this] {
      logger().info("waitForWorkCompletion in {}: {} jobs left", name_,
                    pendingWork_.load());
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

  // on_ is changed while holding scratchPaddMutex_ so a thread in
  // waitForStart()/waitForStop() can't check it and then miss the notify.
  void notifyStart() {
    {
      std::lock_guard<std::mutex> lk(scratchPaddMutex_);
      on_ = true;
    }
    scratchPaddConditionVariable_.notify_all();
  }

  void notifyStop() {
    {
      std::lock_guard<std::mutex> lk(scratchPaddMutex_);
      on_ = false;
    }
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
    scratchPaddConditionVariable_.wait(lk, [this] { return on_.load(); });
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

  // shouldUseMainThread() as read by initialize(); the system uses this to
  // decide which padds run in its main-thread loop.
  bool usesMainThread() const { return shouldUseMainThread_; }

  // How long the work loop sleeps when the queue is empty (see
  // work_thread_sleep_interval_). The main-thread loop sleeps for the
  // shortest of these among its padds.
  std::chrono::duration<double> workSleepInterval() const {
    return work_thread_sleep_interval_;
  }

  // Called by the system's main-thread loop, which does the sleeping.
  void runIfMainThread() {
    if (shouldUseMainThread_ && on_) {
      processWork();
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

  // The work loop of a padd with its own thread.
  void run() {
    startingWrapper();
    while (on_) {
      processWork();
      std::this_thread::sleep_for(work_thread_sleep_interval_);
    }
    finishingWrapper();
  }

  // Runs every queued job, stopping early if the padd is stopped. on_ is
  // checked before popping, so a job is never popped and then dropped.
  void processWork() {
    std::function<void()> *work = nullptr;
    while (on_ && work_queue_.pop(work)) {
      if (work) {
        (*work)();
        delete work;
      } else {
        logger().error("work is null");
      }
      pendingWork_--;
    }
    workCompletionConditionVariable_.notify_all();
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

  // Registers `handler` for messages of type T, replacing any earlier
  // handler for T. The handler runs on this padd's thread and gets the
  // contents as a const reference; the message is shared with other padds.
  //
  // Call from the constructor or prepare(): handlers can't change once the
  // padd is running, which lets its threads read them without locking. The
  // system only queues a message for padds that have a handler for its type.
  template <typename T, typename Handler> void receive(Handler handler) {
    assert(!on_ && "register message handlers before the padd starts");
    handlers_[std::type_index(typeid(T))] =
        [handler = std::move(handler)](const MessageBase &message) {
          handler(static_cast<const MessageOf<T> &>(message).value);
        };
  }

  // Whether this padd has a handler for messages of `type`.
  bool handles(std::type_index type) const { return handlers_.contains(type); }

  // Runs this padd's handler for `message`. Called on the padd's thread by
  // the work queue.
  void deliver(const Message &message) {
    auto handler = handlers_.find(message->type());
    if (handler != handlers_.end()) {
      handler->second(*message);
    }
  }

  void stop() {
    logger().info("Stopping: {}", name_);
    repeatingTimer_.stop(); // might need to flush the queue
    notifyStop();
    if (workerThread_.joinable()) {
      workerThread_.join();
    }
  }

  // Sends `message` to every other padd that handles its type.
  void send(Message message) {
    logger().info("send({}) from {}", message->typeName(), name_);
    system_->send(this, std::move(message));
  }

  // Sends `contents` (any copyable struct) to every other padd that handles
  // its type.
  template <typename T> void send(T contents) { send(makeMessage(std::move(contents))); }

  // Sends `message` to every padd that handles its type, including this one.
  void sendIncludeSender(Message message) { system_->sendIncludeSender(std::move(message)); }

  // Sends `contents` to every padd that handles its type, including this one.
  template <typename T> void sendIncludeSender(T contents) {
    sendIncludeSender(makeMessage(std::move(contents)));
  }

  // Applies the value in `controlChange` to this padd's control, enforcing
  // the control's own rules: numbers are clamped to its range, strings must
  // be one of its allowed values, and the type must match. Only the value is
  // copied, so a change can't alter a control's range. Returns true if the
  // change was applied.
  bool updateControl(const ScratchPadd::MessageType::ControlChange &controlChange) {
    auto control = controlMap_.find(controlChange.controlName);
    if (control == controlMap_.end()) {
      logger().error("No control named {}", controlChange.controlName);
      return false;
    }
    if (!VariantsHoldSameType(controlChange.controlTypeValue, control->second)) {
      logger().error("In {}, ControlChange holds different value type than "
                     "ControlMap for {}",
                     name_, controlChange.controlName);
      return false;
    }
    const auto &incoming = controlChange.controlTypeValue;
    return std::visit(
        VariantHandler{
            [&](ControlType::Double &current) {
              double value = std::get<ControlType::Double>(incoming).value;
              if (std::isnan(value)) {
                return false;
              }
              if (current.enforcableRange) {
                value = std::clamp(value, current.enforcableRange->first,
                                   current.enforcableRange->second);
              }
              current.value = value;
              return true;
            },
            [&](ControlType::Integer &current) {
              int32_t value = std::get<ControlType::Integer>(incoming).value;
              if (current.enforcableRange) {
                value = std::clamp(value, current.enforcableRange->first,
                                   current.enforcableRange->second);
              }
              current.value = value;
              return true;
            },
            [&](ControlType::Boolean &current) {
              current.value = std::get<ControlType::Boolean>(incoming).value;
              return true;
            },
            [&](ControlType::String &current) {
              const std::string &value = std::get<ControlType::String>(incoming).value;
              if (current.enforcableRange &&
                  std::find(current.enforcableRange->begin(), current.enforcableRange->end(),
                            value) == current.enforcableRange->end()) {
                logger().error("In {}, \"{}\" is not an allowed value for {}", name_, value,
                               controlChange.controlName);
                return false;
              }
              current.value = value;
              return true;
            },
        },
        control->second);
  }
};

} // namespace ScratchPadd