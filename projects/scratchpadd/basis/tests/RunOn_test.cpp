// RunOn and kDefaultRunOn (Threads.hpp), checked twice: RunOn_test is built
// as usual, with threads, and RunOnUnthreaded_test with
// SCRATCHPADD_NO_THREADS, as for a platform without them (the Nintendo DS).
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

namespace {

// Whether RunOn::BackgroundThread exists, so a padd returning it compiles
template <typename E>
concept HasBackgroundThread = requires { E::BackgroundThread; };

// Without threads there's no BackgroundThread to ask for: a padd whose
// runOn() returns it doesn't compile
static_assert(HasBackgroundThread<ScratchPadd::RunOn> == ScratchPadd::kHasThreads);
#if SCRATCHPADD_HAS_THREADS
static_assert(ScratchPadd::kDefaultRunOn == ScratchPadd::RunOn::BackgroundThread);
#else
static_assert(ScratchPadd::kDefaultRunOn == ScratchPadd::RunOn::MainThread);
#endif

struct Ping {};

// Doesn't override runOn(): it builds and runs either way
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
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void starting() override { send(Ping{}); }
};

} // namespace

// A padd that doesn't override runOn() gets a background thread where there
// are threads, and runs on the thread that called start() where there aren't
TEST(RunOnTest, DefaultSuitsThePlatform) {
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
