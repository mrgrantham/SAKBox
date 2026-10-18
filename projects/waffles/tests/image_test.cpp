// GImage.
#include <gtest/gtest.h>

#include <WClasses/GImage.h>

#include <cstddef>
#include <cstdint>
#include <string>

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
