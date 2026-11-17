// ActivityPadd with the counting allocator linked (round 89): allocations a
// second in all and for each padd, put down to the padd whose job made
// them, and the heap in use where the platform says.
#include <ScratchPadd/ActivityPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using ScratchPadd::ActivityPadd;
using ScratchPadd::ControlType::Graph;

struct Ping {};

// Allocates 100 blocks (kept) when pinged
class WorkerPadd : public ScratchPadd::Base {
public:
  explicit WorkerPadd(ScratchPadd::System *system) : Base(system) {
    receive<Ping>([this](const Ping &) {
      for (int i = 0; i < 100; ++i)
        kept_.push_back(std::make_unique<std::vector<int>>(64));
    });
  }
  std::string name() override { return "Worker"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

private:
  std::vector<std::unique_ptr<std::vector<int>>> kept_;
};

template <typename... Padds> struct Running {
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread thread;
  Running() {
    system.instantiate();
    thread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~Running() {
    system.stop();
    thread.join();
  }
  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
};

// ActivityPadd's second, run now, and the graph named `name` after it
// (empty if there's no such graph)
std::vector<float> secondThen(ActivityPadd &activity, const std::string &name) {
  return ScratchPadd::onPaddThread(activity, [&](ActivityPadd &padd) {
    padd.repeat();
    if (!ScratchPadd::PaddTestAccess::hasControl(padd, name))
      return std::vector<float>{};
    return ScratchPadd::PaddTestAccess::control<Graph>(padd, name).values;
  });
}

} // namespace

TEST(ActivityPaddMemoryTest, GraphsEachPaddsAllocations) {
  ASSERT_TRUE(ScratchPadd::MemoryStats::linked());
  Running<ActivityPadd, WorkerPadd> app;
  ActivityPadd &activity = app.padd<ActivityPadd>();
  WorkerPadd &worker = app.padd<WorkerPadd>();
  secondThen(activity, ActivityPadd::kMessages); // (a fresh start)

  ScratchPadd::PaddTestAccess::send(activity, Ping{}); // (to the worker)
  worker.waitForWorkCompletion();
  const std::vector<float> all = secondThen(activity, ActivityPadd::kAllocations);
  ASSERT_FALSE(all.empty());
  EXPECT_GT(all.back(), 0.0f);
  const std::vector<float> mine = secondThen(activity, ActivityPadd::allocationsGraph("Worker"));
  ASSERT_GE(mine.size(), 2u); // (the graph appeared; the second just run added another value)
  EXPECT_GT(mine[mine.size() - 2], 0.0f); // (the second it pinged: its 200 allocations, a second)
#if defined(__APPLE__) || defined(__linux__)
  EXPECT_FALSE(secondThen(activity, ActivityPadd::kHeap).empty());
#endif
}
