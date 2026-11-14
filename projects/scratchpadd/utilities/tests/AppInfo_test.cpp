#include <ScratchPadd/AppInfo.hpp>

#include <gtest/gtest.h>

// Built without a console app's defines, an app is named after its
// executable (in Bazel, the target's name), and titled the same
TEST(AppInfoTest, NamedAfterItsExecutable) {
  const ScratchPadd::AppInfo &app = ScratchPadd::AppInfo::current();
  EXPECT_EQ(app.name, "AppInfo_test");
  EXPECT_EQ(app.title, app.name);
  EXPECT_TRUE(app.build == "debug" || app.build == "release");
#if defined(__APPLE__)
  EXPECT_EQ(app.platform, "macOS");
#elif defined(_WIN32)
  EXPECT_EQ(app.platform, "Windows");
#elif defined(__linux__)
  EXPECT_EQ(app.platform, "Linux");
#endif
  EXPECT_EQ(&app, &ScratchPadd::AppInfo::current()); // worked out once
}
