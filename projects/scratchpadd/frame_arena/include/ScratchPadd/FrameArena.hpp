// FrameArena.hpp - a single-frame allocator (Gregory, Game Engine
// Architecture 6.2.1, "Single-Frame and Double-Buffered Memory Allocators";
// round 90, the user's: for the bots' search, measured at 59,000
// allocations a second on a 3DS). A block taken once; allocations during a
// frame (here: one simulated game) are handed out from it by moving a
// pointer along, never freed one by one, and the whole block is reused at
// once with reset().
//
// It works through the counting allocator (memory_stats:counting_allocator,
// which replaces the global new and delete): while an InFrameArena is alive
// on a thread, that thread's new comes from the arena (and its delete of
// arena memory does nothing). Without the counting allocator linked, an
// InFrameArena changes nothing: allocations go to malloc as ever. Memory
// that must outlive the frame (a search tree growing across games) is
// allocated under OutOfFrameArena. A full arena falls back to malloc.
//
//   ScratchPadd::FrameArena arena(64 * 1024);
//   for (...) {
//     {
//       ScratchPadd::InFrameArena frame(arena);
//       ... copies and lists, made and dropped ...
//       { ScratchPadd::OutOfFrameArena keep; tree.push_back(...); }
//     }
//     arena.reset(); // (every allocation of the frame given back at once)
//   }
//
// Anything made in a frame must be gone by its reset(): a static first made
// inside one would point at reused memory (so make such tables at start).
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace ScratchPadd {

class FrameArena;
namespace FrameArenaDetail {
inline thread_local FrameArena *active = nullptr; // (allocations come from it)
inline thread_local FrameArena *recent = nullptr; // (its memory may still be deleted)
inline thread_local int outside = 0;              // (OutOfFrameArena's depth)
} // namespace FrameArenaDetail

class FrameArena {
public:
  explicit FrameArena(std::size_t bytes)
      : start_(static_cast<unsigned char *>(std::malloc(bytes))), size_(start_ ? bytes : 0) {}
  ~FrameArena() {
    if (FrameArenaDetail::recent == this)
      FrameArenaDetail::recent = nullptr; // (nothing of it left to delete)
    std::free(start_);
  }
  FrameArena(const FrameArena &) = delete;
  FrameArena &operator=(const FrameArena &) = delete;

  // `bytes` from the block (16-byte aligned), or nothing if it's full
  void *take(std::size_t bytes) {
    const std::size_t at = (used_ + 15) & ~std::size_t{15};
    if (at + bytes > size_)
      return nullptr;
    used_ = at + bytes;
    peak_ = std::max(peak_, used_);
    ++taken_;
    return start_ + at;
  }
  bool owns(const void *p) const {
    const auto *c = static_cast<const unsigned char *>(p);
    return c >= start_ && c < start_ + size_;
  }
  // Every allocation since the last reset given back at once
  void reset() { used_ = 0; }

  std::size_t size() const { return size_; }
  std::size_t peak() const { return peak_; }           // the most a frame used
  std::uint64_t allocations() const { return taken_; } // handed out, in all

private:
  unsigned char *start_;
  std::size_t size_;
  std::size_t used_ = 0, peak_ = 0;
  std::uint64_t taken_ = 0;
};

// While one lives, this thread's allocations come from `arena`
class InFrameArena {
public:
  explicit InFrameArena(FrameArena &arena) : before_(FrameArenaDetail::active) {
    FrameArenaDetail::active = &arena;
    FrameArenaDetail::recent = &arena;
  }
  ~InFrameArena() { FrameArenaDetail::active = before_; }
  InFrameArena(const InFrameArena &) = delete;
  InFrameArena &operator=(const InFrameArena &) = delete;

private:
  FrameArena *before_;
};

// While one lives, this thread's allocations come from the heap even in a
// frame (for what outlives it)
class OutOfFrameArena {
public:
  OutOfFrameArena() { ++FrameArenaDetail::outside; }
  ~OutOfFrameArena() { --FrameArenaDetail::outside; }
  OutOfFrameArena(const OutOfFrameArena &) = delete;
  OutOfFrameArena &operator=(const OutOfFrameArena &) = delete;
};

// The counting allocator's calls: memory from the thread's arena, if one is
// in use and has room; and whether a pointer is arena memory (not freed)
inline void *frameAllocate(std::size_t bytes) {
  FrameArena *arena = FrameArenaDetail::active;
  if (!arena || FrameArenaDetail::outside > 0)
    return nullptr;
  return arena->take(bytes);
}
inline bool frameOwns(const void *p) {
  const FrameArena *arena = FrameArenaDetail::recent;
  return arena && arena->owns(p);
}

} // namespace ScratchPadd
