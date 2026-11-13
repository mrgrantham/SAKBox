#include <ScratchPadd/DebugLog/Session.hpp>

#include <ScratchPadd/Logger.hpp>

#if defined(__3DS__)
#include <ScratchPadd/DebugLog/LogBufferSink.hpp>
#include <ScratchPadd/DebugLog/LogServer.hpp>
#elif defined(__NDS__)
#include <fat.h>
#include <spdlog/sinks/base_sink.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <mutex>
#endif

namespace ScratchPadd::DebugLog {

#if defined(__3DS__)
struct Session::Platform {
  LogBuffer buffer;
  LogServer server{buffer};
};
#else
struct Session::Platform {};
#endif

#if defined(__3DS__) || defined(__NDS__)
namespace {
// Each line: its time, the padd's logger, the level, the text
constexpr const char *kPattern = "%H:%M:%S.%e [%n] %l: %v";
} // namespace
#endif

#if defined(__NDS__)
namespace {
// A log file on the card, each line on the card once it's logged: fflush
// (all spdlog's file sink does) only reaches libfat's cache, which goes to
// the card when the file closes, so an app switched off rather than quit
// left its log empty. fsync writes the cache out.
class CardFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit CardFileSink(const std::string &path) : file_(std::fopen(path.c_str(), "w")) {
    if (!file_)
      throw spdlog::spdlog_ex("can't write " + path);
  }
  ~CardFileSink() override { std::fclose(file_); }
  CardFileSink(const CardFileSink &) = delete;
  CardFileSink &operator=(const CardFileSink &) = delete;

protected:
  void sink_it_(const spdlog::details::log_msg &message) override {
    spdlog::memory_buf_t line;
    formatter_->format(message, line);
    std::fwrite(line.data(), 1, line.size(), file_);
    flush_();
  }
  void flush_() override {
    std::fflush(file_);
    ::fsync(::fileno(file_));
  }

private:
  std::FILE *file_;
};
} // namespace
#endif

Session::Session(const std::string &appName, spdlog::level::level_enum level)
    : platform_(std::make_unique<Platform>()) {
#if defined(__3DS__)
  if (!platform_->server.listening()) {
    where_ = "off: " + platform_->server.error();
    return;
  }
  auto sink = std::make_shared<LogBufferSink>(platform_->buffer);
  sink->set_pattern(kPattern);
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
  try {
    auto sink = std::make_shared<CardFileSink>(path);
    sink->set_pattern(kPattern);
    Logger::routeTo({sink}, false);
  } catch (const spdlog::spdlog_ex &e) {
    where_ = std::string("off: ") + e.what();
    return;
  }
  where_ = path;
#else
  (void)appName;
  where_ = "the terminal";
#endif
  spdlog::set_level(level);
  PADDLOG_INFO("{}: logging to {}", appName, where_);
}

Session::~Session() {
  spdlog::set_level(spdlog::level::info);
  Logger::routeTo({}, true);
}

} // namespace ScratchPadd::DebugLog
