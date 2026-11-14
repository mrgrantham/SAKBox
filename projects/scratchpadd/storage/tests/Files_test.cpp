#include <ScratchPadd/Storage/Files.hpp>

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using ScratchPadd::Storage::Bytes;

namespace {
std::string scratch(const std::string &name) {
  return std::string(std::getenv("TEST_TMPDIR")) + "/files/" + name;
}
Bytes bytesOf(const std::string &text) { return Bytes(text.begin(), text.end()); }
} // namespace

// A save replaces the file whole (its folder made), leaving no .tmp behind,
// and loads back as it was
TEST(FilesTest, SavesWholeAndLoadsBack) {
  const std::string path = scratch("saves/slot1.bin");
  std::string error;
  ASSERT_TRUE(ScratchPadd::Storage::saveFile(path, bytesOf("first"), error)) << error;
  ASSERT_TRUE(ScratchPadd::Storage::saveFile(path, bytesOf("second, longer"), error)) << error;
  EXPECT_EQ(ScratchPadd::Storage::loadFile(path), bytesOf("second, longer"));
  EXPECT_FALSE(std::filesystem::exists(path + ".tmp"));
}

// A save that didn't get put in place (power lost between removing the old
// file and renaming, on a console) is loaded from its .tmp; nothing at all
// loads as nothing
TEST(FilesTest, LoadsAnUnfinishedSaveAndNothingFromNothing) {
  const std::string path = scratch("half/slot.bin");
  std::filesystem::create_directories(scratch("half"));
  std::FILE *file = std::fopen((path + ".tmp").c_str(), "wb");
  ASSERT_NE(file, nullptr);
  std::fputs("whole", file);
  std::fclose(file);
  EXPECT_EQ(ScratchPadd::Storage::loadFile(path), bytesOf("whole"));
  EXPECT_FALSE(ScratchPadd::Storage::loadFile(scratch("nothing/here.bin")).has_value());
}

// Where it can't write, it says so
TEST(FilesTest, SaysWhyItCouldntSave) {
  const std::string blocker = scratch("blocker");
  std::filesystem::create_directories(scratch(""));
  std::FILE *file = std::fopen(blocker.c_str(), "w"); // a file where a folder should be
  ASSERT_NE(file, nullptr);
  std::fclose(file);
  std::string error;
  EXPECT_FALSE(ScratchPadd::Storage::saveFile(blocker + "/inside.bin", bytesOf("x"), error));
  EXPECT_FALSE(error.empty());
}
