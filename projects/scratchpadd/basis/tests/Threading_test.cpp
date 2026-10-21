#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

// A padd that can run a job on whichever thread its work queue is processed
// on and report back that thread's id.
template <bool UseMainThread, int Id> class ThreadPadd : public ScratchPadd::Base {
public:
  ThreadPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "ThreadPadd" + std::to_string(Id); }
  bool shouldUseMainThread() override { return UseMainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  // Queues a job and returns the id of the thread that ran it.
  std::thread::id workThreadId() {
    auto promise = std::make_shared<std::promise<std::thread::id>>();
    auto future = promise->get_future();
    push([promise] { promise->set_value(std::this_thread::get_id()); });
    EXPECT_EQ(future.wait_for(2s), std::future_status::ready) << name() << " never ran its work";
    return future.valid() ? future.get() : std::thread::id{};
  }
};

using MainA = ThreadPadd<true, 1>;
using MainB = ThreadPadd<true, 2>;
using BackgroundA = ThreadPadd<false, 3>;
using BackgroundB = ThreadPadd<false, 4>;

// Runs a system on its own thread, standing in for the app's main thread.
template <typename... Padds> class RunningSystem {
public:
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread mainThread;

  RunningSystem() {
    system.instantiate();
    mainThread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~RunningSystem() { stop(); }

  void stop() {
    if (mainThread.joinable()) {
      system.stop();
      mainThread.join();
    }
  }

  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
};

} // namespace

// Implementing shouldUseMainThread() to return true for all padds makes every
// padd run in the main thread event loop.
TEST(ThreadingTest, TestAllMainThread) {
  RunningSystem<MainA, MainB> running;
  const auto mainId = running.mainThread.get_id();

  EXPECT_EQ(running.padd<MainA>().workThreadId(), mainId);
  EXPECT_EQ(running.padd<MainB>().workThreadId(), mainId);
}

// Implementing shouldUseMainThread() to return false for all padds makes each
// padd run on its own background thread.
TEST(ThreadingTest, TestAllBackgroundThreads) {
  RunningSystem<BackgroundA, BackgroundB> running;
  const auto mainId = running.mainThread.get_id();

  const auto a = running.padd<BackgroundA>().workThreadId();
  const auto b = running.padd<BackgroundB>().workThreadId();
  EXPECT_NE(a, mainId);
  EXPECT_NE(b, mainId);
  EXPECT_NE(a, b);
  EXPECT_NE(a, std::this_thread::get_id());
}

// Implementing shouldUseMainThread() differently for different padds gives
// each padd the correct behavior.
TEST(ThreadingTest, TestMixedBackgroundAndMainThreads) {
  RunningSystem<MainA, BackgroundA, MainB, BackgroundB> running;
  const auto mainId = running.mainThread.get_id();

  EXPECT_EQ(running.padd<MainA>().workThreadId(), mainId);
  EXPECT_EQ(running.padd<MainB>().workThreadId(), mainId);
  const auto a = running.padd<BackgroundA>().workThreadId();
  const auto b = running.padd<BackgroundB>().workThreadId();
  EXPECT_NE(a, mainId);
  EXPECT_NE(b, mainId);
  EXPECT_NE(a, b);
}

// stop() from another thread ends the main thread's run loop and stops every
// padd.
TEST(ThreadingTest, TestStopFromAnotherThread) {
  RunningSystem<MainA, BackgroundA> running;
  EXPECT_TRUE(running.system.isRunning());
  EXPECT_TRUE(running.padd<MainA>().isRunning());
  EXPECT_TRUE(running.padd<BackgroundA>().isRunning());

  running.stop(); // returns only once start() has returned on the main thread

  EXPECT_FALSE(running.system.isRunning());
  EXPECT_FALSE(running.padd<MainA>().isRunning());
  EXPECT_FALSE(running.padd<BackgroundA>().isRunning());
}

// waitForStart()/waitForStop() wake up when another thread changes the
// running state. Repeated so a lost wakeup would hang (and time out) the test.
TEST(ThreadingTest, TestStartStopHandoffBetweenThreads) {
  ScratchPadd::SystemImplementation<BackgroundA> system;
  system.instantiate();
  auto &padd = *std::get<std::unique_ptr<BackgroundA>>(system.getWorkers());

  for (int i = 0; i < 200; ++i) {
    std::thread starter([&] { ScratchPadd::PaddTestAccess::notifyStart(padd); });
    padd.waitForStart();
    EXPECT_TRUE(padd.isRunning());
    starter.join();

    std::thread stopper([&] { ScratchPadd::PaddTestAccess::notifyStop(padd); });
    padd.waitForStop();
    EXPECT_FALSE(padd.isRunning());
    stopper.join();
  }
}

namespace {

// Records which hook ran on which thread, in order, across all padds.
struct HookLog {
  struct Entry {
    std::string hook;
    std::string padd;
    std::thread::id thread;
  };
  std::mutex mutex;
  std::vector<Entry> entries;

