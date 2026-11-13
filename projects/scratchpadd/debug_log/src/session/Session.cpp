#include <ScratchPadd/DebugLog/Session.hpp>

#include <ScratchPadd/Logger.hpp>

#if defined(__3DS__)
#include <ScratchPadd/DebugLog/LogBufferSink.hpp>
#include <ScratchPadd/DebugLog/LogServer.hpp>
#elif defined(__NDS__)
#include <fat.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <sys/stat.h>
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
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);
    sink->set_pattern(kPattern);
    // Each line written to the card as it's logged (spdlog's flush_every
    // needs a timed wait, which the DS's threads don't have)
    Logger::routeTo({sink}, false, true);
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
