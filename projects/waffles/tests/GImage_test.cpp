// GImage.
#include <gtest/gtest.h>

#include <WClasses/GImage.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <cmath>

using namespace GClasses;

namespace {
uint64_t fingerprint(GImage &image) {
  uint64_t h = 1469598103934665603ull; // FNV-1a
  const unsigned int *p = image.pixels();
  for (size_t i = 0; i < (size_t)image.width() * image.height(); i++) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}
} // namespace

TEST(ImageTextTest, RenderingIsUnchanged) {
  // Every printable character, at a few sizes (fingerprint recorded from
  // the code before the rewrite of GImage::textChar's drawing rules)
  std::string all;
  for (char c = ' '; c <= '~'; c++)
    all += c;
  GImage image;
  image.setSize(1200, 160);
  image.clear(0xff000000);
  image.text(all.c_str(), 0, 0, 1.0f, 0xffffffff);
  image.text(all.c_str(), 0, 40, 1.7f, 0xffff0000);
  image.text(all.c_str(), 0, 90, 2.3f, 0xff00ff00);
  EXPECT_EQ(fingerprint(image), 18229812759536327879ull);
}

// A PPM header's numbers were read with atoi, so a width of "2x" loaded as 2.
TEST(ImagePixMapTest, HeaderNumbersMustBeWhole) {
  const char *tmp = getenv("TEST_TMPDIR");
  const std::string dir = tmp ? tmp : "/tmp";
  auto write = [&](const std::string &name, const std::string &header) {
    std::ofstream f(dir + "/" + name, std::ios::binary);
    f << header << std::string(12, '\x7f'); // 2 x 2 pixels, 3 bytes each
    return dir + "/" + name;
  };
  GImage ok;
  ok.loadPpm(write("ok.ppm", "P6\n2 2\n255\n").c_str());
  EXPECT_EQ(ok.width(), 2u);
  EXPECT_EQ(ok.height(), 2u);
  GImage bad;
  EXPECT_THROW(bad.loadPpm(write("bad.ppm", "P6\n2x 2\n255\n").c_str()), std::exception);
}

// munge's "waved in or out of the middle" (style 3, used by captcha)
// measured each pixel's angle from the middle with y - m_height / 2 and
// x - m_width / 2, which are unsigned: above or left of the middle they
// wrapped to about 4 billion instead of going negative, so the waves were
// wrong there. Checked against the effect's formula, worked out here.
TEST(ImageMungeTest, WavesAroundTheMiddle) {
  GImage image;
  image.setSize(64, 48);
  for (int y = 0; y < 48; y++)
    for (int x = 0; x < 64; x++)
      image.setPixel(x, y, ((x / 4 + y / 4) % 2) ? 0xffffffff : 0xff000000);
  std::unique_ptr<GImage> waved(image.munge(3, 0.5f));
  float extent = 1 - 0.5f; // as munge eases it
  extent *= extent;
  extent = 1 - extent;
  size_t wrong = 0;
  for (int y = 0; y < 48; y++)
    for (int x = 0; x < 64; x++) {
      double d = atan2((double)(y - 24), (double)(x - 32));
      d = extent * cos(d * 5);
      const unsigned int expected =
          image.pixelNearest((int)((1 - d) * x + d * 32), (int)((1 - d) * y + d * 24));
      wrong += waved->pixel(x, y) != expected;
    }
  EXPECT_EQ(wrong, 0u);
}
