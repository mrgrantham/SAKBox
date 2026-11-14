#include <ScratchPadd/Storage/Folders.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

using ScratchPadd::Storage::Folders;

namespace {
ScratchPadd::AppInfo app(const std::string &name) {
  return {.name = name, .title = name, .build = "debug", .platform = "test"};
}
} // namespace

// Each platform's usual places, named after the app, with no other name in
// them (the toolkit the app was made with isn't one)
TEST(FoldersTest, ThePlatformsUsualPlacesNamedAfterTheApp) {
  const Folders folders = Folders::forApp(app("my-app"));
#if defined(__APPLE__)
  const std::string home = std::getenv("HOME");
  EXPECT_EQ(folders.files, home + "/Library/Application Support/my-app/");
  EXPECT_EQ(folders.logs, home + "/Library/Logs/my-app/");
#elif defined(_WIN32)
  EXPECT_NE(folders.files.find("\\my-app\\"), std::string::npos);
  EXPECT_EQ(folders.logs, folders.files + "logs\\");
#else
  EXPECT_NE(folders.files.find("/my-app/"), std::string::npos);
  EXPECT_NE(folders.logs.find("/my-app/"), std::string::npos);
#endif
  EXPECT_EQ(folders.files.find("sakbox"), std::string::npos);
  EXPECT_EQ(folders.files.find("SAKBox"), std::string::npos);
}

#if defined(__linux__)
// Linux follows the XDG folders when they're set
TEST(FoldersTest, LinuxFollowsXdg) {
  ::setenv("XDG_DATA_HOME", "/data/here", 1);
  ::setenv("XDG_STATE_HOME", "/state/here/", 1);
  const Folders folders = Folders::forApp(app("my-app"));
  EXPECT_EQ(folders.files, "/data/here/my-app/");
  EXPECT_EQ(folders.logs, "/state/here/my-app/");
}
#endif

// Under a root (for tests and tools), files there and logs in logs/;
// makeFolder makes the folders above too
TEST(FoldersTest, UnderARootAndMadeAsNeeded) {
  const std::string root = std::string(std::getenv("TEST_TMPDIR")) + "/under";
  const Folders folders = Folders::under(root);
  EXPECT_EQ(folders.files, root + "/");
  EXPECT_EQ(folders.logs, root + "/logs/");
  EXPECT_TRUE(ScratchPadd::Storage::makeFolder(folders.logs + "deeper/still"));
  EXPECT_TRUE(std::filesystem::is_directory(folders.logs + "deeper/still"));
  EXPECT_TRUE(ScratchPadd::Storage::makeFolder(folders.logs)); // (already there)
}
