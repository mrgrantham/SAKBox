// LogBuffer.hpp - the last part of a log, kept in memory for a reader to
// fetch (LogServer sends it over Wi-Fi): text appended, the oldest lines
// dropped past its capacity, read from where each reader left off. Plain
// C++: a program without ScratchPadd or spdlog can keep its results in one
// (the 3DS benchmark and the audio self-test do).
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace ScratchPadd::DebugLog {

class LogBuffer {
public:
  explicit LogBuffer(std::size_t capacity = 64 * 1024) : capacity_(capacity) {}

  void append(std::string_view text);
  // The text after `position` (how many characters had been appended when
  // the reader last read; 0 the first time), or from the oldest kept if
  // that's gone; `position` moved past it. With nothing new, waits up to
  // `wait` for some (not on a DS: its threads have no timed wait).
  std::string readFrom(std::uint64_t &position,
                       std::chrono::milliseconds wait = std::chrono::milliseconds(0));
  // How many characters have been appended, ever
  std::uint64_t end() const;
  // Called (on the appending thread, outside the buffer's lock) after each
  // append: a StreamService's wake(), so its loop sends what's new
  void setOnAppend(std::function<void()> onAppend);

private:
  mutable std::mutex mutex_;
  std::condition_variable appended_;
  std::size_t capacity_;
  std::string text_;          // what's kept
  std::uint64_t dropped_ = 0; // characters dropped from its front
  std::function<void()> onAppend_;
};

} // namespace ScratchPadd::DebugLog
