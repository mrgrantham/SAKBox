#pragma once

// For tests only: reaching inside padds, which padds themselves can't do to
// each other (see "Padds only talk through messages" in the ScratchPadd
// README). Tests drive padds from outside: sending as if from a padd,
// queueing work on its thread, and reading its controls.

#include <ScratchPadd/Base.hpp>

#include <future>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace ScratchPadd {

struct PaddTestAccess {
  // Messages, sent from the test's thread as if `from` sent them
  template <typename T> static void send(Base &from, T contents) { from.send(std::move(contents)); }
  template <typename T> static void sendIncludeSender(Base &from, T contents) {
    from.sendIncludeSender(std::move(contents));
  }
  template <typename T> static bool sendTo(Base &from, PaddAddress to, T contents) {
    return from.sendTo(to, std::move(contents));
  }
  static PaddAddress address(Base &padd) { return padd.address(); }
  static PaddAddress addressOf(Base &padd, std::string_view name) { return padd.addressOf(name); }
  template <typename Padd> static PaddAddress addressOf(Base &padd) {
    return padd.addressOf<Padd>();
  }
  static bool handles(const Base &padd, std::type_index type) { return padd.handles(type); }

  // Queues `job` on the padd's thread
  static void push(Base &padd, WorkQueue::Job job) { padd.push(std::move(job)); }

  // A padd's lifecycle, for tests that run one without a system
  static void initialize(Base &padd) { padd.initialize(); }
  static void notifyStart(Base &padd) { padd.notifyStart(); }
  static void notifyStop(Base &padd) { padd.notifyStop(); }

  // Its controls. While it runs, call these on its own thread (onPaddThread
  // below), as a padd would; from another thread they throw.
  template <typename C> static const C &control(const Base &padd, const std::string &name) {
    return padd.control<C>(name);
  }
  static MessageType::ControlSnapshot snapshot(const Base &padd) {
    return padd.generateControlsSnapshot();
  }
  static bool updateControl(Base &padd, const MessageType::ControlChange &change) {
    return padd.updateControl(change);
  }
  static bool setControl(Base &padd, const std::string &name, ControlTypeVariant value) {
    return padd.setControl(name, std::move(value));
  }
  static void addControl(Base &padd, const std::string &name, ControlTypeVariant control) {
    padd.addControl(name, std::move(control));
  }
  static bool removeControl(Base &padd, const std::string &name) {
    return padd.removeControl(name);
  }
};

// Runs `work(padd)` on `padd`'s own thread, waits for it, and returns what
// it returned (rethrowing what it threw). The padd must be running.
template <typename Padd, typename Work> auto onPaddThread(Padd &padd, Work work) {
  using Result = std::invoke_result_t<Work &, Padd &>;
  std::promise<Result> done;
  std::future<Result> result = done.get_future();
  PaddTestAccess::push(padd, [&padd, &work, &done] {
    try {
      if constexpr (std::is_void_v<Result>) {
        work(padd);
        done.set_value();
      } else {
        done.set_value(work(padd));
      }
    } catch (...) {
      done.set_exception(std::current_exception());
    }
  });
  return result.get();
}

} // namespace ScratchPadd
