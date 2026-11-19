// MemoryStats.hpp - what an app's memory costs, measured before optimising
// anything (the user's: as message statistics are; Gregory, Game
// Engine Architecture 6.2 and 10.9). Two kinds of number:
//
// - Allocations, counted by the counting allocator (the library
//   //projects/scratchpadd/memory_stats:counting_allocator, which replaces
//   the global new and delete in an app that links it): how many, the bytes
//   in use and the most ever, in all and for each padd. A padd's jobs run on
//   its own thread (or, for main-thread padds, one at a time on the main
//   one), and Base marks the thread with the padd while a job runs, so an
//   allocation is put down to the padd whose job made it ("(none)" outside
//   any job: start-up, the system's own threads).
// - The heap itself (heap()): how big it is, and how much is in use and
//   free, as the platform's allocator says (newlib's mallinfo on the DS and
//   3DS, the system's on desktops), to see how near an app is to a console's
//   memory and whether it's breaking up (free memory in many pieces).
//
// As with MessageStats, anything that wants the counts makes a Reader, each
// reader its own; counting is on while any reader exists, and with none an
// allocation costs one atomic load more than malloc. Without the counting
// allocator linked, linked() is false and the counts stay at 0 (heap()
// works either way).
//
//   MemoryStats::Reader reader;                       // counting from now
//   ...
//   MemoryStats::Snapshot recent = reader.take();     // since the last take
//   MemoryStats::Heap heap = MemoryStats::heap();     // the heap as it is
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__NDS__) || defined(__3DS__)
#include <malloc.h>
#endif

namespace ScratchPadd::MemoryStats {

// Allocations and frees over some time, and the bytes in use
struct Counts {
  std::uint64_t allocations = 0;
  std::uint64_t frees = 0;
  std::uint64_t bytesAllocated = 0; // in those allocations
  // Bytes now: allocated and not freed while counting (memory from before
  // counting began, freed during it, makes it lower)
  std::int64_t inUse = 0;
  std::int64_t peak = 0; // the most in use at once, since counting began
};

struct PaddCounts {
  std::string name;
  Counts counts;
};

// The heap as the platform's allocator reports it (known: false where it
// can't say, as on Windows)
struct Heap {
  bool known = false;
  std::size_t size = 0; // the heap's whole size so far
  std::size_t inUse = 0;
  std::size_t free = 0;       // in the heap, not yet given back
  std::size_t freeChunks = 0; // free memory in this many pieces (0 if not known)
};

// Counts since a reader's last take(): in all, and for each padd that
// allocated (most allocations first)
struct Snapshot {
  Counts all;
  std::vector<PaddCounts> padds;
};

namespace Detail {
inline constexpr int kSlots = 64; // padds told apart (0: none; more share the last)

struct Slot {
  std::atomic<std::uint64_t> allocations{0}, frees{0}, bytesAllocated{0};
  std::atomic<std::int64_t> inUse{0}, peak{0};
};

struct State {
  std::atomic<int> readers{0};
  std::atomic<bool> linked{false};
  std::array<Slot, kSlots> slots;
  std::mutex namesLock;
  std::array<std::string, kSlots> names{};
  int named = 1; // slots given out (0 is "(none)")
};

// (made in place, not with new, which would come back here through the
// counting allocator; and never destroyed, as allocations come during exit)
inline State &state() {
  alignas(State) static unsigned char storage[sizeof(State)];
  static State *s = ::new (static_cast<void *>(storage)) State;
  return *s;
}

inline thread_local int currentSlot = 0;

inline void raisePeak(Slot &slot, std::int64_t now) {
  std::int64_t peak = slot.peak.load(std::memory_order_relaxed);
  while (now > peak && !slot.peak.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {
  }
}
} // namespace Detail

// Whether anything is reading (and so allocations are counted)
inline bool counting() { return Detail::state().readers.load(std::memory_order_relaxed) > 0; }
// Whether the counting allocator is in this app
inline bool linked() { return Detail::state().linked.load(std::memory_order_relaxed); }

// A padd's slot (given out the first time its name is asked for)
inline int slotFor(std::string_view name) {
  Detail::State &s = Detail::state();
  std::lock_guard lock(s.namesLock);
  for (int i = 1; i < s.named; ++i)
    if (s.names[static_cast<std::size_t>(i)] == name)
      return i;
  if (s.named == Detail::kSlots)
    return Detail::kSlots - 1; // (full: the rest share the last)
  s.names[static_cast<std::size_t>(s.named)] = std::string(name);
  return s.named++;
}

// While one is alive, this thread's allocations are the slot's (Base makes
// one around each of a padd's jobs)
class Owner {
public:
  explicit Owner(int slot) : before_(Detail::currentSlot) { Detail::currentSlot = slot; }
  ~Owner() { Detail::currentSlot = before_; }
  Owner(const Owner &) = delete;
  Owner &operator=(const Owner &) = delete;

private:
  int before_;
};

// The counting allocator's calls (it says it's linked once, at start)
inline void markLinked() { Detail::state().linked.store(true, std::memory_order_relaxed); }
inline void allocated(std::size_t bytes) {
  if (!counting())
    return;
  Detail::Slot &slot = Detail::state().slots[static_cast<std::size_t>(Detail::currentSlot)];
  slot.allocations.fetch_add(1, std::memory_order_relaxed);
  slot.bytesAllocated.fetch_add(bytes, std::memory_order_relaxed);
  const std::int64_t now =
      slot.inUse.fetch_add(static_cast<std::int64_t>(bytes), std::memory_order_relaxed) +
      static_cast<std::int64_t>(bytes);
  Detail::raisePeak(slot, now);
}
inline void freed(std::size_t bytes) {
  if (!counting())
    return;
  // (a free counts against the thread's padd, not whoever allocated it: in
  // use is net per padd, and exact in all)
  Detail::Slot &slot = Detail::state().slots[static_cast<std::size_t>(Detail::currentSlot)];
  slot.frees.fetch_add(1, std::memory_order_relaxed);
  slot.inUse.fetch_sub(static_cast<std::int64_t>(bytes), std::memory_order_relaxed);
}

// Counts from when it's made, and since each take()
class Reader {
public:
  Reader() {
    Detail::state().readers.fetch_add(1, std::memory_order_relaxed);
    taken_ = totals();
  }
  ~Reader() { Detail::state().readers.fetch_sub(1, std::memory_order_relaxed); }
  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;

