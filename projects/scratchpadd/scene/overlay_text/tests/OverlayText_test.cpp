// The overlay as the GPU backends draw it, in the platform's font (on
// desktops, Orbitron), without a window
#include <ScratchPadd/Scene/OverlayText.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace ScratchPadd::Scene;

namespace {
// The atlas's alpha at texture coordinates (u, v)
int alphaAt(const FontAtlas &atlas, float u, float v) {
  const int x = std::min(atlas.width - 1, static_cast<int>(u * static_cast<float>(atlas.width)));
  const int y = std::min(atlas.height - 1, static_cast<int>(v * static_cast<float>(atlas.height)));
  return atlas.alpha[static_cast<std::size_t>(y) * static_cast<std::size_t>(atlas.width) +
                     static_cast<std::size_t>(x)];
}

struct Bounds {
  float left = std::numeric_limits<float>::max(), top = left;
  float right = std::numeric_limits<float>::lowest(), bottom = right;
};
Bounds boundsOf(const std::vector<TexturedVertex> &vertices) {
  Bounds b;
  for (const TexturedVertex &v : vertices) {
    b.left = std::min(b.left, v.x);
    b.top = std::min(b.top, v.y);
    b.right = std::max(b.right, v.x);
    b.bottom = std::max(b.bottom, v.y);
  }
  return b;
}
} // namespace

// The overlay measures in the platform's font: Orbitron, off the DS
TEST(OverlayTextTest, MeasuresInThePlatformsFont) {
  EXPECT_EQ(&Font::platform(), &Font::orbitron());
  EXPECT_FLOAT_EQ(Overlay::textWidth("Build", 2), Font::orbitron().width("Build", 2));
  EXPECT_FLOAT_EQ(Overlay::lineHeight(1.5f), Font::orbitron().lineHeight(1.5f));
  EXPECT_FLOAT_EQ(Overlay::inkOf("7", 3).height, Font::orbitron().ink("7", 3).height);
}

// Shapes come first, drawn from the atlas's solid corner; then the text's
// letters, coloured, from their pictures
TEST(OverlayTextTest, ShapesThenLetters) {
  Overlay overlay;
  overlay.rectangle(0, 0, 10, 10, {0, 0, 255});
  overlay.text(100, 50, "Hi", Colour{255, 0, 0}, 2);
  std::vector<TexturedVertex> vertices;
  overlayVertices(overlay, vertices);
  ASSERT_GT(vertices.size(), 6u);
  EXPECT_EQ(vertices.size() % 3, 0u);
  const FontAtlas atlas = Font::platform().atlas();
  for (std::size_t i = 0; i < 6; ++i) {
    EXPECT_EQ(vertices[i].colour, (Colour{0, 0, 255}));
    EXPECT_EQ(alphaAt(atlas, vertices[i].u, vertices[i].v), 255);
  }
  const std::vector<TexturedVertex> letters(vertices.begin() + 6, vertices.end());
  for (const TexturedVertex &v : letters)
    EXPECT_EQ(v.colour, (Colour{255, 0, 0}));
  // ...where the text was put, within its width and line height
  const Bounds b = boundsOf(letters);
  EXPECT_GE(b.left, 100);
  EXPECT_GE(b.top, 50);
  EXPECT_LE(b.right, 100 + Overlay::textWidth("Hi", 2) + 1);
  EXPECT_LE(b.bottom, 50 + Overlay::lineHeight(2) + 1);
}

// A text's letters on whole points, wherever it was put (a picture between
// pixels is blurred)
TEST(OverlayTextTest, LettersOnWholePoints) {
  Overlay overlay;
  overlay.text(10.4f, 20.6f, "Fuel", Colour{255, 255, 255});
  std::vector<TexturedVertex> vertices;
  overlayVertices(overlay, vertices);
  ASSERT_FALSE(vertices.empty());
  for (std::size_t i = 0; i < vertices.size(); i += 6) {
    EXPECT_FLOAT_EQ(vertices[i].x, std::round(vertices[i].x));
    EXPECT_FLOAT_EQ(vertices[i].y, std::round(vertices[i].y));
  }
}

// Centring by the ink puts the letters' middle where it was asked
// (centring by the width left numbers off centre)
TEST(OverlayTextTest, CentredByItsInk) {
  Overlay overlay;
  overlay.textCentred(100, 50, "7", Colour{255, 255, 255}, 3);
  std::vector<TexturedVertex> vertices;
  overlayVertices(overlay, vertices);
  const Bounds b = boundsOf(vertices);
  EXPECT_NEAR((b.left + b.right) / 2, 100, 0.5f);
  EXPECT_NEAR((b.top + b.bottom) / 2, 50, 0.5f);
}

// Clipped text: letters cut to the clip, their pictures cut with them,
// those outside it gone
TEST(OverlayTextTest, ClippedTextIsCut) {
  Overlay overlay;
  overlay.clipText({.left = 0, .top = 0, .right = 100, .bottom = 4});
  overlay.text(10, 0, "8", {255, 255, 255}, 1);
  overlay.clipText({.left = 0, .top = 100, .right = 100, .bottom = 200});
  overlay.text(10, 0, "8", {255, 255, 255}, 1);
  overlay.clipText({});
  overlay.text(10, 0, "8", {255, 255, 255}, 1);
  std::vector<TexturedVertex> vertices;
  overlayVertices(overlay, vertices);
  ASSERT_EQ(vertices.size(), 12u); // (the clipped 8, the whole one; not the hidden one)
  const TexturedVertex &cutTop = vertices[0], &cutBottom = vertices[2];
  const TexturedVertex &top = vertices[6], &bottom = vertices[8];
  EXPECT_LE(cutBottom.y, 4);
  EXPECT_FLOAT_EQ(cutTop.v, top.v);
  EXPECT_LT(cutBottom.v, bottom.v);
  EXPECT_FLOAT_EQ((cutBottom.v - top.v) / (bottom.v - top.v),
                  (cutBottom.y - top.y) / (bottom.y - top.y));
}

// The atlas as RGBA: white, its alpha
TEST(OverlayTextTest, AtlasPixelsAreWhite) {
  const FontAtlas atlas = Font::platform().atlas();
  const std::vector<Colour> pixels = atlasPixels(atlas);
  ASSERT_EQ(pixels.size(), atlas.alpha.size());
  EXPECT_EQ(pixels[0], (Colour{255, 255, 255, 255}));
  for (std::size_t i = 0; i < pixels.size(); i += 997)
    EXPECT_EQ(pixels[i].a, atlas.alpha[i]);
}
