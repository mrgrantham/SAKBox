#pragma once

#include <ScratchPadd/EventTimer.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <typeindex>
#include <unordered_map>

#include <ScratchPadd/Helper.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/System.hpp>
#include <ScratchPadd/WorkQueue.hpp>

namespace ScratchPadd {
using namespace std::chrono_literals;

class Base : public Logger {
protected:
  // Read by the work loop and written from other threads (e.g. stop()).
  std::atomic<bool> on_{false};
  std::thread workerThread_;
  EventTimer repeatingTimer_;
  System *system_;
  // Wakes this padd's own thread when work arrives. Main-thread padds are
  // switched to the system's shared signal instead (useWakeSignal).
  WakeSignal ownSignal_;
  WorkQueue workQueue_{&ownSignal_};
  std::atomic<ssize_t> pendingWork_{0};
  std::atomic<std::uint64_t> workLoopPasses_{0};
  std::condition_variable workCompletionConditionVariable_;
  std::mutex workCompletionMutex_;
  std::condition_variable scratchPaddConditionVariable_;

private:
  std::unordered_map<std::string, ControlTypeVariant> controlMap_;
  std::optional<std::chrono::duration<double>> repeatInterval_{std::nullopt};
  std::string name_{"Unnamed Padd"};
  bool shouldUseMainThread_{false};
  std::mutex scratchPaddMutex_;
  // Message handlers by message type. Senders read this from their own
  // threads (handles()) while the padd may change it (receive(),
  // stopReceiving()), so it's guarded by handlersMutex_. Each handler is held
  // by shared_ptr so deliver() can run it after unlocking, even if the
  // handler replaces or removes itself.
  using Handler = std::function<void(const MessageBase &)>;
  std::unordered_map<std::type_index, std::shared_ptr<const Handler>> handlers_;
  mutable std::shared_mutex handlersMutex_;

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
    return ScratchPadd::MessageType::ControlSnapshot{.paddName = name_, .controlMap = controlMap_};
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

  // How many times the work loop has drained this padd's queue. An idle padd
  // sleeps, so this only grows when work arrives (tests use it).
  std::uint64_t workLoopPasses() const { return workLoopPasses_; }

  void waitForWorkCompletion() {
    std::unique_lock<std::mutex> lk(workCompletionMutex_);
    workCompletionConditionVariable_.wait_for(lk, 3000ms, [this] {
      logger().info("waitForWorkCompletion in {}: {} jobs left", name_, pendingWork_.load());
      return pendingWork_ == 0;
    });
  }

  // Every Padd needs to implement this so their name can be setup for use in
  // the system. It's read once, when the system initializes the padd; use
  // paddName() after that.
  virtual std::string name() = 0;

  // The name name() gave when the padd was initialized (before that,
  // "Unnamed Padd"). Doesn't call name() again, which for a padd built on
  // __CLASS_NAME__ demangles its class name each time.
  const std::string &paddName() const { return name_; }

  // Implementing this to true will execute the Padd work queue in whatever
  // thread the overall system was started in. Overriding this method to return
  // false will run the work queue in its own background thread
  virtual bool shouldUseMainThread() { return false; }

  // Overriding this to return a std::chrono::duration interval will setup
  // the padd to call the repeat() virtual method repeatedly on the interval
  // provided
  virtual std::optional<std::chrono::duration<double>> repeatInterval() { return std::nullopt; }

  // The controls this padd starts with: values a control panel (such as
  // DisplayPadd's) shows and lets the user edit. Read once, by initialize();
  // to change them while the padd runs, use setControl(), addControl() and
  // removeControl(). Return {} for none.
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
    logger().info("Base::repeat() called. Padd set a repeat interval of {}ms with "
                  "no method override",
                  repeatInterval_ ? repeatInterval_->count() : 0);
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

  // Makes pushes to this padd wake `signal` instead of the padd's own. The
  // system calls this for main-thread padds, so that a push to any of them
  // wakes their shared loop. Only valid before the padd starts.
  void useWakeSignal(WakeSignal &signal) { workQueue_.setSignal(&signal); }

  // Called by the system's main-thread loop, which does the waiting.
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

  // The work loop of a padd with its own thread: run whatever is queued,
  // then sleep until more work arrives or the padd is stopped.
  void run() {
    startingWrapper();
    WakeSignal &signal = workQueue_.signal();
    while (on_) {
      // Read before draining: work pushed while draining changes the
      // generation, so the wait below returns straight away for it.
      const std::uint64_t seen = signal.generation();
      processWork();
      signal.waitForChange(seen, [this] { return !on_; });
    }
    finishingWrapper();
  }

