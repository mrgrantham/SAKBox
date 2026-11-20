// The console's name as text: ASCII kept, the rest '?', trimmed, stopping
// at a terminator
#include <ScratchPadd/OwnerName/OwnerName.hpp>

#include <gtest/gtest.h>

#include <array>

using ScratchPadd::OwnerName::fromUtf16;

TEST(OwnerNameTest, Utf16BecomesPlainText) {
  const std::array<std::uint16_t, 8> james{'J', 'a', 'm', 'e', 's', 0, 'x', 'y'};
  EXPECT_EQ(fromUtf16(james), "James");
  const std::array<std::uint16_t, 5> spaced{' ', 'A', 0x00E9, 'l', ' '};
  EXPECT_EQ(fromUtf16(spaced), "A?l");
  const std::array<std::uint16_t, 2> blank{' ', ' '};
  EXPECT_EQ(fromUtf16(blank), "");
}

// A desktop account: the full name's first word, else the login name
TEST(OwnerNameTest, ADesktopAccountsFirstName) {
  using ScratchPadd::OwnerName::fromAccount;
  EXPECT_EQ(fromAccount("James Grantham", "jamesgrantham"), "James");
  EXPECT_EQ(fromAccount("Ada Lovelace,,,", "ada"), "Ada");
  EXPECT_EQ(fromAccount("", "josh"), "Josh");
  EXPECT_EQ(fromAccount(" , ", ""), "");
}
