// CardLog: lines written in batches, by itself or by an attached writer
// whose doorbell it rings; warnings at once; the figures in the file.
#include <ScratchPadd/DebugLog/CardLog.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using ScratchPadd::DebugLog::CardLog;

namespace {

class CardLogTest : public ::testing::Test {
protected:
  std::filesystem::path path_ =
      std::filesystem::path(::testing::TempDir()) /
      (std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + ".log");

  std::unique_ptr<CardLog> open(unsigned writeEvery) {
    std::string error;
    auto log = CardLog::open(
        path_.string(), writeEvery,
        [](std::string_view text) { return "[DebugLog] " + std::string(text) + "\n"; }, error);
    EXPECT_TRUE(log) << error;
    return log;
  }
  // What's in the file now
  std::string written() const {
    std::ifstream file(path_);
    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
  }
};

} // namespace

// With no writer attached, the lines are written in batches as they're added
TEST_F(CardLogTest, WritesItsOwnBatchesWithoutAWriter) {
  auto log = open(3);
  log->add("one\n", false);
  log->add("two\n", false);
  EXPECT_EQ(written(), "");
  EXPECT_EQ(log->figures().linesWaiting, 2u);
  log->add("three\n", false);
  EXPECT_EQ(written(), "one\ntwo\nthree\n");
  EXPECT_EQ(log->figures().syncs, 1u);
  log->add("warning\n", true); // at once
  EXPECT_EQ(written(), "one\ntwo\nthree\nwarning\n");
}

// Attached, it rings the writer's doorbell instead (once until it writes),
// and writes nothing itself
TEST_F(CardLogTest, RingsAnAttachedWriter) {
  auto log = open(2);
  int rings = 0;
  log->attach([&] { ++rings; });
  log->add("one\n", false);
  EXPECT_EQ(rings, 0);
  log->add("two\n", false);
  EXPECT_EQ(rings, 1);
  log->add("three\n", false); // rung already, not written yet
  EXPECT_EQ(rings, 1);
  EXPECT_EQ(written(), "");
  log->write(); // (the writer, woken)
  EXPECT_EQ(written(), "one\ntwo\nthree\n");
  EXPECT_EQ(log->figures().syncs, 1u); // one sync for all three
  log->add("warning\n", true);
  EXPECT_EQ(rings, 2);
  log->detach();
  log->add("after\n", true); // detached: written as it's added
  EXPECT_EQ(rings, 2);
  // (the warning was waiting for the writer, so it's written with "after")
  EXPECT_EQ(written(), "one\ntwo\nthree\nwarning\nafter\n");
}

// Closing writes what's waiting, then the figures, as the log's own line
TEST_F(CardLogTest, ClosingWritesTheRestAndTheFigures) {
  auto log = open(10);
  log->add("one\n", false);
  log.reset();
  const std::string contents = written();
  EXPECT_EQ(contents.rfind("one\n[DebugLog] 1 card syncs so far: ", 0), 0u) << contents;
  EXPECT_NE(contents.find(" ms at worst\n"), std::string::npos) << contents;
}

// The lines between writes can change while it runs
TEST_F(CardLogTest, WriteEveryChanges) {
  auto log = open(5);
  log->setWriteEvery(1);
  EXPECT_EQ(log->writeEvery(), 1u);
  log->add("one\n", false);
  EXPECT_EQ(written(), "one\n");
  log->setWriteEvery(0); // (1 at least)
  EXPECT_EQ(log->writeEvery(), 1u);
}
