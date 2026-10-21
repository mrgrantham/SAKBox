// png/WImagePng.h: Waffles' PNG reading and writing (with the repo's
// libpng), which the plot tool and waffles_generate use.
#include <gtest/gtest.h>

#include <WClasses/WImage.h>
#include <png/WImagePng.h>

#include <cstdlib>
#include <png.h>
#include <stdexcept>
#include <string>
#include <vector>

using namespace WClasses;

namespace {
std::string tmpPath(const char *name) {
  const char *dir = std::getenv("TEST_TMPDIR");
  return std::string(dir ? dir : "/tmp") + "/" + name;
}
} // namespace

// An image saved as a PNG loads back with the same size and pixels, with
// and without its alpha channel.
TEST(WImagePngTest, SavedImageLoadsBackTheSame) {
  WImage image;
  image.setSize(7, 5);
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 7; x++)
      image.setPixel(x, y, gARGB(255 - 30 * x, 40 * x, 50 * y, (x * 37 + y * 11) % 256));

  const std::string rgba = tmpPath("rgba.png");
  FILE *f = fopen(rgba.c_str(), "wb");
  ASSERT_NE(f, nullptr);
  savePng(&image, f, true);
  fclose(f);
  WImage loaded;
  loadPng(&loaded, rgba.c_str());
  ASSERT_EQ(loaded.width(), 7u);
  ASSERT_EQ(loaded.height(), 5u);
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 7; x++)
      EXPECT_EQ(loaded.pixel(x, y), image.pixel(x, y)) << x << "," << y;

  // Saved without its alpha channel, every pixel comes back opaque (the
  // overloads without the flag include alpha)
  const std::string rgb = tmpPath("rgb.png");
  f = fopen(rgb.c_str(), "wb");
  ASSERT_NE(f, nullptr);
  savePng(&image, f, false);
  fclose(f);
  WImage opaque;
  loadPng(&opaque, rgb.c_str());
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 7; x++)
      EXPECT_EQ(opaque.pixel(x, y), image.pixel(x, y) | 0xff000000u) << x << "," << y;
}

TEST(WImagePngTest, RejectsDataThatIsntAPng) {
  WImage image;
  const unsigned char notPng[] = "GIF89a, not a png";
  EXPECT_THROW(loadPng(&image, notPng, sizeof(notPng)), std::exception);
}

