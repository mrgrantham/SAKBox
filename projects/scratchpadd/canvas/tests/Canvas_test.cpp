#include <ScratchPadd/Canvas/Canvas.hpp>
#include <ScratchPadd/Scene/Font.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using ScratchPadd::Canvas;

namespace {
constexpr Canvas::Colour kBlack{0, 0, 0, 255}, kRed{220, 40, 40, 255};

int count(const Canvas &canvas, Canvas::Colour colour) {
  int n = 0;
  for (const Canvas::Colour &c : canvas.pixels())
    n += c == colour;
  return n;
}
} // namespace

// A rectangle fills exactly its pixels; one partly off the picture is cut
TEST(CanvasTest, RectanglesFillTheirPixels) {
  Canvas canvas(20, 10);
  canvas.clear(kBlack);
  canvas.fillRect(2, 3, 4, 5, kRed);
  EXPECT_EQ(count(canvas, kRed), 20);
  EXPECT_EQ(canvas.at(2, 3), kRed);
  EXPECT_EQ(canvas.at(6, 3), kBlack);
  canvas.fillRect(18, 8, 10, 10, kRed);
  EXPECT_EQ(count(canvas, kRed), 24);
}

// Words draw some pixels (in Orbitron, many part of the way), centred words
// about their middle
TEST(CanvasTest, WordsAreDrawnAndCentred) {
  Canvas canvas(64, 32);
  canvas.clear(kBlack);
  canvas.textCentred(32, 16, "HEX", kRed);
  EXPECT_GT(64 * 32 - count(canvas, kBlack), 10);
  int left = 64, right = 0;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      if (canvas.at(x, y) != kBlack) {
        left = std::min(left, x);
        right = std::max(right, x);
      }
  EXPECT_NEAR((left + right) / 2.0, 32, 1.5);
}

// In Silkscreen (the DS's), words are its own pixels: an H at scale 1 is
// its two uprights and its bar; at scale 2 each pixel is 2 x 2
TEST(CanvasTest, SilkscreenWordsAreItsPixels) {
  Canvas canvas(16, 16);
  canvas.useFont(ScratchPadd::Scene::Font::silkscreen());
  canvas.clear(kBlack);
  canvas.text(0, 0, "H", kRed);
  std::string drawn;
  for (int y = 0; y < 7; ++y) {
    for (int x = 0; x < 6; ++x)
      drawn += canvas.at(x, y) == kRed ? '#' : '.';
    drawn += '\n';
  }
  EXPECT_EQ(drawn, "......\n"
                   ".#..#.\n"
                   ".#..#.\n"
                   ".####.\n"
                   ".#..#.\n"
                   ".#..#.\n"
                   "......\n");
  canvas.clear(kBlack);
  canvas.text(0, 0, "H", kRed, 2);
  EXPECT_EQ(count(canvas, kRed), 4 * 12);
  EXPECT_EQ(canvas.at(2, 2), kRed);
  EXPECT_EQ(canvas.at(3, 3), kRed);
}

// In Orbitron (elsewhere), words are smooth: their letters' edges blended
// part of the way, their middles the words' colour
TEST(CanvasTest, OrbitronWordsAreSmooth) {
  Canvas canvas(64, 32);
  canvas.clear(kBlack);
  canvas.text(4, 4, "HEX", kRed, 1.25f);
  int whole = 0, part = 0;
  for (const Canvas::Colour &c : canvas.pixels()) {
    whole += c == kRed;
    part += c != kRed && c != kBlack;
  }
  EXPECT_GT(whole, 20);
  EXPECT_GT(part, 0);
}

// Drawing over another picture's pixels changes them, not a copy
TEST(CanvasTest, DrawingOverAnotherPictureChangesIt) {
  std::vector<Canvas::Colour> picture(16 * 8, kBlack);
  Canvas over = Canvas::over(picture, 16, 8);
  over.fillRect(0, 0, 2, 2, kRed);
  EXPECT_EQ(picture[0], kRed);
  EXPECT_EQ(picture[17], kRed);
  EXPECT_EQ(picture[2], kBlack);
}

// A sprite blends by its alpha
TEST(CanvasTest, SpritesBlendByAlpha) {
  Canvas canvas(4, 4);
  canvas.clear(kBlack);
  const std::vector<Canvas::Colour> sprite{{200, 200, 200, 255}, {200, 200, 200, 0}};
  canvas.blend(sprite, 2, 1, 1, 1);
  EXPECT_EQ(canvas.at(1, 1), (Canvas::Colour{200, 200, 200, 255}));
  EXPECT_EQ(canvas.at(2, 1), kBlack);
}
