// LogBuffer: text kept and read from where each reader left off, the
// oldest lines dropped past its capacity, a late reader getting what's
// kept; and spdlog's lines through LogBufferSink, routed by the Logger.
#include <ScratchPadd/DebugLog/LogBufferSink.hpp>
#include <ScratchPadd/Logger.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using ScratchPadd::DebugLog::LogBuffer;
using namespace std::chrono_literals;

TEST(LogBufferTest, ReadersGetWhatsNewSinceTheirLastRead) {
  LogBuffer buffer;
  std::uint64_t first = 0, second = 0;
  buffer.append("one\n");
  EXPECT_EQ(buffer.readFrom(first), "one\n");
  buffer.append("two\n");
  EXPECT_EQ(buffer.readFrom(first), "two\n");
  EXPECT_EQ(buffer.readFrom(first), "");
  EXPECT_EQ(buffer.readFrom(second), "one\ntwo\n"); // each reader its own place
  EXPECT_EQ(buffer.end(), 8u);
}

// Past its capacity the oldest whole lines go; a reader that fell behind
// gets from the oldest kept
TEST(LogBufferTest, DropsTheOldestLines) {
  LogBuffer buffer(40);
  std::uint64_t slow = 0;
  for (int i = 0; i < 20; ++i)
    buffer.append("line " + std::to_string(i) + "\n");
  const std::string kept = buffer.readFrom(slow);
  EXPECT_LE(kept.size(), 40u);
  EXPECT_EQ(kept.rfind("line 19\n"), kept.size() - 8);
  EXPECT_TRUE(kept.starts_with("line ")); // whole lines only
  EXPECT_EQ(slow, buffer.end());
}

// A reader with nothing new waits for the next line, up to its limit
TEST(LogBufferTest, WaitsForTheNextLine) {
  LogBuffer buffer;
  std::uint64_t position = 0;
  std::thread writer([&] {
    std::this_thread::sleep_for(20ms);
    buffer.append("late\n");
  });
  EXPECT_EQ(buffer.readFrom(position, 2000ms), "late\n");
  writer.join();
  const auto start = std::chrono::steady_clock::now();
  EXPECT_EQ(buffer.readFrom(position, 30ms), "");
  EXPECT_GE(std::chrono::steady_clock::now() - start, 25ms);
}

// Routed there, every ScratchPadd logger's lines go into the buffer (and
// not the terminal); routed back, they don't
TEST(LogBufferTest, LoggersRoutedToIt) {
  LogBuffer buffer;
  auto sink = std::make_shared<ScratchPadd::DebugLog::LogBufferSink>(buffer);
  sink->set_pattern("[%n] %v");
  const auto earlier = ScratchPadd::Logger::loggerNamed("MadeEarlier");
  ScratchPadd::Logger::routeTo({sink}, false);
  earlier->info("from before");
  ScratchPadd::Logger::loggerNamed("MadeAfter")->info("from after");
  ScratchPadd::Logger::routeTo({}, true);
  earlier->info("not routed");
  std::uint64_t position = 0;
  EXPECT_EQ(buffer.readFrom(position), "[MadeEarlier] from before\n[MadeAfter] from after\n");
}
