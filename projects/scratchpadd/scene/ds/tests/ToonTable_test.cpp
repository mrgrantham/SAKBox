#include "ToonTable.hpp"

#include <gtest/gtest.h>

using namespace ScratchPadd::Scene;

namespace {
int greyOf(std::uint16_t colour) {
  const int red = colour & 31;
  EXPECT_EQ((colour >> 5) & 31, red);
  EXPECT_EQ((colour >> 10) & 31, red);
  return red;
}
} // namespace

// Shades: three greys for three bands, darkest for little light, white for
// the most, changing where the bands do
TEST(ToonTableTest, ShadesInBands) {
  const auto table = DisplayList::toonTable(DsToonTable::Shades, 3);
  EXPECT_EQ(greyOf(table[0]), DisplayList::kDarkestShade);
  EXPECT_EQ(greyOf(table[10]), DisplayList::kDarkestShade); // (10/31 under a third)
  EXPECT_EQ(greyOf(table[11]), 20);                         // half way
  EXPECT_EQ(greyOf(table[20]), 20);
  EXPECT_EQ(greyOf(table[21]), 31);
  EXPECT_EQ(greyOf(table[31]), 31);
}

// Highlights: nothing added in the darkest band, the most in the brightest
TEST(ToonTableTest, HighlightsAddLight) {
  const auto table = DisplayList::toonTable(DsToonTable::Highlights, 2);
  EXPECT_EQ(greyOf(table[0]), 0);
  EXPECT_EQ(greyOf(table[15]), 0);
  EXPECT_EQ(greyOf(table[16]), DisplayList::kBrightestHighlight);
  EXPECT_EQ(greyOf(table[31]), DisplayList::kBrightestHighlight);
}
