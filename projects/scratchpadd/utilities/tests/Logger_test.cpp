#include <ScratchPadd/Logger.hpp>
#include <gtest/gtest.h>
#include <spdlog/details/os.h>
#include <spdlog/sinks/ostream_sink.h>

#include <regex>
#include <sstream>
#include <string>

namespace {

// What `logger` writes at `level` with the line formatter
std::string lineFor(const std::string &logger, spdlog::level::level_enum level,
                    const std::string &text) {
  std::ostringstream out;
  auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(out);
  sink->set_formatter(ScratchPadd::Logger::lineFormatter());
  spdlog::logger(logger, sink).log(level, text);
  return out.str();
}

const std::string kEol = spdlog::details::os::default_eol;

} // namespace

// Time, level and [logger] in columns, so the messages line up
TEST(LoggerTest, LinesHaveTheTimeLevelAndBracketedNameInColumns) {
  const std::string info = lineFor("GamePadd", spdlog::level::info, "Starting GamePadd");
  EXPECT_TRUE(std::regex_match(info.substr(0, 13), std::regex(R"(\d\d:\d\d:\d\d\.\d\d\d )")))
      << info;
  EXPECT_EQ(info.substr(13), "info     [GamePadd]           Starting GamePadd" + kEol);
  const std::string warning = lineFor("scratchpadd", spdlog::level::warn, "Dropped");
  EXPECT_EQ(warning.substr(13), "warning  [scratchpadd]        Dropped" + kEol);
  // the messages start in the same column
  EXPECT_EQ(info.find("Starting"), warning.find("Dropped"));
}

// A name too long for its column isn't cut: the message moves along
TEST(LoggerTest, ALongNameIsKeptWhole) {
  const std::string line = lineFor("ControlGeneratorPadd", spdlog::level::info, "Hello");
  EXPECT_EQ(line.substr(13), "info     [ControlGeneratorPadd] Hello" + kEol);
}
