// Shared state (SharedState.hpp): padds publishing a current value with
// publishState() and reading it with currentState(), across threads.
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

namespace {

struct Pad {
  int buttons = 0;
  float stick = 0;
};
struct Pose {
  float x = 0;
};

// A padd that runs a job on its own thread (where publishState() and
// currentState() are called) and returns the job's result
template <ScratchPadd::RunOn Thread, int Id> class JobPadd : public ScratchPadd::Base {
public:
  JobPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "JobPadd" + std::to_string(Id); }
  ScratchPadd::RunOn runOn() override { return Thread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  template <typename Job> auto run(Job job) {
    using Result = decltype(job(*this));
    auto promise = std::make_shared<std::promise<Result>>();
    push([this, promise, job] { promise->set_value(job(*this)); });
    auto future = promise->get_future();
    EXPECT_EQ(future.wait_for(5s), std::future_status::ready) << name() << " never ran its job";
    return future.get();
  }

  // (protected in Base: only a padd's own code calls them)
  template <typename T> bool publish(const T &value, std::string_view name = {}) {
    return publishState(value, name);
  }
  template <typename T> ScratchPadd::State<T> current(std::string_view name = {}) {
    return currentState<T>(name);
  }
};

using Publisher = JobPadd<ScratchPadd::RunOn::BackgroundThread, 1>;
using Reader = JobPadd<ScratchPadd::RunOn::BackgroundThread, 2>;
using MainReader = JobPadd<ScratchPadd::RunOn::MainThread, 3>;

// Runs a system on its own thread, standing in for the app's main thread
template <typename... Padds> class RunningSystem {
public:
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread mainThread;

  RunningSystem() {
    system.instantiate();
    mainThread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~RunningSystem() {
    system.stop();
    mainThread.join();
  }
  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
};

} // namespace

// Nothing published yet: a default value at version 0. Each publish is one
// more version, and a reader gets the newest, however many it missed.
TEST(SharedStateTest, ReadersGetTheNewestValue) {
  RunningSystem<Publisher, Reader, MainReader> running;
  auto &publisher = running.padd<Publisher>();
  auto &reader = running.padd<Reader>();

  const auto before = reader.run([](Reader &r) { return r.current<Pad>(); });
  EXPECT_EQ(before.version, 0u);
  EXPECT_EQ(before.value.buttons, 0);

  publisher.run([](Publisher &p) {
    for (int i = 1; i <= 3; ++i)
      EXPECT_TRUE(p.publish(Pad{.buttons = i, .stick = i * 0.5f}));
    return 0;
  });
  const auto after = reader.run([](Reader &r) { return r.current<Pad>(); });
  EXPECT_EQ(after.version, 3u);
  EXPECT_EQ(after.value.buttons, 3);
  EXPECT_EQ(after.value.stick, 1.5f);
  // A main-thread padd reads the same
  const auto onMain =
      running.padd<MainReader>().run([](MainReader &r) { return r.current<Pad>(); });
  EXPECT_EQ(onMain.version, 3u);
  EXPECT_EQ(onMain.value.buttons, 3);
}

// Each type, and each name within a type, is its own state.
TEST(SharedStateTest, TypesAndNamesAreSeparate) {
  RunningSystem<Publisher, Reader> running;
  running.padd<Publisher>().run([](Publisher &p) {
    p.publish(Pose{.x = 1});
    p.publish(Pose{.x = 2}, "ragdoll");
    p.publish(Pose{.x = 3}, "ragdoll");
    return 0;
  });
  auto &reader = running.padd<Reader>();
  EXPECT_EQ(reader.run([](Reader &r) { return r.current<Pose>(); }).value.x, 1);
  const auto ragdoll = reader.run([](Reader &r) { return r.current<Pose>("ragdoll"); });
  EXPECT_EQ(ragdoll.value.x, 3);
  EXPECT_EQ(ragdoll.version, 2u);
  EXPECT_EQ(reader.run([](Reader &r) { return r.current<Pad>(); }).version, 0u);
  EXPECT_EQ(reader.run([](Reader &r) { return r.current<Pose>("other"); }).version, 0u);
}

// One padd publishes each state: the first to. Another's publish is refused
// and changes nothing; the same type under another name is its to publish.
TEST(SharedStateTest, OnePublisherEach) {
  RunningSystem<Publisher, Reader> running;
  EXPECT_TRUE(running.padd<Publisher>().run([](Publisher &p) { return p.publish(Pose{.x = 1}); }));
  auto &other = running.padd<Reader>();
  EXPECT_FALSE(other.run([](Reader &r) { return r.publish(Pose{.x = 9}); }));
  EXPECT_FALSE(other.run([](Reader &r) { return r.publish(Pose{.x = 9}); }));
  EXPECT_TRUE(other.run([](Reader &r) { return r.publish(Pose{.x = 5}, "mine"); }));
  const auto pose = other.run([](Reader &r) { return r.current<Pose>(); });
  EXPECT_EQ(pose.value.x, 1);
  EXPECT_EQ(pose.version, 1u);
}

// Publishing and reading at once on two threads: a reader never sees a
// value half written (every element of one publish), and versions only go
// up. (Run under ThreadSanitizer too.)
TEST(SharedStateTest, ReadersNeverSeeHalfAValue) {
  struct Big {
    std::array<int, 64> numbers{};
  };
  RunningSystem<Publisher, Reader> running;
  constexpr int kPublishes = 20000;
  std::atomic<bool> publishing{true};
  auto published = std::async(std::launch::async, [&] {
    running.padd<Publisher>().run([&](Publisher &p) {
      for (int i = 1; i <= kPublishes; ++i) {
        Big big;
        big.numbers.fill(i);
        p.publish(big);
      }
      publishing = false;
      return 0;
    });
  });
  const int torn = running.padd<Reader>().run([&](Reader &r) {
    int torn = 0;
    std::uint64_t last = 0;
    do {
      const auto big = r.current<Big>();
      for (int n : big.value.numbers)
        torn += n != big.value.numbers[0];
      torn += big.version < last;
      last = big.version;
    } while (publishing);
    return torn;
  });
  published.get();
  EXPECT_EQ(torn, 0);
  EXPECT_EQ(running.padd<Reader>().run([](Reader &r) { return r.current<Big>(); }).version,
            static_cast<std::uint64_t>(kPublishes));
}

// Only a padd's own thread may publish or read through it (as with its
// controls); another thread is told to send it a message instead.
TEST(SharedStateTest, OnlyOnThePaddsThread) {
  RunningSystem<Publisher, Reader> running;
  EXPECT_THROW(running.padd<Reader>().current<Pad>(), std::logic_error);
  EXPECT_THROW(running.padd<Publisher>().publish(Pad{}), std::logic_error);
}
