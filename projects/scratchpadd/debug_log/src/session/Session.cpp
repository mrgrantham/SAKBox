#include <ScratchPadd/DebugLog/Session.hpp>

#include <ScratchPadd/AppInfo.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/Storage/AppendFile.hpp>
#include <ScratchPadd/Storage/Source.hpp>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

#include <mutex>

#if defined(__3DS__)
#include <ScratchPadd/DebugLog/LogBufferSink.hpp>
#include <ScratchPadd/DebugLog/LogServer.hpp>
#endif

namespace ScratchPadd::DebugLog {

struct Session::Platform {
  std::unique_ptr<Storage::AppendFile> file;
#if defined(__3DS__)
  LogBuffer buffer;
  LogServer server{buffer};
#endif
};

namespace {

// Every logger's lines, formatted, into the log's file (written in batches;
// warnings and errors at once)
class AppendFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit AppendFileSink(Storage::AppendFile &file) : file_(file) {}

protected:
  void sink_it_(const spdlog::details::log_msg &message) override {
    spdlog::memory_buf_t line;
    formatter_->format(message, line);
    file_.add(std::string_view(line.data(), line.size()), message.level >= spdlog::level::warn);
  }
  void flush_() override { file_.write(); }

private:
  Storage::AppendFile &file_;
};

// The file's own lines (its figures) formatted as a "Storage" logger's
std::string storageLine(std::string_view text) {
  static const std::shared_ptr<spdlog::formatter> kFormatter(Logger::lineFormatter());
  spdlog::memory_buf_t line;
  kFormatter->format(spdlog::details::log_msg("Storage", spdlog::level::info, text), line);
  return std::string(line.data(), line.size());
}

} // namespace

Session::Session(const SessionOptions &options) : platform_(std::make_unique<Platform>()) {
  const AppInfo &app = AppInfo::current();
  const Storage::Folders folders = options.folders.value_or(Storage::Folders::forApp(app));
  std::vector<spdlog::sink_ptr> sinks;

  std::string error;
  const std::string path = folders.logs + app.name + ".log";
  platform_->file = Storage::AppendFile::open(path,
                                              {.name = "log",
                                               .writeEvery = options.writeEvery,
                                               .keepRuns = options.keepRuns,
                                               .formatLine = storageLine},
                                              error);
  if (platform_->file) {
    Storage::sources().add(platform_->file.get()); // (for StoragePadd)
    sinks.push_back(std::make_shared<AppendFileSink>(*platform_->file));
    where_ = path;
  } else {
    where_ = "off: " + error;
  }

#if defined(__3DS__)
  if (platform_->server.listening()) {
    sinks.push_back(std::make_shared<LogBufferSink>(platform_->buffer));
    where_ = platform_->server.address() + (platform_->file ? ", " + path : "");
  } else if (!platform_->file) {
    where_ += "; " + platform_->server.error();
  }
#endif
  for (const spdlog::sink_ptr &sink : sinks)
    sink->set_formatter(Logger::lineFormatter()); // (as the terminal's)
#if defined(__3DS__) || defined(__NDS__)
  constexpr bool kTerminal = false; // (a console's screens are the app's)
#else
  constexpr bool kTerminal = true;
#endif
  Logger::routeTo(sinks, kTerminal);
  Logger::setTerminalLevel(options.terminalLevel.value_or(options.level));
  spdlog::set_level(options.level);
  PADDLOG_INFO("{} ({}, {} build): logging to {}", app.title, app.name, app.build, where_);
}

Session::~Session() {
  spdlog::set_level(spdlog::level::info);
  Logger::setTerminalLevel(spdlog::level::trace);
  Logger::routeTo({}, true);
  if (platform_->file)
    Storage::sources().remove(platform_->file.get()); // (StoragePadd lets go first)
}

std::string Session::file() const { return platform_->file ? platform_->file->path() : ""; }

} // namespace ScratchPadd::DebugLog
