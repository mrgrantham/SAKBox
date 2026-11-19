#include <ScratchPadd/Canvas/Canvas.hpp>

#include <gtest/gtest.h>

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

// Words draw some pixels, centred words about their middle
TEST(CanvasTest, WordsAreDrawnAndCentred) {
  Canvas canvas(64, 32);
  canvas.clear(kBlack);
  canvas.textCentred(32, 16, "HEX", kRed);
  EXPECT_GT(count(canvas, kRed), 10);
  int left = 64, right = 0;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      if (canvas.at(x, y) == kRed) {
        left = std::min(left, x);
        right = std::max(right, x);
      }
  EXPECT_NEAR((left + right) / 2.0, 32, 1.5);
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
