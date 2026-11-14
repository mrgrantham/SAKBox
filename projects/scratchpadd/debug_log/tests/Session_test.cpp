// Session on a desktop: every ScratchPadd logger's lines in a file in the
// app's log folder (named after the app), as well as the terminal; the file
// registered as a Storage source while the session lasts; the runs before
// kept.
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/Storage/Source.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

std::string contentsOf(const std::string &path) {
  std::ifstream file(path);
  std::stringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

ScratchPadd::DebugLog::SessionOptions inTestFolder() {
  return {.folders = ScratchPadd::Storage::Folders::under(::testing::TempDir() + "/session")};
}

} // namespace

TEST(SessionTest, LogsToAFileNamedAfterTheApp) {
  std::string path;
  {
    ScratchPadd::DebugLog::Session log(inTestFolder());
    path = log.file();
    EXPECT_EQ(path, ::testing::TempDir() + "/session/logs/Session_test.log");
    EXPECT_EQ(log.where(), path);
    EXPECT_EQ(ScratchPadd::Storage::sources().all().size(), 1u);
    ScratchPadd::Logger::loggerNamed("SessionTest")->info("one line");
    ScratchPadd::Logger::loggerNamed("SessionTest")->warn("a warning");
    // (written as they're logged: no StoragePadd here, a batch of one)
    const std::string written = contentsOf(path);
    EXPECT_NE(written.find("Session_test (Session_test, "), std::string::npos) << written;
    EXPECT_NE(written.find("[SessionTest]        one line"), std::string::npos) << written;
    EXPECT_NE(written.find("warning  [SessionTest]        a warning"), std::string::npos)
        << written;
  }
  EXPECT_TRUE(ScratchPadd::Storage::sources().all().empty());

  // The next run's log starts afresh, the last kept beside it
  {
    ScratchPadd::DebugLog::Session log(inTestFolder());
    ScratchPadd::Logger::loggerNamed("SessionTest")->info("second run");
  }
  EXPECT_EQ(contentsOf(path).find("one line"), std::string::npos);
  EXPECT_NE(contentsOf(path).find("second run"), std::string::npos);
  EXPECT_NE(contentsOf(::testing::TempDir() + "/session/logs/Session_test.1.log").find("one line"),
            std::string::npos);
}