  // Runs every job queued so far, oldest first. Jobs taken after the padd
  // has been stopped are discarded rather than run.
  void processWork() {
    for (WorkQueue::Job &job : workQueue_.takeAll()) {
      if (on_) {
        job();
      }
      pendingWork_--;
    }
    ++workLoopPasses_;
    workCompletionConditionVariable_.notify_all();
  }

  // If you want some method to repeat at a regular interval, you
  // can implement the repeat() method and set a repeat interval.
  // A lambda for the repeating work will be added to the work queue
  // and processed immediately.
  void startRepeater() {
    if (repeatInterval_) {
      logger().info("repeat() interval set to {}ms for {}", repeatInterval_.value().count(), name_);
      repeatingTimer_.startRepeatingEvent([this] { push([this] { repeat(); }); },
                                          repeatInterval_.value());
    } else {
      logger().info("No repeat() interval set for: {}", name_);
    }
  }

  // Queues `job` to run on this padd's thread and wakes it. Safe from any
  // thread.
  void push(WorkQueue::Job job) {
    pendingWork_++;
    workQueue_.push(std::move(job));
  }

  // Registers `handler` for messages of type T, replacing any earlier
  // handler for T. The handler runs on this padd's thread and gets the
  // contents as a const reference; the message is shared with other padds.
  //
  // Usually called from the constructor or prepare(), but a running padd can
  // call it too (typically from its own thread, e.g. in a handler or
  // repeat()). The system only queues a message for padds that handle its
  // type when it's sent, so messages sent before the handler was registered
  // never arrive.
  template <typename T, typename OnMessage> void receive(OnMessage onMessage) {
    auto handler = std::make_shared<const Handler>(
        [onMessage = std::move(onMessage)](const MessageBase &message) {
          onMessage(static_cast<const MessageOf<T> &>(message).value);
        });
    std::unique_lock lock(handlersMutex_);
    handlers_.insert_or_assign(std::type_index(typeid(T)), std::move(handler));
  }

  // Removes this padd's handler for messages of type T, so they stop being
  // sent to it. Messages of type T already queued are dropped when their turn
  // comes. Can be called while the padd runs, like receive(). Returns false
  // if there was no handler for T.
  template <typename T> bool stopReceiving() {
    std::unique_lock lock(handlersMutex_);
    return handlers_.erase(std::type_index(typeid(T))) != 0;
  }

  // Whether this padd has a handler for messages of `type`. Safe from any
  // thread; the system calls it from the sender's.
  bool handles(std::type_index type) const {
    std::shared_lock lock(handlersMutex_);
    return handlers_.contains(type);
  }

  // Runs this padd's current handler for `message`, if it still has one.
  // Called on the padd's thread by the work queue.
  void deliver(const Message &message) {
    std::shared_ptr<const Handler> handler;
    {
      std::shared_lock lock(handlersMutex_);
      auto found = handlers_.find(message->type());
      if (found == handlers_.end()) {
        return;
      }
      handler = found->second;
    }
    (*handler)(*message);
  }

  void stop() {
    logger().info("Stopping: {}", name_);
    repeatingTimer_.stop(); // might need to flush the queue
    notifyStop();
    workQueue_.signal().notify(); // wake the work loop so it sees on_ is false
    if (workerThread_.joinable()) {
      workerThread_.join();
    }
  }

  // Sends `message` to every other padd that handles its type.
  void send(Message message) {
    logger().debug("send({}) from {}", message->typeName(), name_);
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

  // Changes this padd makes to its own controls while it runs. Call them on
  // the padd's own thread, like control<C>(). Each sends a fresh snapshot,
  // so control panels show the change straight away. None calls
  // controlChanged(), which reports edits made from a panel.

  // Sets control `name` to the value in `value`, under the same rules as an
  // edit from a panel (clamped to its range, an allowed string, the same
  // type; only the value is taken). Returns false, changing nothing and
  // sending nothing, if it isn't allowed.
  bool setControl(const std::string &name, ControlTypeVariant value) {
    if (!updateControl(MessageType::ControlChange{
            .paddName = name_, .controlName = name, .controlTypeValue = std::move(value)})) {
      return false;
    }
    send(generateControlsSnapshot());
    return true;
  }

  // Adds control `name`, or replaces the one with that name (its range
  // too), and gives it a panel if it's this padd's first.
  void addControl(const std::string &name, ControlTypeVariant control) {
    controlMap_.insert_or_assign(name, std::move(control));
    send(generateControlsSnapshot());
  }

  // Removes control `name`. Removing the last one sends an empty snapshot,
  // which takes this padd's panel away. Returns false if there was none.
  bool removeControl(const std::string &name) {
    if (controlMap_.erase(name) == 0) {
      return false;
    }
    send(generateControlsSnapshot());
    return true;
  }
};

} // namespace ScratchPadd