#include "Bitmap.hpp"

#include <gtest/gtest.h>

using namespace ScratchPadd::Scene;

// A 2 x 2 picture: the header says so, and the pixels come out bottom row
// first, each blue, green, red, the DS's 5 bits widened to 8
TEST(BitmapTest, WritesABmp) {
  const std::uint16_t red = 31, green = 31 << 5, blue = 31 << 10, white = 0x7FFF;
  const std::uint16_t pixels[] = {red, green, blue, white}; // top row: red, green
  const std::vector<std::uint8_t> bmp = DisplayList::bmpFromRgb15(pixels, 2, 2);
  ASSERT_EQ(bmp.size(), 54u + 2 * 8); // (rows of 6 bytes padded to 8)
  EXPECT_EQ(bmp[0], 'B');
  EXPECT_EQ(bmp[1], 'M');
  EXPECT_EQ(bmp[2], bmp.size()); // (small enough for its first byte)
  EXPECT_EQ(bmp[18], 2);         // width
  EXPECT_EQ(bmp[22], 2);         // height
  EXPECT_EQ(bmp[28], 24);        // bits a pixel
  // The bottom row first: blue, then white
  const std::vector<std::uint8_t> bottom(bmp.begin() + 54, bmp.begin() + 60);
  EXPECT_EQ(bottom, (std::vector<std::uint8_t>{255, 0, 0, 255, 255, 255}));
  // Then the top: red, then green
  const std::vector<std::uint8_t> top(bmp.begin() + 62, bmp.begin() + 68);
  EXPECT_EQ(top, (std::vector<std::uint8_t>{0, 0, 255, 0, 255, 0}));
  EXPECT_EQ(DisplayList::channel8(16), 132);
}
