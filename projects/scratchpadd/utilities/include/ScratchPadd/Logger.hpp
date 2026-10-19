#pragma once

#include <cassert>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#pragma clang diagnostic ignored "-Wambiguous-reversed-operator"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <spdlog/spdlog.h>

#pragma clang diagnostic pop

namespace ScratchPadd {

class Logger {
private:
  std::shared_ptr<spdlog::logger> logger_;

public:
  static std::shared_ptr<spdlog::logger> GetGlobalLogger();
  static std::shared_ptr<spdlog::logger> GetThreadLogger();
  static std::shared_ptr<spdlog::logger> GetActiveLogger();
  // Handles loggers made once only once for each class. Helpful when running
  // in tests
  void createLoggerIfNeeded(const std::string &name) {
    logger_ = spdlog::get(name);
    if (!logger_) {
      logger_ = spdlog::stdout_color_mt(name);
    }
  }
  spdlog::logger &logger() const {
    assert(logger_);
    return *logger_;
  }

  // Sets the default logger used by the macro on the thread this method is
  // called from
  void setLoggerToThread();

  void clearThreadLogger();
};

} // namespace ScratchPadd

#define PADDLOG(level, ...) ScratchPadd::Logger::GetActiveLogger()->log(level, __VA_ARGS__);

#define PADDLOG_INFO(...)                                                                          \
  ScratchPadd::Logger::GetActiveLogger()->log(spdlog::level::info, __VA_ARGS__);

#define PADDLOG_DEBUG(...)                                                                         \
  ScratchPadd::Logger::GetActiveLogger()->log(spdlog::level::debug, __VA_ARGS__);

#define PADDLOG_ERROR(...)                                                                         \
  ScratchPadd::Logger::GetActiveLogger()->log(spdlog::level::error, __VA_ARGS__);

#define PADDLOG_INTERVAL(level, interval, ...)                                                     \
  {                                                                                                \
    static std::chrono::steady_clock::time_point intervalEndTime =                                 \
        std::chrono::steady_clock::now() + interval;                                               \
    if (std::chrono::steady_clock::now() > intervalEndTime) {                                      \
      ScratchPadd::Logger::GetActiveLogger()->log(level, __VA_ARGS__);                             \
      intervalEndTime = std::chrono::steady_clock::now() + interval;                               \
    }                                                                                              \
  }