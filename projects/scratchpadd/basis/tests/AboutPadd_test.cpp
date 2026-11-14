// AboutPadd: the app's facts and each padd's way of running, on its tab as
// information (Info controls), once the padds are set up.
#include <ScratchPadd/AboutPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <string>
#include <thread>

namespace {
class WorkerPadd : public ScratchPadd::Base {
public:
  explicit WorkerPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Worker"; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::WhenIdle; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};
} // namespace

TEST(AboutPaddTest, ShowsTheAppAndHowEachPaddRuns) {
  ScratchPadd::SystemImplementation<ScratchPadd::AboutPadd, WorkerPadd> system;
  system.instantiate();
  std::thread running([&] { system.start(); });
  system.waitForStart();
  auto &about = *std::get<std::unique_ptr<ScratchPadd::AboutPadd>>(system.getWorkers());
  auto info = [&](const std::string &name) {
    return ScratchPadd::onPaddThread(about, [&](ScratchPadd::AboutPadd &padd) {
      return ScratchPadd::PaddTestAccess::control<ScratchPadd::ControlType::Info>(padd, name).value;
    });
  };
  EXPECT_EQ(info("App name"), "AboutPadd_test");
  EXPECT_EQ(info("App"), "AboutPadd_test"); // (no title given: the name)
  EXPECT_FALSE(info("Platform").empty());
  EXPECT_EQ(info("Padds"), "Padd\tRuns\nAbout\tOnMainThread\nWorker\tWhenIdle");
  EXPECT_EQ(system.padds().size(), 2u);
  system.stop();
  running.join();
}
