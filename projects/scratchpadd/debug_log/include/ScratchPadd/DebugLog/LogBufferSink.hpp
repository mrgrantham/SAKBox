// LogBufferSink.hpp - an spdlog sink writing each line, formatted, into a
// LogBuffer (Session routes every ScratchPadd logger to one on a 3DS)
#pragma once

#include <ScratchPadd/DebugLog/LogBuffer.hpp>

#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string_view>

namespace ScratchPadd::DebugLog {

// An spdlog sink writing each line, formatted, into a LogBuffer
class LogBufferSink : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit LogBufferSink(LogBuffer &buffer) : buffer_(buffer) {}

protected:
  void sink_it_(const spdlog::details::log_msg &message) override {
    spdlog::memory_buf_t formatted;
    formatter_->format(message, formatted);
    buffer_.append(std::string_view(formatted.data(), formatted.size()));
  }
  void flush_() override {}

private:
  LogBuffer &buffer_;
};

} // namespace ScratchPadd::DebugLog
