// ActivityPadd: every second (here, each repeat() the test runs), the
// messages sent and each padd's busy share and longest wait, added to
// graphs; a padd's graphs appearing once it runs a job, and moving on with
// a 0 in a second it's idle.
#include <ScratchPadd/ActivityPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

namespace {

using ScratchPadd::ActivityPadd;
using ScratchPadd::ControlType::Graph;

struct Ping {};

// Works for 30 ms when pinged
class WorkerPadd : public ScratchPadd::Base {
public:
  explicit WorkerPadd(ScratchPadd::System *system) : Base(system) {
    receive<Ping>([](const Ping &) { std::this_thread::sleep_for(std::chrono::milliseconds(30)); });
  }
  std::string name() override { return "Worker"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
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

TEST(ActivityPaddTest, GraphsEachPaddsBusySecond) {
  Running<ActivityPadd, WorkerPadd> app;
  ActivityPadd &activity = app.padd<ActivityPadd>();
  WorkerPadd &worker = app.padd<WorkerPadd>();
  secondThen(activity, ActivityPadd::kMessages); // (a fresh start)

  for (int i = 0; i < 3; ++i)
    ScratchPadd::PaddTestAccess::send(activity, Ping{}); // (to the worker)
  worker.waitForWorkCompletion();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  const std::vector<float> busy = secondThen(activity, ActivityPadd::busyGraph("Worker"));
  ASSERT_EQ(busy.size(), 1u); // the graph appeared, with this second's value
  // 90 ms of work in a "second" of about 110 ms or more: busy, but not
  // over 100%
  EXPECT_GT(busy.back(), 10.0f);
  EXPECT_LE(busy.back(), 100.0f);
  const std::vector<float> wait = secondThen(activity, ActivityPadd::waitGraph("Worker"));
  ASSERT_EQ(wait.size(), 2u);   // (the idle second just run added a 0 here)
  EXPECT_EQ(wait.back(), 0.0f); // idle: nothing waited
  const std::vector<float> idle = secondThen(activity, ActivityPadd::busyGraph("Worker"));
  ASSERT_EQ(idle.size(), 3u);
  EXPECT_EQ(idle.back(), 0.0f);

  const std::vector<float> messages = secondThen(activity, ActivityPadd::kMessages);
  EXPECT_EQ(messages.size(), 5u); // one value a second, every second
}

TEST(ActivityPaddTest, CountsMessagesASecond) {
  Running<ActivityPadd, WorkerPadd> app;
  ActivityPadd &activity = app.padd<ActivityPadd>();
  WorkerPadd &worker = app.padd<WorkerPadd>();
  secondThen(activity, ActivityPadd::kMessages);
  ScratchPadd::PaddTestAccess::send(activity, Ping{}); // (to the worker)
  worker.waitForWorkCompletion();
  const std::vector<float> messages = secondThen(activity, ActivityPadd::kMessages);
  ASSERT_EQ(messages.size(), 2u);
  EXPECT_GT(messages.back(), 0.0f); // (one message over a short second: many a second)
}
