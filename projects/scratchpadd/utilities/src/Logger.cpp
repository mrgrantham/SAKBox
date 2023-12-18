#include <ScratchPadd/Logger.hpp>

thread_local std::shared_ptr<spdlog::logger> tThreadLogger = nullptr;

std::shared_ptr<spdlog::logger> CreateLoggerIfNeeded(const std::string &name) {
  auto logger = spdlog::get(name);
  if (!logger) {
    logger = spdlog::stdout_color_mt(name);
  }
  return logger;
}

std::shared_ptr<spdlog::logger> ScratchPadd::Logger::GetGlobalLogger() {
  static auto gThreadLogger = CreateLoggerIfNeeded("Global");
  return gThreadLogger;
}

std::shared_ptr<spdlog::logger> ScratchPadd::Logger::GetThreadLogger() {
  return tThreadLogger;
}

std::shared_ptr<spdlog::logger> ScratchPadd::Logger::GetActiveLogger() {
  if (tThreadLogger) {
    return tThreadLogger;
  }
  return GetGlobalLogger();
}

void ScratchPadd::Logger::setLoggerToThread() { tThreadLogger = logger_; }

void ScratchPadd::Logger::clearThreadLogger() { tThreadLogger = nullptr; }