  void add(const std::string &hook, const std::string &padd) {
    std::lock_guard<std::mutex> lk(mutex);
    entries.push_back({hook, padd, std::this_thread::get_id()});
  }
};
HookLog hookLog;

template <bool UseMainThread, int Id> class RecordingPadd : public ThreadPadd<UseMainThread, Id> {
public:
  using ThreadPadd<UseMainThread, Id>::ThreadPadd;
  void finishing() override { hookLog.add("finishing", this->name()); }
  void cleanup() override { hookLog.add("cleanup", this->name()); }
};

// Calls system_->stop() from inside its own repeat().
template <bool UseMainThread, int Id>
class SelfStoppingPadd : public ThreadPadd<UseMainThread, Id> {
public:
  using ThreadPadd<UseMainThread, Id>::ThreadPadd;
  std::optional<std::chrono::duration<double>> repeatInterval() override { return 5ms; }
  void repeat() override { this->system_->stop(); }
};

// Runs start() on a new thread and reports whether it returns in time.
template <typename... Padds> bool startReturnsAfterSelfStop() {
  ScratchPadd::SystemImplementation<Padds...> system;
  system.instantiate();
  std::promise<void> returned;
  auto future = returned.get_future();
  std::thread mainThread([&] {
    system.start();
    returned.set_value();
  });
  const bool inTime = future.wait_for(2s) == std::future_status::ready;
  if (!inTime)
    system.stop(); // don't leave the thread running
  mainThread.join();
  return inTime;
}

} // namespace

// finishing() runs for every padd before any cleanup(); cleanup() runs on the
// thread that called start(); finishing() runs on each padd's own thread.
TEST(ThreadingTest, TestShutdownOrder) {
  using Main = RecordingPadd<true, 5>;
  using Background = RecordingPadd<false, 6>;
  hookLog.entries.clear();
  std::thread::id mainId;
  {
    RunningSystem<Main, Background> running;
    mainId = running.mainThread.get_id();
  } // stops the system

  std::lock_guard<std::mutex> lk(hookLog.mutex);
  ASSERT_EQ(hookLog.entries.size(), 4u);
  EXPECT_EQ(hookLog.entries[0].hook, "finishing");
  EXPECT_EQ(hookLog.entries[1].hook, "finishing");
  EXPECT_EQ(hookLog.entries[2].hook, "cleanup");
  EXPECT_EQ(hookLog.entries[3].hook, "cleanup");
  for (const auto &entry : hookLog.entries) {
    if (entry.hook == "cleanup" || entry.padd == "ThreadPadd5") {
      EXPECT_EQ(entry.thread, mainId) << entry.hook << " " << entry.padd;
    } else {
      EXPECT_NE(entry.thread, mainId) << entry.hook << " " << entry.padd;
    }
  }
}

// A padd can stop the system from inside itself, whichever thread it runs on.
TEST(ThreadingTest, TestStopFromInsideMainThreadPadd) {
  EXPECT_TRUE((startReturnsAfterSelfStop<SelfStoppingPadd<true, 7>, BackgroundA>()));
}

TEST(ThreadingTest, TestStopFromInsideBackgroundPadd) {
  EXPECT_TRUE((startReturnsAfterSelfStop<SelfStoppingPadd<false, 8>, MainA>()));
}

// Padds don't poll: with no work, their loops sleep instead of spinning.
TEST(ThreadingTest, IdlePaddsDontSpin) {
  RunningSystem<MainA, BackgroundA> running;
  std::this_thread::sleep_for(50ms); // let startup work finish
  const auto mainBefore = running.padd<MainA>().workLoopPasses();
  const auto backgroundBefore = running.padd<BackgroundA>().workLoopPasses();
  std::this_thread::sleep_for(200ms);
  EXPECT_EQ(running.padd<MainA>().workLoopPasses(), mainBefore);
  EXPECT_EQ(running.padd<BackgroundA>().workLoopPasses(), backgroundBefore);
}

// Work wakes a sleeping padd straight away, on either kind of thread.
TEST(ThreadingTest, WorkWakesAnIdlePaddPromptly) {
  RunningSystem<MainA, BackgroundA> running;
  std::this_thread::sleep_for(100ms); // both loops are asleep by now
  for (int i = 0; i < 2; ++i) {
    const auto start = std::chrono::steady_clock::now();
    i == 0 ? running.padd<MainA>().workThreadId() : running.padd<BackgroundA>().workThreadId();
    EXPECT_LT(std::chrono::steady_clock::now() - start, 50ms) << (i == 0 ? "main" : "background");
  }
}

// stop() wakes idle padds, so shutdown doesn't wait on a sleep.
TEST(ThreadingTest, StopIsPromptWhenIdle) {
  RunningSystem<MainA, BackgroundA> running;
  std::this_thread::sleep_for(100ms);
  const auto start = std::chrono::steady_clock::now();
  running.stop();
  EXPECT_LT(std::chrono::steady_clock::now() - start, 100ms);
}

// Many threads pushing to one padd at once: every job runs exactly once, and
// each sender's jobs run in the order it pushed them.
TEST(ThreadingTest, ConcurrentSendersLoseNothing) {
  RunningSystem<BackgroundA> running;
  auto &padd = running.padd<BackgroundA>();
  constexpr int kSenders = 8, kJobs = 1000;
  std::vector<std::vector<int>> seen(kSenders); // written only on the padd's thread
  std::vector<std::thread> senders;
  senders.reserve(kSenders);
  for (int sender = 0; sender < kSenders; ++sender) {
    senders.emplace_back([&, sender] {
      for (int job = 0; job < kJobs; ++job) {
        ScratchPadd::PaddTestAccess::push(padd,
                                          [&seen, sender, job] { seen[sender].push_back(job); });
      }
    });
  }
  for (std::thread &t : senders) {
    t.join();
  }
  padd.workThreadId(); // queued after everything above, so runs last
  for (int sender = 0; sender < kSenders; ++sender) {
    ASSERT_EQ(seen[sender].size(), static_cast<size_t>(kJobs)) << sender;
    for (int job = 0; job < kJobs; ++job) {
      EXPECT_EQ(seen[sender][job], job);
    }
  }
}
