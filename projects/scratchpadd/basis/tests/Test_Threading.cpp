#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <future>
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
TEST(ThreadingTest, TesMixedlBackgroundAndMainThreads) {
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
