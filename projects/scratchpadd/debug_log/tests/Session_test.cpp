// Session on a desktop: every ScratchPadd logger's lines in a file in the
// app's log folder (named after the app), as well as the terminal; the file
// registered as a Storage source while the session lasts; the runs before
// kept; and in a debug build, the log served over the network on the first
// free port from 5005, as a 3DS's is.
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/Network/Loop.hpp>
#include <ScratchPadd/Network/Service.hpp>
#include <ScratchPadd/Network/Sockets.hpp>
#include <ScratchPadd/Storage/Source.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace {

std::string contentsOf(const std::string &path) {
  std::ifstream file(path);
  std::stringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

// (Not served: these tests are about the file)
ScratchPadd::DebugLog::SessionOptions inTestFolder() {
  return {.folders = ScratchPadd::Storage::Folders::under(::testing::TempDir() + "/session"),
          .serve = false};
}

ScratchPadd::DebugLog::SessionOptions servedFromTestFolder() {
  ScratchPadd::DebugLog::SessionOptions options = inTestFolder();
  options.serve = true;
  return options;
}

std::uint16_t portIn(const std::string &address) {
  return static_cast<std::uint16_t>(std::stoi(address.substr(address.rfind(':') + 1)));
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

// A debug build serves its log by default; a release build doesn't
TEST(SessionTest, ServedByDefaultInADebugBuild) {
#if defined(SAKBOX_DEBUG) || !defined(NDEBUG)
  EXPECT_TRUE(ScratchPadd::DebugLog::kServeByDefault);
#else
  EXPECT_FALSE(ScratchPadd::DebugLog::kServeByDefault);
#endif
  ScratchPadd::DebugLog::Session log(inTestFolder());
  EXPECT_EQ(log.servedAt(), ""); // (asked not to)
}

// With 5005 taken (another app serving its log), it takes the next free port
TEST(SessionTest, ServedOnTheFirstFreePort) {
  std::string error;
  ScratchPadd::Network::Socket taken =
      ScratchPadd::Network::Socket::listenTcp(ScratchPadd::DebugLog::Session::kLogPort, error);
  ScratchPadd::DebugLog::Session log(servedFromTestFolder());
  ASSERT_NE(log.servedAt(), "") << log.where();
  const std::uint16_t port = portIn(log.servedAt());
  EXPECT_GE(port, ScratchPadd::DebugLog::Session::kLogPort);
  EXPECT_LE(port,
            ScratchPadd::DebugLog::Session::kLogPort + ScratchPadd::DebugLog::Session::kLogPorts);
  EXPECT_NE(port, ScratchPadd::DebugLog::Session::kSkippedPort); // RemoteControlsPadd's
  if (taken.valid()) {
    EXPECT_NE(port, ScratchPadd::DebugLog::Session::kLogPort);
  }
  EXPECT_NE(log.where().find(log.servedAt()), std::string::npos);
  EXPECT_EQ(ScratchPadd::Network::services().all().size(), 1u); // for NetworkPadd
}

// What NetworkPadd runs serves the log: what came before, then each line
TEST(SessionTest, AReaderGetsTheLog) {
  using namespace std::chrono_literals;
  ScratchPadd::DebugLog::Session log(servedFromTestFolder());
  ASSERT_NE(log.servedAt(), "") << log.where();
  ScratchPadd::Logger::loggerNamed("SessionTest")->info("before anyone read");
  ScratchPadd::Network::Loop loop; // (NetworkPadd's loop)
  std::string error;
  ASSERT_TRUE(loop.start(error)) << error;
  ASSERT_EQ(ScratchPadd::Network::services().all().size(), 1u);
  loop.attach(ScratchPadd::Network::services().all().front());
  ScratchPadd::Network::Socket reader;
  for (int i = 0; i < 100 && !reader.valid(); ++i) {
    reader = ScratchPadd::Network::Socket::connectTcp("127.0.0.1", portIn(log.servedAt()), error);
    if (!reader.valid())
      std::this_thread::sleep_for(20ms);
  }
  ASSERT_TRUE(reader.valid()) << error;
  ScratchPadd::Logger::loggerNamed("SessionTest")->info("while reading");
  std::string text;
  char chunk[4096];
  for (auto until = std::chrono::steady_clock::now() + 3s;
       std::chrono::steady_clock::now() < until &&
       text.find("while reading") == std::string::npos;) {
    const long got = reader.receive(chunk);
    if (got > 0)
      text.append(chunk, static_cast<std::size_t>(got));
    else
      std::this_thread::sleep_for(10ms);
  }
  EXPECT_NE(text.find("before anyone read"), std::string::npos) << text;
  EXPECT_NE(text.find("while reading"), std::string::npos) << text;
  loop.detach(ScratchPadd::Network::services().all().front());
}
