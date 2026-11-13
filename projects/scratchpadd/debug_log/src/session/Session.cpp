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

#include <algorithm>
#include <chrono>
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

#if defined(__NDS__)
namespace {
// A log file on the card, synced to it every `syncEvery` lines, and at
// once for a warning or an error. fflush (all spdlog's file sink does) only
// reaches libfat's cache, which goes to the card when the file closes, so
// an app switched off rather than quit left its log empty; fsync writes the
// cache out. How long the syncs take goes in the log every kSyncsPerReport
// of them, and when it closes.
class CardFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  CardFileSink(const std::string &path, unsigned syncEvery)
      : file_(std::fopen(path.c_str(), "w")), syncEvery_(std::max(1u, syncEvery)) {
    if (!file_)
      throw spdlog::spdlog_ex("can't write " + path);
  }
  ~CardFileSink() override {
    if (syncs_ % kSyncsPerReport != 0)
      report();
    std::fclose(file_);
  }
  CardFileSink(const CardFileSink &) = delete;
  CardFileSink &operator=(const CardFileSink &) = delete;

protected:
  void sink_it_(const spdlog::details::log_msg &message) override {
    spdlog::memory_buf_t line;
    formatter_->format(message, line);
    std::fwrite(line.data(), 1, line.size(), file_);
    if (++unsynced_ >= syncEvery_ || message.level >= spdlog::level::warn)
      flush_();
  }
  void flush_() override {
    if (unsynced_ == 0)
      return;
    const auto started = Clock::now();
    sync();
    const auto took = Clock::now() - started;
    unsynced_ = 0;
    ++syncs_;
    total_ += took;
    worst_ = std::max(worst_, took);
    if (syncs_ % kSyncsPerReport == 0)
      report();
  }

private:
  using Clock = std::chrono::steady_clock;
  using Milliseconds = std::chrono::duration<double, std::milli>;
  static constexpr unsigned kSyncsPerReport = 16;

  void sync() {
    std::fflush(file_);
    ::fsync(::fileno(file_));
  }
  // (written straight to the file, not logged: the sink is in use)
  void report() {
    if (syncs_ == 0)
      return;
    const std::string text = spdlog::fmt_lib::format(
        "{} card syncs so far, a sync every {}: {:.1f} ms each, {:.1f} ms at worst", syncs_,
        syncEvery_ == 1 ? std::string("line") : spdlog::fmt_lib::format("{} lines", syncEvery_),
        Milliseconds(total_).count() / syncs_, Milliseconds(worst_).count());
    // (formatted as the logged lines are, as the "DebugLog" logger's)
    spdlog::memory_buf_t line;
    formatter_->format(spdlog::details::log_msg("DebugLog", spdlog::level::info, text), line);
    std::fwrite(line.data(), 1, line.size(), file_);
    sync();
  }

  std::FILE *file_;
  unsigned syncEvery_;
  unsigned unsynced_ = 0;
  unsigned syncs_ = 0;
  Clock::duration total_{};
  Clock::duration worst_{};
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
  try {
    auto sink = std::make_shared<CardFileSink>(path, options.cardSyncEvery);
    sink->set_formatter(Logger::lineFormatter()); // (as the terminal's)
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
  spdlog::set_level(options.level);
  PADDLOG_INFO("{}: logging to {}", appName, where_);
}

Session::~Session() {
  spdlog::set_level(spdlog::level::info);
  Logger::routeTo({}, true);
}

} // namespace ScratchPadd::DebugLog
