#include <ScratchPadd/Logger.hpp>

#include <mutex>

thread_local std::shared_ptr<spdlog::logger> tThreadLogger = nullptr;

namespace {

// Where loggers write (routeTo), for every logger made
struct Routes {
  std::mutex mutex;
  std::vector<spdlog::sink_ptr> sinks;
  bool terminal = true;
  bool flushEveryLine = false;
  spdlog::sink_ptr terminalSink;
};
Routes &routes() {
  static Routes r;
  return r;
}

// The sinks a logger writes to now (under routes().mutex)
std::vector<spdlog::sink_ptr> currentSinks(Routes &r) {
  std::vector<spdlog::sink_ptr> sinks;
  if (r.terminal) {
    if (!r.terminalSink)
      r.terminalSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(r.terminalSink);
  }
  sinks.insert(sinks.end(), r.sinks.begin(), r.sinks.end());
  return sinks;
}

} // namespace

std::shared_ptr<spdlog::logger> ScratchPadd::Logger::loggerNamed(const std::string &name) {
  Routes &r = routes();
  std::lock_guard lock(r.mutex);
  if (auto logger = spdlog::get(name))
    return logger;
  auto sinks = currentSinks(r);
  auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
  // At the app's level (a console app's set_level(off) included), flushing
  // as the route says. (Not spdlog::initialize_logger: it would set its
  // pattern on every sink, a route's own included.)
  logger->set_level(spdlog::get_level());
  if (r.flushEveryLine)
    logger->flush_on(spdlog::level::trace);
  spdlog::register_logger(logger);
  return logger;
}

void ScratchPadd::Logger::routeTo(std::vector<spdlog::sink_ptr> sinks, bool terminal,
                                  bool flushEveryLine) {
  Routes &r = routes();
  std::lock_guard lock(r.mutex);
  r.sinks = std::move(sinks);
  r.terminal = terminal;
  r.flushEveryLine = flushEveryLine;
  const auto now = currentSinks(r);
  spdlog::apply_all([&](const std::shared_ptr<spdlog::logger> &logger) {
    logger->sinks() = now;
    logger->flush_on(flushEveryLine ? spdlog::level::trace : spdlog::level::off);
  });
}

std::shared_ptr<spdlog::logger> ScratchPadd::Logger::GetGlobalLogger() {
  static auto gThreadLogger = loggerNamed("Global");
  return gThreadLogger;
}

std::shared_ptr<spdlog::logger> ScratchPadd::Logger::GetThreadLogger() { return tThreadLogger; }

std::shared_ptr<spdlog::logger> ScratchPadd::Logger::GetActiveLogger() {
  if (tThreadLogger) {
    return tThreadLogger;
  }
  return GetGlobalLogger();
}

void ScratchPadd::Logger::setLoggerToThread() { tThreadLogger = logger_; }

void ScratchPadd::Logger::clearThreadLogger() { tThreadLogger = nullptr; }