#pragma once

#include "spdlog/sinks/stdout_color_sinks.h"
#include <cassert>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <vector>

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
  void createLoggerIfNeeded(const std::string &name) { logger_ = loggerNamed(name); }

  // How every ScratchPadd log line looks, in the terminal and off a console
  // (DebugLog::Session): its time, level and [logger] in columns, then the
  // message. kLinePattern is spdlog's pattern; %N in it is ScratchPadd's,
  // the logger's name in brackets, padded as a whole:
  //   11:57:35.855 info     [RagdollPadd]        Base() Destroying: RagdollPadd
  //   11:16:45.758 warning  [scratchpadd]        No padd handles ...
  // (%^ and %$ colour the level in a colour terminal; other sinks ignore them)
  static constexpr const char *kLinePattern = "%H:%M:%S.%e %^%-8l%$ %-20N %v";
  static std::unique_ptr<spdlog::formatter> lineFormatter();
  // The logger `name`, made (and registered with spdlog) the first time:
  // to the terminal, and wherever routeTo() says
  static std::shared_ptr<spdlog::logger> loggerNamed(const std::string &name);
  // Where every logger writes: `sinks` as well as the terminal, or instead
  // of it (`terminal` false: on a console, whose screens are the app's).
  // Loggers made before are changed too. DebugLog::Session calls it.
  static void routeTo(std::vector<spdlog::sink_ptr> sinks, bool terminal);
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