  Snapshot take() {
    const std::array<Counts, Detail::kSlots> now = totals();
    Snapshot out;
    for (int i = 0; i < Detail::kSlots; ++i) {
      const Counts &a = now[static_cast<std::size_t>(i)], &b = taken_[static_cast<std::size_t>(i)];
      Counts d{a.allocations - b.allocations, a.frees - b.frees,
               a.bytesAllocated - b.bytesAllocated, a.inUse, a.peak};
      out.all.allocations += d.allocations;
      out.all.frees += d.frees;
      out.all.bytesAllocated += d.bytesAllocated;
      out.all.inUse += d.inUse;
      out.all.peak = std::max(out.all.peak, d.peak);
      if (d.allocations == 0 && d.frees == 0)
        continue;
      out.padds.push_back({nameOf(i), d});
    }
    std::sort(out.padds.begin(), out.padds.end(), [](const PaddCounts &x, const PaddCounts &y) {
      return x.counts.allocations > y.counts.allocations;
    });
    taken_ = now;
    return out;
  }

private:
  static std::string nameOf(int slot) {
    if (slot == 0)
      return "(none)";
    Detail::State &s = Detail::state();
    std::lock_guard lock(s.namesLock);
    return s.names[static_cast<std::size_t>(slot)];
  }
  static std::array<Counts, Detail::kSlots> totals() {
    std::array<Counts, Detail::kSlots> out{};
    for (int i = 0; i < Detail::kSlots; ++i) {
      const Detail::Slot &slot = Detail::state().slots[static_cast<std::size_t>(i)];
      out[static_cast<std::size_t>(i)] = {slot.allocations.load(std::memory_order_relaxed),
                                          slot.frees.load(std::memory_order_relaxed),
                                          slot.bytesAllocated.load(std::memory_order_relaxed),
                                          slot.inUse.load(std::memory_order_relaxed),
                                          slot.peak.load(std::memory_order_relaxed)};
    }
    return out;
  }
  std::array<Counts, Detail::kSlots> taken_{};
};

// The heap now, as the platform's allocator says
inline Heap heap() {
  Heap out;
#if defined(__APPLE__)
  malloc_statistics_t stats{};
  malloc_zone_statistics(nullptr, &stats);
  out.known = true;
  out.size = stats.size_allocated;
  out.inUse = stats.size_in_use;
  out.free =
      stats.size_allocated > stats.size_in_use ? stats.size_allocated - stats.size_in_use : 0;
#elif defined(__linux__) && defined(__GLIBC__)
  const struct mallinfo2 info = mallinfo2();
  out.known = true;
  out.size = info.arena + info.hblkhd;
  out.inUse = info.uordblks + info.hblkhd;
  out.free = info.fordblks;
  out.freeChunks = info.ordblks;
#elif defined(__NDS__) || defined(__3DS__)
  const struct mallinfo info = mallinfo(); // (newlib)
  out.known = true;
  out.size = static_cast<std::size_t>(info.arena);
  out.inUse = static_cast<std::size_t>(info.uordblks);
  out.free = static_cast<std::size_t>(info.fordblks);
  out.freeChunks = static_cast<std::size_t>(info.ordblks);
#endif
  return out;
}

} // namespace ScratchPadd::MemoryStats
