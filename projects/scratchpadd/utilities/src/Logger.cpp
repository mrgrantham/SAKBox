#include <ScratchPadd/Logger.hpp>

#include <spdlog/pattern_formatter.h>

#include <mutex>

thread_local std::shared_ptr<spdlog::logger> tThreadLogger = nullptr;

namespace {

// %N in a pattern: the logger's name in brackets, padded as a whole
// ("[GamePadd]" then spaces, where spdlog's %-20n would put the spaces
// inside the brackets)
class BracketedNameFlag final : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg &message, const std::tm &,
              spdlog::memory_buf_t &line) override {
    const auto &name = message.logger_name;
    const std::size_t size = name.size() + 2;
    const std::size_t padding =
        padinfo_.enabled() && padinfo_.width_ > size ? padinfo_.width_ - size : 0;
    using Side = spdlog::details::padding_info::pad_side;
    const std::size_t before = padinfo_.side_ == Side::left     ? padding
                               : padinfo_.side_ == Side::center ? padding / 2
                                                                : 0;
    line.append(std::string(before, ' '));
    line.push_back('[');
    line.append(name.data(), name.data() + name.size());
    line.push_back(']');
    line.append(std::string(padding - before, ' '));
  }
  std::unique_ptr<custom_flag_formatter> clone() const override {
    return std::make_unique<BracketedNameFlag>();
  }
};

// Where loggers write (routeTo), for every logger made
struct Routes {
  std::mutex mutex;
  std::vector<spdlog::sink_ptr> sinks;
  bool terminal = true;
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
    if (!r.terminalSink) {
      r.terminalSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      r.terminalSink->set_formatter(ScratchPadd::Logger::lineFormatter());
    }
    sinks.push_back(r.terminalSink);
  }
  sinks.insert(sinks.end(), r.sinks.begin(), r.sinks.end());
  return sinks;
}

} // namespace

std::unique_ptr<spdlog::formatter> ScratchPadd::Logger::lineFormatter() {
  auto formatter = std::make_unique<spdlog::pattern_formatter>();
  formatter->add_flag<BracketedNameFlag>('N').set_pattern(kLinePattern);
  return formatter;
}

std::shared_ptr<spdlog::logger> ScratchPadd::Logger::loggerNamed(const std::string &name) {
  Routes &r = routes();
  std::lock_guard lock(r.mutex);
  if (auto logger = spdlog::get(name))
    return logger;
  auto sinks = currentSinks(r);
  auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
  // At the app's level (a console app's set_level(off) included). (Not
  // spdlog::initialize_logger: it would set its pattern on every sink, a
  // route's own included.)
  logger->set_level(spdlog::get_level());
  spdlog::register_logger(logger);
  return logger;
}

void ScratchPadd::Logger::routeTo(std::vector<spdlog::sink_ptr> sinks, bool terminal) {
  Routes &r = routes();
  std::lock_guard lock(r.mutex);
  r.sinks = std::move(sinks);
  r.terminal = terminal;
  const auto now = currentSinks(r);
  spdlog::apply_all([&](const std::shared_ptr<spdlog::logger> &logger) { logger->sinks() = now; });
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