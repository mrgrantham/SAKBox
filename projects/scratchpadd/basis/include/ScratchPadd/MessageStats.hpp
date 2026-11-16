// MessageStats.hpp - what a system's messages and padds cost, for finding
// what's busy before optimising anything (see the README's message
// performance notes).
//
// Per message type: how many were sent, and how many jobs they made (one
// for each padd that received one), its size and whether it's trivially
// copyable. Per padd: every job it ran, by kind (message handlers, its
// repeat(), and other work it pushed for itself), each kind's count, time
// in all and longest run; its deepest queue; and the longest a job waited
// to start.
//
// Every system has one (System::messageStats()). Anything that wants the
// numbers makes a Reader, and each reader has counts of its own: one can
// take them every 5 seconds for a tab while another takes them every
// second for a graph, and neither resets the other's. Counting is on while
// any reader exists; with none it costs one atomic load per message and
// job. On, every count takes a lock, so it's for debug builds and tools.
//
//   MessageStats::Reader reader(system.messageStats());   // counting from now
//   ...
//   MessageStats::Snapshot recent = reader.take();  // since the last take, and again
#pragma once

#include <ScratchPadd/WorkQueue.hpp>

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
  // One kind of job a padd ran (JobKind): how many, and how long they took
  struct JobCounts {
    std::uint64_t count = 0;
    Clock::duration busy{};    // in all
    Clock::duration longest{}; // the longest single run
  };
  struct PaddCounts {
    std::string name;
    JobCounts messages;              // its handlers, run for messages
    JobCounts repeats;               // its repeat(), on its timer
    JobCounts pushed;                // other work it queued for itself
    std::ptrdiff_t deepestQueue = 0; // the most jobs waiting for it at once
    Clock::duration longestWait{};   // the longest a job waited to start

    // Every kind together
    Clock::duration busy() const { return messages.busy + repeats.busy + pushed.busy; }
    Clock::duration longest() const {
      return std::max({messages.longest, repeats.longest, pushed.longest});
    }
    std::uint64_t jobs() const { return messages.count + repeats.count + pushed.count; }
  };
  // Counts over `over`: types busiest first (by jobs), padds busiest first
  // (by time running jobs)
  struct Snapshot {
    Clock::duration over{};
    std::vector<TypeCounts> types;
    std::vector<PaddCounts> padds;
  };

  // Someone reading the counts. Counting starts for it when it's made, and
  // its take() gives the counts since its last take() and starts its own
  // again, whatever other readers do. Make and use it on one thread.
  class Reader {
  public:
    explicit Reader(MessageStats &stats) : stats_(stats), id_(stats.addReader()) {}
    ~Reader() { stats_.removeReader(id_); }
    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;

    Snapshot take() { return stats_.take(id_); }

  private:
    MessageStats &stats_;
    int id_;
  };

  // Whether anyone is reading, so there's anything to count
  bool counting() const { return counting_.load(std::memory_order_relaxed); }

  // What happened (Base calls these; they do nothing while no one reads)

  // A message was sent
  void sent(std::type_index type, const std::string &typeName, std::size_t size,
            bool triviallyCopyable) {
    if (!counting())
      return;
    std::lock_guard lock(mutex_);
    for (auto &[id, counts] : readers_) {
      TypeCounts &typeCounts = counts.type(type, typeName);
      ++typeCounts.messages;
      typeCounts.size = size;
      typeCounts.triviallyCopyable = triviallyCopyable;
    }
  }
  // A message's job was queued for `padd`, which now has `waiting` jobs
  void queued(std::type_index type, const std::string &typeName, const std::string &padd,
              std::ptrdiff_t waiting) {
    if (!counting())
      return;
    std::lock_guard lock(mutex_);
    for (auto &[id, counts] : readers_) {
      ++counts.type(type, typeName).jobs;
      PaddCounts &paddCounts = counts.padd(padd);
      paddCounts.deepestQueue = std::max(paddCounts.deepestQueue, waiting);
    }
  }
  // `padd` ran a job of `kind`, taking `took`, after it waited `waited`
  void ran(const std::string &padd, JobKind kind, Clock::duration took, Clock::duration waited) {
    if (!counting())
      return;
    std::lock_guard lock(mutex_);
    for (auto &[id, counts] : readers_) {
      PaddCounts &paddCounts = counts.padd(padd);
      JobCounts &job = kind == JobKind::Message  ? paddCounts.messages
                       : kind == JobKind::Repeat ? paddCounts.repeats
                                                 : paddCounts.pushed;
      ++job.count;
      job.busy += took;
      job.longest = std::max(job.longest, took);
      paddCounts.longestWait = std::max(paddCounts.longestWait, waited);
    }
  }

private:
  // One reader's counts since its last take()
  struct Counts {
    Clock::time_point since = Clock::now();
    std::map<std::type_index, TypeCounts> types;
    std::map<std::string, PaddCounts> padds;

    TypeCounts &type(std::type_index type, const std::string &typeName) {
      TypeCounts &counts = types[type];
      if (counts.name.empty())
        counts.name = typeName;
      return counts;
    }
    PaddCounts &padd(const std::string &name) {
      PaddCounts &counts = padds[name];
      if (counts.name.empty())
        counts.name = name;
      return counts;
    }
  };

  int addReader() {
    std::lock_guard lock(mutex_);
    const int id = nextReader_++;
    readers_[id] = Counts{};
    counting_.store(true, std::memory_order_relaxed);
    return id;
  }
  void removeReader(int id) {
    std::lock_guard lock(mutex_);
    readers_.erase(id);
    counting_.store(!readers_.empty(), std::memory_order_relaxed);
  }
  Snapshot take(int id) {
    std::lock_guard lock(mutex_);
    Counts &counts = readers_.at(id);
    Snapshot snapshot;
    snapshot.over = Clock::now() - counts.since;
    for (const auto &[type, typeCounts] : counts.types)
      snapshot.types.push_back(typeCounts);
    for (const auto &[name, paddCounts] : counts.padds)
      snapshot.padds.push_back(paddCounts);
    std::ranges::stable_sort(snapshot.types, std::ranges::greater{}, &TypeCounts::jobs);
    std::ranges::stable_sort(snapshot.padds, std::ranges::greater{}, &PaddCounts::busy);
    counts = Counts{};
    return snapshot;
  }

  std::atomic<bool> counting_{false};
  std::mutex mutex_; // the rest
  std::map<int, Counts> readers_;
  int nextReader_ = 0;
};

} // namespace ScratchPadd
