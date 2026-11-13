// WriterPadd in a running system: lines added to the active CardLog are
// written by the padd, on its own thread, when the doorbell rings; it
// publishes the figures; and without a CardLog it does nothing.
#include <ScratchPadd/DebugLog/WriterPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;
using ScratchPadd::DebugLog::CardLog;
using ScratchPadd::DebugLog::WriterPadd;

namespace {

std::string contentsOf(const std::filesystem::path &path) {
  std::ifstream file(path);
  std::stringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

// Waits up to 2 s for `done`
template <typename Done> bool eventually(Done done) {
  for (auto until = std::chrono::steady_clock::now() + 2s; std::chrono::steady_clock::now() < until;
       std::this_thread::sleep_for(5ms)) {
    if (done()) {
      return true;
    }
  }
  return done();
}

} // namespace

TEST(WriterPaddTest, WritesTheActiveLogWhenRung) {
  const auto path = std::filesystem::path(::testing::TempDir()) / "WriterPadd.log";
  std::string error;
  auto log = CardLog::open(path.string(), 2, nullptr, error);
  ASSERT_TRUE(log) << error;
  CardLog::setActive(log.get());

  ScratchPadd::SystemImplementation<WriterPadd> system;
  system.instantiate();
  std::thread running([&] { system.start(); });
  system.waitForStart();

  log->add("one\n", false);
  EXPECT_EQ(contentsOf(path), ""); // (waiting for a second line)
  log->add("two\n", false);        // rings the padd
  EXPECT_TRUE(eventually([&] { return contentsOf(path) == "one\ntwo\n"; })) << contentsOf(path);
  log->add("warning\n", true);
  EXPECT_TRUE(eventually([&] { return contentsOf(path) == "one\ntwo\nwarning\n"; }));

  system.stop();
  running.join();
  log->add("after\n", true); // stopped: the log writes for itself again
  EXPECT_EQ(contentsOf(path), "one\ntwo\nwarning\nafter\n");
  CardLog::setActive(nullptr);
}

// Without a CardLog (a release build, a desktop) it has no timer or controls
TEST(WriterPaddTest, IdleWithoutALog) {
  CardLog::setActive(nullptr);
  ScratchPadd::SystemImplementation<WriterPadd> system;
  system.instantiate();
  auto &padd = *std::get<std::unique_ptr<WriterPadd>>(system.getWorkers());
  EXPECT_FALSE(padd.repeatInterval());
  EXPECT_TRUE(padd.generateControls().empty());
}
