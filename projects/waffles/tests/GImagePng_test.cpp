// png/GImagePng.h: Waffles' PNG reading and writing (with the repo's
// libpng), which the plot tool and waffles_generate use.
#include <gtest/gtest.h>

#include <WClasses/GImage.h>
#include <png/GImagePng.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

using namespace GClasses;

namespace {
std::string tmpPath(const char *name) {
  const char *dir = std::getenv("TEST_TMPDIR");
  return std::string(dir ? dir : "/tmp") + "/" + name;
}
} // namespace

// An image saved as a PNG loads back with the same size and pixels, with
// and without its alpha channel.
TEST(GImagePngTest, SavedImageLoadsBackTheSame) {
  GImage image;
  image.setSize(7, 5);
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 7; x++)
      image.setPixel(x, y, gARGB(255 - 30 * x, 40 * x, 50 * y, (x * 37 + y * 11) % 256));

  const std::string rgba = tmpPath("rgba.png");
  FILE *f = fopen(rgba.c_str(), "wb");
  ASSERT_NE(f, nullptr);
  savePng(&image, f, true);
  fclose(f);
  GImage loaded;
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
  GImage opaque;
  loadPng(&opaque, rgb.c_str());
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 7; x++)
      EXPECT_EQ(opaque.pixel(x, y), image.pixel(x, y) | 0xff000000u) << x << "," << y;
}

TEST(GImagePngTest, RejectsDataThatIsntAPng) {
  GImage image;
  const unsigned char notPng[] = "GIF89a, not a png";
  EXPECT_THROW(loadPng(&image, notPng, sizeof(notPng)), std::exception);
}
