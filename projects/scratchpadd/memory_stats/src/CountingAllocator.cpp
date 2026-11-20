// CountingAllocator.cpp - the global new and delete, replaced so that
// MemoryStats (ScratchPadd/MemoryStats.hpp) can count allocations, and so a
// thread can allocate from a frame arena (ScratchPadd/FrameArena.hpp) while
// it has one in use: otherwise each goes to malloc as before, and its size (as the allocator gave
// it) is counted while anything reads the counts. An app gets this by linking the library
// //projects/scratchpadd/memory_stats:counting_allocator (or, on a console, by building this file);
// without it nothing is counted.
#include <ScratchPadd/FrameArena.hpp>
#include <ScratchPadd/MemoryStats.hpp>

#include <cstdlib>
#include <new>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(_WIN32)
#include <malloc.h>
#else
#include <malloc.h>
#endif

namespace {
namespace MS = ScratchPadd::MemoryStats;

// The bytes the allocator gave for `p` (at least what was asked for)
std::size_t sizeOf(void *p) {
#if defined(__APPLE__)
  return malloc_size(p);
#elif defined(_WIN32)
  return _msize(p);
#else
  return malloc_usable_size(p);
#endif
}

// Said once, before main: the counting allocator is in this app
const bool kLinked = [] {
  MS::markLinked();
  return true;
}();

void *allocate(std::size_t bytes) {
  // (a frame arena in use on this thread: from it, if it has room)
  if (void *p = ScratchPadd::frameAllocate(bytes == 0 ? 1 : bytes))
    return p;
  void *p = std::malloc(bytes == 0 ? 1 : bytes);
  if (!p)
    throw std::bad_alloc();
  if (!MS::takeFromBudget(sizeOf(p))) { // (a heap budget, and past it)
    std::free(p);
    throw std::bad_alloc();
  }
  if (MS::counting()) // (the size only asked for while counting)
    MS::allocated(sizeOf(p));
  return p;
}

void *allocateNoThrow(std::size_t bytes) noexcept {
  if (void *p = ScratchPadd::frameAllocate(bytes == 0 ? 1 : bytes))
    return p;
  void *p = std::malloc(bytes == 0 ? 1 : bytes);
  if (p && !MS::takeFromBudget(sizeOf(p))) {
    std::free(p);
    return nullptr;
  }
  if (p && MS::counting())
    MS::allocated(sizeOf(p));
  return p;
}

void release(void *p) noexcept {
  if (!p || ScratchPadd::frameOwns(p)) // (arena memory: given back at its reset)
    return;
  // (the size only asked for when it's wanted)
  if (MS::budgetOn() || MS::counting()) {
    const std::size_t size = sizeOf(p);
    MS::giveToBudget(size);
    if (MS::counting())
      MS::freed(size);
  }
  std::free(p);
}
} // namespace

// (aligned new and delete are left as the library's own: nothing in
// ScratchPadd's apps over-aligns, and their memory isn't counted)
void *operator new(std::size_t bytes) { return allocate(bytes); }
void *operator new[](std::size_t bytes) { return allocate(bytes); }
void *operator new(std::size_t bytes, const std::nothrow_t &) noexcept {
  return allocateNoThrow(bytes);
}
void *operator new[](std::size_t bytes, const std::nothrow_t &) noexcept {
  return allocateNoThrow(bytes);
}
void operator delete(void *p) noexcept { release(p); }
void operator delete[](void *p) noexcept { release(p); }
void operator delete(void *p, std::size_t) noexcept { release(p); }
void operator delete[](void *p, std::size_t) noexcept { release(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { release(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { release(p); }
