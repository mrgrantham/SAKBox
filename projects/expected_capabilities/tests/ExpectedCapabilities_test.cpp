#include <ExpectedCapabilities/ExpectedCapabilities.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string>

namespace {

// Sets SAKBOX_EXPECT for the scope, and puts back what was there
class ExpectFor {
public:
  explicit ExpectFor(const char *list) {
    if (const char *was = std::getenv(Name))
      previous = was;
    set(list);
  }
  ~ExpectFor() { set(previous ? previous->c_str() : nullptr); }
  ExpectFor(const ExpectFor &) = delete;
  ExpectFor &operator=(const ExpectFor &) = delete;

private:
  static constexpr const char *Name = "SAKBOX_EXPECT";
  static void set(const char *list) {
#ifdef _WIN32
    _putenv_s(Name, list ? list : "");
#else
    if (list)
      setenv(Name, list, 1);
    else
      unsetenv(Name);
#endif
  }
  std::optional<std::string> previous;
};

void missingDisplay() { SKIP_OR_FAIL_WITHOUT("display", std::string("no window")); }

// What missingDisplay reports, caught before it reaches the test
testing::TestPartResult whatMissingDisplayReports() {
  testing::TestPartResultArray results;
  {
    testing::ScopedFakeTestPartResultReporter catching(
        testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &results);
    missingDisplay();
  }
  EXPECT_EQ(results.size(), 1);
  return results.GetTestPartResult(0);
}

} // namespace

TEST(ExpectedCapabilitiesTest, NamesAreMatchedWholeInTheList) {
  ExpectFor expect("display,vulkan,midi");
  EXPECT_TRUE(ExpectedCapabilities::expected("display"));
  EXPECT_TRUE(ExpectedCapabilities::expected("vulkan"));
  EXPECT_TRUE(ExpectedCapabilities::expected("midi"));
  EXPECT_FALSE(ExpectedCapabilities::expected("metal"));
  EXPECT_FALSE(ExpectedCapabilities::expected("vulk"));
  EXPECT_FALSE(ExpectedCapabilities::expected(""));
}

TEST(ExpectedCapabilitiesTest, NothingIsExpectedWithoutTheList) {
  ExpectFor expect(nullptr);
  EXPECT_FALSE(ExpectedCapabilities::expected("display"));
}

TEST(ExpectedCapabilitiesTest, MissingWhatsExpectedFails) {
  ExpectFor expect("vulkan,display");
  const testing::TestPartResult result = whatMissingDisplayReports();
  EXPECT_TRUE(result.fatally_failed());
  EXPECT_THAT(result.message(),
              testing::HasSubstr("no window (and SAKBOX_EXPECT says this machine has display)"));
}

TEST(ExpectedCapabilitiesTest, MissingWhatsNotExpectedSkips) {
  ExpectFor expect("vulkan");
  const testing::TestPartResult result = whatMissingDisplayReports();
  EXPECT_TRUE(result.skipped());
  EXPECT_THAT(result.message(), testing::HasSubstr("no window"));
}
