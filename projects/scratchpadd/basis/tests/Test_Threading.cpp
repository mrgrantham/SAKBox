#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <future>
#include <mutex>
#include <vector>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

namespace {

// A padd that can run a job on whichever thread its work queue is processed
// on and report back that thread's id.
template <bool UseMainThread, int Id> class ThreadPadd : public ScratchPadd::Base {
public:
  ThreadPadd(ScratchPadd::System *system) : Base(system) {
    work_thread_sleep_interval_ = 1ms; // keep the tests fast
  }
  std::string name() override { return "ThreadPadd" + std::to_string(Id); }
  bool shouldUseMainThread() override { return UseMainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void receive(ScratchPadd::Message message) override {}

  // Queues a job and returns the id of the thread that ran it.
  std::thread::id workThreadId() {
    auto promise = std::make_shared<std::promise<std::thread::id>>();
    auto future = promise->get_future();
    push(new std::function<void()>(
        [promise] { promise->set_value(std::this_thread::get_id()); }));
    EXPECT_EQ(future.wait_for(2s), std::future_status::ready)
        << name() << " never ran its work";
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
    std::thread starter([&] { padd.notifyStart(); });
    padd.waitForStart();
    EXPECT_TRUE(padd.isRunning());
    starter.join();

    std::thread stopper([&] { padd.notifyStop(); });
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

template <bool UseMainThread, int Id>
class RecordingPadd : public ThreadPadd<UseMainThread, Id> {
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

// A main-thread padd that sleeps for a long time between passes.
class SlowMainPadd : public ThreadPadd<true, 9> {
public:
  SlowMainPadd(ScratchPadd::System *system) : ThreadPadd(system) {
    work_thread_sleep_interval_ = 500ms;
  }
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

// Main-thread padds don't add up their sleeps: a fast padd sharing the loop
// with a slow one still responds at its own pace.
TEST(ThreadingTest, TestMainThreadSleepsDontStack) {
  RunningSystem<MainA, SlowMainPadd> running;
  // Let the loop settle into a sleep before measuring.
  std::this_thread::sleep_for(50ms);

  const auto start = std::chrono::steady_clock::now();
  running.padd<MainA>().workThreadId();
  EXPECT_LT(std::chrono::steady_clock::now() - start, 200ms);
}
