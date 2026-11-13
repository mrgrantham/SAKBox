#include <ScratchPadd/DebugLog/Session.hpp>

#include <ScratchPadd/Logger.hpp>

#if defined(__3DS__)
#include <ScratchPadd/DebugLog/LogBufferSink.hpp>
#include <ScratchPadd/DebugLog/LogServer.hpp>
#elif defined(__NDS__)
#include <ScratchPadd/DebugLog/CardLog.hpp>
#include <fat.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>
#include <sys/stat.h>

#include <mutex>
#endif

namespace ScratchPadd::DebugLog {

#if defined(__3DS__)
struct Session::Platform {
  LogBuffer buffer;
  LogServer server{buffer};
};
#elif defined(__NDS__)
struct Session::Platform {
  std::unique_ptr<CardLog> log;
};
#else
struct Session::Platform {};
#endif

#if defined(__NDS__)
namespace {
// Every logger's lines, formatted, into the CardLog (which writes them to
// the card in batches); warnings and errors written at once
class CardLogSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit CardLogSink(CardLog &log) : log_(log) {}

protected:
  void sink_it_(const spdlog::details::log_msg &message) override {
    spdlog::memory_buf_t line;
    formatter_->format(message, line);
    log_.add(std::string_view(line.data(), line.size()), message.level >= spdlog::level::warn);
  }
  void flush_() override { log_.write(); }

private:
  CardLog &log_;
};
} // namespace
#endif

Session::Session(const std::string &appName, SessionOptions options)
    : platform_(std::make_unique<Platform>()) {
#if defined(__3DS__)
  if (!platform_->server.listening()) {
    where_ = "off: " + platform_->server.error();
    return;
  }
  auto sink = std::make_shared<LogBufferSink>(platform_->buffer);
  sink->set_formatter(Logger::lineFormatter()); // (as the terminal's)
  Logger::routeTo({sink}, false);
  where_ = platform_->server.address();
#elif defined(__NDS__)
  // (libfat: the flashcart's card, through its DLDI driver)
  if (!fatInitDefault()) {
    where_ = "off: no SD card";
    return;
  }
  ::mkdir("fat:/logs", 0777);
  const std::string path = "fat:/logs/" + appName + ".log";
  // (the log's own lines, its figures, formatted as a "DebugLog" logger's)
  auto formatLine = [formatter = std::shared_ptr<spdlog::formatter>(Logger::lineFormatter())](
                        std::string_view text) {
    spdlog::memory_buf_t line;
    formatter->format(spdlog::details::log_msg("DebugLog", spdlog::level::info, text), line);
    return std::string(line.data(), line.size());
  };
  std::string error;
  platform_->log = CardLog::open(path, options.cardWriteEvery, formatLine, error);
  if (!platform_->log) {
    where_ = "off: " + error;
    return;
  }
  CardLog::setActive(platform_->log.get()); // (for WriterPadd)
  auto sink = std::make_shared<CardLogSink>(*platform_->log);
  sink->set_formatter(Logger::lineFormatter()); // (as the terminal's)
  Logger::routeTo({sink}, false);
  where_ = path;
#else
  (void)appName;
  where_ = "the terminal";
#endif
  spdlog::set_level(options.level);
  PADDLOG_INFO("{}: logging to {}", appName, where_);
}

Session::~Session() {
  spdlog::set_level(spdlog::level::info);
  Logger::routeTo({}, true);
}

} // namespace ScratchPadd::DebugLog
