// Memory statistics: with the counting allocator linked,
// allocations counted only while something reads, put down to the slot
// marked on the thread (as Base marks a padd's jobs), each reader its own
// counts, and the heap's own figures where the platform gives them
#include <ScratchPadd/FrameArena.hpp>
#include <ScratchPadd/MemoryStats.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <vector>

namespace MS = ScratchPadd::MemoryStats;

namespace {
const MS::PaddCounts *find(const MS::Snapshot &snapshot, const std::string &name) {
  for (const MS::PaddCounts &padd : snapshot.padds)
    if (padd.name == name)
      return &padd;
  return nullptr;
}
} // namespace

TEST(MemoryStatsTest, TheCountingAllocatorIsLinked) { EXPECT_TRUE(MS::linked()); }

TEST(MemoryStatsTest, CountsWhileReadAndByOwner) {
  const int slot = MS::slotFor("Tester");
  EXPECT_EQ(MS::slotFor("Tester"), slot); // (the same name, the same slot)
  // Nothing counted with nobody reading
  {
    const MS::Owner owner(slot);
    auto before = std::make_unique<std::vector<int>>(1000);
    (void)before;
  }
  MS::Reader reader;
  std::unique_ptr<std::vector<int>> kept;
  {
    const MS::Owner owner(slot);
    kept = std::make_unique<std::vector<int>>(1000); // (4,000 bytes and the vector)
  }
  const MS::Snapshot first = reader.take();
  const MS::PaddCounts *tester = find(first, "Tester");
  ASSERT_NE(tester, nullptr);
  EXPECT_GE(tester->counts.allocations, 2u);
  EXPECT_GE(tester->counts.bytesAllocated, 4000u);
  EXPECT_GE(tester->counts.inUse, 4000);
  EXPECT_GE(first.all.allocations, tester->counts.allocations);
  // Freed on the tester's thread: its in-use goes back down
  {
    const MS::Owner owner(slot);
    kept.reset();
  }
  const MS::Snapshot second = reader.take();
  const MS::PaddCounts *again = find(second, "Tester");
  ASSERT_NE(again, nullptr);
  EXPECT_GE(again->counts.frees, 2u);
  EXPECT_EQ(again->counts.allocations, 0u); // (since the last take)
  EXPECT_LT(again->counts.inUse, tester->counts.inUse);
  EXPECT_GE(again->counts.peak, tester->counts.inUse);
}

TEST(MemoryStatsTest, AnotherThreadsAllocationsAreItsOwn) {
  const int slot = MS::slotFor("Worker");
  MS::Reader reader;
  // (kept past the thread, so the compiler can't leave the allocation out)
  std::unique_ptr<std::vector<char>> kept;
  std::thread worker([&] {
    const MS::Owner owner(slot);
    kept = std::make_unique<std::vector<char>>(10000);
  });
  worker.join();
  const MS::Snapshot snapshot = reader.take(); // (kept: find() points into it)
  const MS::PaddCounts *counts = find(snapshot, "Worker");
  ASSERT_NE(counts, nullptr);
  EXPECT_GE(counts->counts.bytesAllocated, 10000u);
}

// (Under ThreadSanitizer its own allocator takes the system's place, so the
// system's figures are 0: nothing to check)
#if defined(__has_feature)
#if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
#define SANITIZED_ALLOCATOR 1
#endif
#endif
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
#define SANITIZED_ALLOCATOR 1
#endif

TEST(MemoryStatsTest, TheHeapsOwnFigures) {
  const MS::Heap heap = MS::heap();
#if (defined(__APPLE__) || defined(__linux__)) && !defined(SANITIZED_ALLOCATOR)
  EXPECT_TRUE(heap.known);
  EXPECT_GT(heap.inUse, 0u);
  EXPECT_GE(heap.size, heap.inUse);
#else
  (void)heap;
#endif
}

// The frame arena: in a frame, allocations come from the arena
// (not the heap: not counted), deleting them does nothing, OutOfFrameArena
// goes to the heap, a full arena falls back to the heap, and reset() gives
// the whole frame back
TEST(FrameArenaTest, AFramesAllocationsComeFromTheArena) {
  ScratchPadd::FrameArena arena(64 * 1024);
  MS::Reader reader;
  std::unique_ptr<std::vector<int>> kept;
  {
    ScratchPadd::InFrameArena frame(arena);
    auto inFrame = std::make_unique<std::vector<int>>(100);
    EXPECT_TRUE(arena.owns(inFrame.get()));
    EXPECT_TRUE(arena.owns(inFrame->data()));
    {
      ScratchPadd::OutOfFrameArena keep;
      kept = std::make_unique<std::vector<int>>(100);
    }
    EXPECT_FALSE(arena.owns(kept.get()));
    // Too big for what's left: from the heap
    auto big = std::make_unique<std::vector<char>>(100 * 1024);
    EXPECT_FALSE(arena.owns(big->data()));
  } // (the frame's own deleted here: nothing to free)
  const MS::Snapshot counts = reader.take();
  EXPECT_LE(counts.all.allocations, 4u); // (the kept two and the big one: not the frame's)
  EXPECT_GE(arena.allocations(), 2u);
  EXPECT_GT(arena.peak(), 400u);
  arena.reset();
  {
    ScratchPadd::InFrameArena frame(arena);
    auto again = std::make_unique<int>(7);
    EXPECT_TRUE(arena.owns(again.get())); // (the block reused)
  }
  kept.reset(); // (heap memory, freed as ever)
}
