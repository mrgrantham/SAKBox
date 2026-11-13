// Runs and kDefaultRuns (Threads.hpp), checked twice: Runs_test is built as
// usual, with threads, and RunsUnthreaded_test with SCRATCHPADD_NO_THREADS,
// as for a platform without them (some bare-metal targets). With threads,
// each padd's thread also has the priority its Runs asks for.
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

// Whether the Runs that give a padd a thread of its own exist, so a padd
// returning one compiles
template <typename E>
concept HasOwnThreads = requires {
  E::Urgently;
  E::Steadily;
  E::WhenIdle;
};

// Without threads there's no thread of its own to ask for: a padd whose
// runs() returns one doesn't compile
static_assert(HasOwnThreads<ScratchPadd::Runs> == ScratchPadd::kHasThreads);
#if SCRATCHPADD_HAS_THREADS
static_assert(ScratchPadd::kDefaultRuns == ScratchPadd::Runs::Steadily);
#else
static_assert(ScratchPadd::kDefaultRuns == ScratchPadd::Runs::OnMainThread);
#endif

struct Ping {};

// Doesn't override runs(): it builds and runs either way
class DefaultPadd : public ScratchPadd::Base {
public:
  DefaultPadd(ScratchPadd::System *system) : Base(system) {
    receive<Ping>([this](const Ping &) {
      ranOn = std::this_thread::get_id();
      system_->stop();
    });
  }
  std::string name() override { return "DefaultPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::thread::id ranOn;
};

class Sender : public ScratchPadd::Base {
public:
  Sender(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Sender"; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void starting() override { send(Ping{}); }
};

} // namespace

// A padd that doesn't override runs() gets a thread of its own where there
// are threads, and runs on the thread that called start() where there aren't
TEST(RunsTest, DefaultSuitsThePlatform) {
  ScratchPadd::SystemImplementation<Sender, DefaultPadd> system;
  system.instantiate();
  system.start(); // returns when DefaultPadd stops it
  const auto &padd = *std::get<std::unique_ptr<DefaultPadd>>(system.getWorkers());
  if (ScratchPadd::kHasThreads) {
    EXPECT_NE(padd.ranOn, std::this_thread::get_id());
  } else {
    EXPECT_EQ(padd.ranOn, std::this_thread::get_id());
  }
}

#if SCRATCHPADD_HAS_THREADS
namespace {

// What a thread set to `priority` has, as the platform describes it
std::string priorityOfAThreadSetTo(ScratchPadd::Platform::ThreadPriority priority) {
  std::string described;
  std::thread([&] {
    ScratchPadd::Platform::setThisThreadPriority(priority);
    described = ScratchPadd::Platform::describeThisThreadPriority();
  }).join();
  return described;
}

// How many ReportingPadds have reported (one count for all three kinds)
std::atomic<int> paddsReported{0};

// Reports its thread's priority, then stops the system once all three have
template <ScratchPadd::Runs How> class ReportingPadd : public ScratchPadd::Base {
public:
  ReportingPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return std::string("Runs ") + ScratchPadd::describe(How); }
  ScratchPadd::Runs runs() override { return How; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void starting() override {
    priority = ScratchPadd::Platform::describeThisThreadPriority();
    if (++paddsReported == 3)
      system_->stop();
  }
  std::string priority;
};

} // namespace

// Each padd's thread has the priority its Runs asks for: the one a thread
// set to it has (ThreadPriority_test checks those against each platform)
TEST(RunsTest, EachPaddsThreadHasItsPriority) {
  using ScratchPadd::Runs;
  using ScratchPadd::Platform::ThreadPriority;
  ScratchPadd::SystemImplementation<ReportingPadd<Runs::Urgently>, ReportingPadd<Runs::Steadily>,
                                    ReportingPadd<Runs::WhenIdle>>
      system;
  system.instantiate();
  system.start(); // returns when the third has reported
  const auto &workers = system.getWorkers();
  EXPECT_EQ(std::get<0>(workers)->priority, priorityOfAThreadSetTo(ThreadPriority::High));
  EXPECT_EQ(std::get<1>(workers)->priority, priorityOfAThreadSetTo(ThreadPriority::Normal));
  EXPECT_EQ(std::get<2>(workers)->priority, priorityOfAThreadSetTo(ThreadPriority::Low));
}
#endif