// PNGs stored other than as 8-bit RGB or RGBA: greyscale, grey with alpha,
// palette (with and without transparency), 1-bit and 16-bit. loadPng
// threw for all of these: the calls that ask libpng to expand them to RGB
// came after png_read_update_info, where libpng rejects them, so they had
// been commented out.
namespace {
// Writes a w x h PNG of the given type with libpng directly. `rows` holds
// each row's bytes as libpng expects them for that type.
void writeRawPng(const std::string &path, int w, int h, int depth, int colorType,
                 const std::vector<std::vector<png_byte>> &rows,
                 const std::vector<png_color> &palette = {},
                 const std::vector<png_byte> &paletteAlpha = {}) {
  FILE *f = fopen(path.c_str(), "wb");
  ASSERT_NE(f, nullptr);
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  png_init_io(png, f);
  png_set_IHDR(png, info, w, h, depth, colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  if (!palette.empty())
    png_set_PLTE(png, info, palette.data(), (int)palette.size());
  if (!paletteAlpha.empty())
    png_set_tRNS(png, info, paletteAlpha.data(), (int)paletteAlpha.size(), nullptr);
  png_write_info(png, info);
  for (const auto &row : rows)
    png_write_row(png, row.data());
  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  fclose(f);
}

WImage load(const std::string &path) {
  WImage image;
  loadPng(&image, path.c_str());
  return image;
}
} // namespace

TEST(WImagePngTest, LoadsGreyscale) {
  const std::string path = tmpPath("grey.png");
  writeRawPng(path, 3, 1, 8, PNG_COLOR_TYPE_GRAY, {{0, 100, 255}});
  WImage image = load(path);
  ASSERT_EQ(image.width(), 3u);
  EXPECT_EQ(image.pixel(0, 0), gARGB(255, 0, 0, 0));
  EXPECT_EQ(image.pixel(1, 0), gARGB(255, 100, 100, 100));
  EXPECT_EQ(image.pixel(2, 0), gARGB(255, 255, 255, 255));
}

TEST(WImagePngTest, LoadsGreyWithAlpha) {
  const std::string path = tmpPath("grey_alpha.png");
  writeRawPng(path, 2, 1, 8, PNG_COLOR_TYPE_GRAY_ALPHA, {{100, 50, 200, 255}});
  WImage image = load(path);
  EXPECT_EQ(image.pixel(0, 0), gARGB(50, 100, 100, 100));
  EXPECT_EQ(image.pixel(1, 0), gARGB(255, 200, 200, 200));
}

TEST(WImagePngTest, LoadsPalette) {
  const std::string path = tmpPath("palette.png");
  const std::vector<png_color> palette = {{10, 20, 30}, {200, 100, 0}};
  writeRawPng(path, 3, 1, 8, PNG_COLOR_TYPE_PALETTE, {{1, 0, 1}}, palette);
  WImage image = load(path);
  EXPECT_EQ(image.pixel(0, 0), gARGB(255, 200, 100, 0));
  EXPECT_EQ(image.pixel(1, 0), gARGB(255, 10, 20, 30));
  EXPECT_EQ(image.pixel(2, 0), gARGB(255, 200, 100, 0));
}

// A palette's transparency (its tRNS chunk) becomes the alpha channel
TEST(WImagePngTest, LoadsPaletteWithTransparency) {
  const std::string path = tmpPath("palette_alpha.png");
  const std::vector<png_color> palette = {{10, 20, 30}, {200, 100, 0}};
  writeRawPng(path, 2, 1, 8, PNG_COLOR_TYPE_PALETTE, {{0, 1}}, palette, {0, 128});
  WImage image = load(path);
  EXPECT_EQ(image.pixel(0, 0), gARGB(0, 10, 20, 30));
  EXPECT_EQ(image.pixel(1, 0), gARGB(128, 200, 100, 0));
}

// Fewer than 8 bits per pixel: 1-bit black and white, eight pixels a byte
TEST(WImagePngTest, LoadsOneBitGreyscale) {
  const std::string path = tmpPath("one_bit.png");
  writeRawPng(path, 8, 1, 1, PNG_COLOR_TYPE_GRAY, {{0b10100000}});
  WImage image = load(path);
  ASSERT_EQ(image.width(), 8u);
  EXPECT_EQ(image.pixel(0, 0), gARGB(255, 255, 255, 255));
  EXPECT_EQ(image.pixel(1, 0), gARGB(255, 0, 0, 0));
  EXPECT_EQ(image.pixel(2, 0), gARGB(255, 255, 255, 255));
  EXPECT_EQ(image.pixel(7, 0), gARGB(255, 0, 0, 0));
}

// 16 bits per channel, scaled to 8 (v * 257 is v exactly at 16 bits)
TEST(WImagePngTest, LoadsSixteenBitRgb) {
  const std::string path = tmpPath("sixteen.png");
  auto be16 = [](int v) { return std::vector<png_byte>{(png_byte)(v >> 8), (png_byte)(v & 0xff)}; };
  std::vector<png_byte> row;
  for (int v : {10 * 257, 20 * 257, 30 * 257, 250 * 257, 128 * 257, 0}) {
    auto b = be16(v);
    row.insert(row.end(), b.begin(), b.end());
  }
  writeRawPng(path, 2, 1, 16, PNG_COLOR_TYPE_RGB, {row});
  WImage image = load(path);
  EXPECT_EQ(image.pixel(0, 0), gARGB(255, 10, 20, 30));
  EXPECT_EQ(image.pixel(1, 0), gARGB(255, 250, 128, 0));
}
