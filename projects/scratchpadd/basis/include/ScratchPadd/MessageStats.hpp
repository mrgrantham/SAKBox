// MessageStats.hpp - what a system's messages cost, for finding what's
// busy before optimising anything (see the README's message performance
// notes). Per message type: how many were sent, and how many jobs they made
// (one for each padd that received one), its size and whether it's
// trivially copyable. Per padd: how many it handled, how long its handlers
// took in all and at most, its deepest queue, and the longest a message
// waited in it.
//
// Every system has one (System::messageStats()). It's off until something
// turns it on, which MessageStatsPadd does, and off it costs one atomic load
// per message. On, every count takes a lock, so it's for debug builds.
//
//   system.messageStats().setEnabled(true);
//   ...
//   MessageStats::Snapshot since = system.messageStats().take(); // and start again
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <typeindex>
#include <vector>

namespace ScratchPadd {

class MessageStats {
public:
  using Clock = std::chrono::steady_clock;

  struct TypeCounts {
    std::string name;
    std::uint64_t messages = 0; // sent
    std::uint64_t jobs = 0;     // queued for receivers (a message reaching two padds: two)
    std::size_t size = 0;       // sizeof the contents
    bool triviallyCopyable = false;
  };
  struct PaddCounts {
    std::string name;
    std::uint64_t handled = 0;       // messages its handlers ran for
    Clock::duration busy{};          // in its handlers, in all
    Clock::duration longest{};       // its longest handler run
    std::ptrdiff_t deepestQueue = 0; // the most jobs waiting for it at once
    Clock::duration longestWait{};   // the longest a message waited to be handled
  };
  // Counts over `over`: types busiest first (by jobs), padds busiest first
  // (by time in handlers)
  struct Snapshot {
    Clock::duration over{};
    std::vector<TypeCounts> types;
    std::vector<PaddCounts> padds;
  };

  bool enabled() const { return enabled_.load(std::memory_order_relaxed); }
  // Turning it on starts the counts again from now
  void setEnabled(bool on) {
    std::lock_guard lock(mutex_);
    if (on && !enabled_.load(std::memory_order_relaxed))
      clear();
    enabled_.store(on, std::memory_order_relaxed);
  }

  // A message was sent (Base calls these; they do nothing while off)
  void sent(std::type_index type, const std::string &typeName, std::size_t size,
            bool triviallyCopyable) {
    if (!enabled())
      return;
    std::lock_guard lock(mutex_);
    TypeCounts &counts = typeCounts(type, typeName);
    ++counts.messages;
    counts.size = size;
    counts.triviallyCopyable = triviallyCopyable;
  }
  // A job for one, queued for `padd`, which now has `waiting` jobs queued
  void queued(std::type_index type, const std::string &typeName, const std::string &padd,
              std::ptrdiff_t waiting) {
    if (!enabled())
      return;
    std::lock_guard lock(mutex_);
    ++typeCounts(type, typeName).jobs;
    PaddCounts &counts = paddCounts(padd);
    counts.deepestQueue = std::max(counts.deepestQueue, waiting);
  }
  // `padd` ran a handler, taking `took`, for a message that had waited `waited`
  void handled(const std::string &padd, Clock::duration took, Clock::duration waited) {
    if (!enabled())
      return;
    std::lock_guard lock(mutex_);
    PaddCounts &counts = paddCounts(padd);
    ++counts.handled;
    counts.busy += took;
    counts.longest = std::max(counts.longest, took);
    counts.longestWait = std::max(counts.longestWait, waited);
  }

  // The counts since the last take() (or since it was turned on), and a
  // fresh start
  Snapshot take() {
    std::lock_guard lock(mutex_);
    Snapshot snapshot;
    snapshot.over = Clock::now() - since_;
    for (const auto &[type, counts] : types_)
      snapshot.types.push_back(counts);
    for (const auto &[name, counts] : padds_)
      snapshot.padds.push_back(counts);
    std::ranges::stable_sort(snapshot.types, std::ranges::greater{}, &TypeCounts::jobs);
    std::ranges::stable_sort(snapshot.padds, std::ranges::greater{}, &PaddCounts::busy);
    clear();
    return snapshot;
  }

private:
  void clear() {
    types_.clear();
    padds_.clear();
    since_ = Clock::now();
  }
  TypeCounts &typeCounts(std::type_index type, const std::string &typeName) {
    TypeCounts &counts = types_[type];
    if (counts.name.empty())
      counts.name = typeName;
    return counts;
  }
  PaddCounts &paddCounts(const std::string &padd) {
    PaddCounts &counts = padds_[padd];
    if (counts.name.empty())
      counts.name = padd;
    return counts;
  }

  std::atomic<bool> enabled_{false};
  std::mutex mutex_; // the rest
  Clock::time_point since_ = Clock::now();
  std::map<std::type_index, TypeCounts> types_;
  std::map<std::string, PaddCounts> padds_;
};

} // namespace ScratchPadd
