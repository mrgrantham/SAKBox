#include <ScratchPadd/Scene/OverlayText.hpp>

#include <algorithm>
#include <cmath>

namespace ScratchPadd::Scene {

namespace {

// Two triangles over a rectangle, the atlas's (u0, v0) to (u1, v1) on it
void addQuad(std::vector<TexturedVertex> &out, float left, float top, float right, float bottom,
             Colour colour, float u0, float v0, float u1, float v1) {
  const TexturedVertex a{left, top, colour, u0, v0}, b{right, top, colour, u1, v0},
      c{right, bottom, colour, u1, v1}, d{left, bottom, colour, u0, v1};
  out.insert(out.end(), {a, b, c, a, c, d});
}

void addShapes(const Overlay &overlay, const FontAtlas &atlas, std::vector<TexturedVertex> &out) {
  for (const Overlay::Vertex &corner : overlay.triangles())
    out.push_back({corner.x, corner.y, corner.colour, atlas.solidU(), atlas.solidV()});
}

// A piece of `text` as a quad, cut to the text's clip (its texture
// coordinates cut with it); nothing when none of it is inside
void addPiece(const Overlay::Text &text, const TextPiece &piece, const FontAtlas &atlas,
              std::vector<TexturedVertex> &out) {
  // (the letter on whole points: a picture between pixels is blurred)
  const float left = std::round(text.x + piece.x), top = std::round(text.y + piece.y);
  const float right = left + piece.width, bottom = top + piece.height;
  const float clippedLeft = std::max(left, text.clip.left),
              clippedTop = std::max(top, text.clip.top);
  const float clippedRight = std::min(right, text.clip.right),
              clippedBottom = std::min(bottom, text.clip.bottom);
  if (clippedRight <= clippedLeft || clippedBottom <= clippedTop)
    return;
  if (piece.solid()) {
    addQuad(out, clippedLeft, clippedTop, clippedRight, clippedBottom, text.colour, atlas.solidU(),
            atlas.solidV(), atlas.solidU(), atlas.solidV());
    return;
  }
  // The atlas's rectangle, in the proportion the clip left
  const auto w = static_cast<float>(atlas.width), h = static_cast<float>(atlas.height);
  const float u0 = static_cast<float>(piece.atlasX) / w,
              u1 = static_cast<float>(piece.atlasX + piece.atlasWidth) / w;
  const float v0 = static_cast<float>(piece.atlasY) / h,
              v1 = static_cast<float>(piece.atlasY + piece.atlasHeight) / h;
  const auto across = [&](float x) { return u0 + (u1 - u0) * (x - left) / (right - left); };
  const auto down = [&](float y) { return v0 + (v1 - v0) * (y - top) / (bottom - top); };
  addQuad(out, clippedLeft, clippedTop, clippedRight, clippedBottom, text.colour,
          across(clippedLeft), down(clippedTop), across(clippedRight), down(clippedBottom));
}

} // namespace

void overlayVertices(const Overlay &overlay, std::vector<TexturedVertex> &out) {
  const Font &font = Font::platform();
  const FontAtlas atlas = font.atlas();
  out.clear();
  addShapes(overlay, atlas, out);
  // (each text's pieces into a list kept by each thread: no allocation a
  // frame once it's grown)
  thread_local std::vector<TextPiece> pieces;
  for (const Overlay::Text &text : overlay.texts()) {
    font.layOut(text.text, text.scale, pieces);
    for (const TextPiece &piece : pieces)
      addPiece(text, piece, atlas, out);
  }
}

std::vector<Colour> atlasPixels(const FontAtlas &atlas) {
  std::vector<Colour> pixels;
  pixels.reserve(atlas.alpha.size());
  for (const std::uint8_t alpha : atlas.alpha)
    pixels.push_back({255, 255, 255, alpha});
  return pixels;
}

// The overlay measures text in the platform's font
float Overlay::textWidth(std::string_view text, float scale) {
  return Font::platform().width(text, scale);
}

float Overlay::lineHeight(float scale) { return Font::platform().lineHeight(scale); }

Overlay::Ink Overlay::inkOf(std::string_view text, float scale) {
  return Font::platform().ink(text, scale);
}

} // namespace ScratchPadd::Scene
