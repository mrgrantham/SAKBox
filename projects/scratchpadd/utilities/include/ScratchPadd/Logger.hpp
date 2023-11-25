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
};

} // namespace ScratchPadd