// AppendFile: pieces written in batches, by itself or by StoragePadd when
// it rings its doorbell; urgent pieces, and ones that waited too long,
// written at once by the thread adding them; the runs before kept; the
// figures in the file.
#include <ScratchPadd/Storage/AppendFile.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

using ScratchPadd::Storage::AppendFile;

namespace {

class AppendFileTest : public ::testing::Test {
protected:
  std::filesystem::path folder_ = std::filesystem::path(::testing::TempDir()) /
                                  ::testing::UnitTest::GetInstance()->current_test_info()->name();
  std::string path_ = (folder_ / "logs" / "app.log").string();

  std::unique_ptr<AppendFile> open(unsigned writeEvery, unsigned keepRuns = 5) {
    std::string error;
    auto file = AppendFile::open(
        path_,
        {.name = "log",
         .writeEvery = writeEvery,
         .keepRuns = keepRuns,
         .formatLine = [](std::string_view text) { return "[log] " + std::string(text) + "\n"; }},
        error);
    EXPECT_TRUE(file) << error;
    return file;
  }
  static std::string contentsOf(const std::string &path) {
    std::ifstream file(path);
    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
  }
  std::string written() const { return contentsOf(path_); }
};

} // namespace

// With nothing attached, pieces are written in batches as they're added
// (its folder made), urgent ones at once
TEST_F(AppendFileTest, WritesItsOwnBatchesWithNothingAttached) {
  auto file = open(3);
  file->add("one\n");
  file->add("two\n");
  EXPECT_EQ(written(), "");
  EXPECT_EQ(file->figures().waiting, 2u);
  file->add("three\n");
  EXPECT_EQ(written(), "one\ntwo\nthree\n");
  EXPECT_EQ(file->figures().syncs, 1u);
  file->add("warning\n", /*urgent=*/true);
  EXPECT_EQ(written(), "one\ntwo\nthree\nwarning\n");
}

// Attached, it rings the doorbell instead (once until written) and leaves
// the writing to whoever answers; an urgent piece is still written at once
// by the thread adding it, counted as such
TEST_F(AppendFileTest, RingsWhenAttachedAndWritesUrgentPiecesItself) {
  auto file = open(2);
  int rings = 0;
  file->attach([&] { ++rings; });
  file->add("one\n");
  EXPECT_EQ(rings, 0);
  file->add("two\n");
  EXPECT_EQ(rings, 1);
  file->add("three\n"); // rung already
  EXPECT_EQ(rings, 1);
  EXPECT_EQ(written(), "");
  file->write(); // (StoragePadd, answering)
  EXPECT_EQ(written(), "one\ntwo\nthree\n");
  EXPECT_EQ(file->figures().syncs, 1u);
  file->add("warning\n", /*urgent=*/true);
  EXPECT_EQ(written(), "one\ntwo\nthree\nwarning\n");
  EXPECT_EQ(rings, 1);
  EXPECT_EQ(file->figures().written, 1u);
  file->detach();
  file->add("after\n");
  file->add("and\n");
  EXPECT_EQ(rings, 1); // detached: no more rings, its own batches
  EXPECT_EQ(written(), "one\ntwo\nthree\nwarning\nafter\nand\n");
}

// A piece added when the oldest has waited kLongestWait is written then by
// the thread adding it: a busy app's StoragePadd may not get to run
TEST_F(AppendFileTest, WritesItselfWhenStoragePaddCantGetToIt) {
  auto file = open(100);
  file->attach([] {}); // (a doorbell nobody answers)
  file->add("one\n");
  std::this_thread::sleep_for(AppendFile::kLongestWait + std::chrono::milliseconds(50));
  EXPECT_EQ(written(), "");
  file->add("two\n");
  EXPECT_EQ(written(), "one\ntwo\n");
}

// Each opening starts the file afresh and keeps the runs before, the
// newest as .1, up to keepRuns
TEST_F(AppendFileTest, KeepsTheRunsBefore) {
  for (const char *run : {"first\n", "second\n", "third\n", "fourth\n"}) {
    auto file = open(1, /*keepRuns=*/2);
    file->add(run, /*urgent=*/true);
    file->detach();
    file.reset();
  }
  EXPECT_EQ(written().rfind("fourth\n", 0), 0u);
  EXPECT_EQ(contentsOf(AppendFile::runPath(path_, 1)).rfind("third\n", 0), 0u);
  EXPECT_EQ(contentsOf(AppendFile::runPath(path_, 2)).rfind("second\n", 0), 0u);
  EXPECT_FALSE(std::filesystem::exists(AppendFile::runPath(path_, 3)));
  EXPECT_EQ(AppendFile::runPath("logs/app.log", 2), "logs/app.2.log");
  EXPECT_EQ(AppendFile::runPath("logs.d/recording", 1), "logs.d/recording.1");
}

// Closing writes what's waiting, then the figures, as a line of its own
TEST_F(AppendFileTest, ClosingWritesTheRestAndTheFigures) {
  auto file = open(10);
  file->add("one\n");
  file.reset();
  const std::string contents = written();
  EXPECT_EQ(contents.rfind("one\n[log] 1 syncs so far: ", 0), 0u) << contents;
  EXPECT_NE(contents.find(" waited "), std::string::npos) << contents;
}

// As a Source: its name, where it writes, a line of figures, and the
// pieces between writes, changed while it runs (1 at least)
TEST_F(AppendFileTest, IsASource) {
  auto file = open(5);
  ScratchPadd::Storage::Source &source = *file;
  EXPECT_EQ(source.name(), "log");
  EXPECT_EQ(source.where(), path_);
  EXPECT_EQ(source.writeEvery(), 5u);
  source.setWriteEvery(0);
  EXPECT_EQ(source.writeEvery(), 1u);
  file->add("one\n");
  EXPECT_EQ(source.figuresLine().rfind("1 syncs: ", 0), 0u) << source.figuresLine();
}
