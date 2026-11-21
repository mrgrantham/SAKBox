#include <ScratchPadd/Canvas/Canvas.hpp>

#include <ScratchPadd/Scene/Font.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace ScratchPadd {

namespace {
// A letter's alpha at (x, y) in its picture (pixels, from its top left;
// between pixels, the four nearest mixed)
float letterAlpha(const Scene::FontAtlas &atlas, const Scene::TextPiece &piece, float x, float y) {
  const auto at = [&](int ax, int ay) {
    ax = std::clamp(ax, 0, piece.atlasWidth - 1);
    ay = std::clamp(ay, 0, piece.atlasHeight - 1);
    return static_cast<float>(atlas.alpha[static_cast<std::size_t>(piece.atlasY + ay) *
                                              static_cast<std::size_t>(atlas.width) +
                                          static_cast<std::size_t>(piece.atlasX + ax)]);
  };
  const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
  const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
  return (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy) +
         (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
}
} // namespace

Canvas::Canvas(int width, int height)
    : width_(width), height_(height),
      owned_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)), pixels_(owned_),
      font_(&Scene::Font::platform()) {}

Canvas Canvas::over(std::span<Colour> pixels, int width, int height) {
  Canvas canvas;
  canvas.font_ = &Scene::Font::platform();
  canvas.width_ = width;
  canvas.height_ = height;
  canvas.pixels_ = pixels.first(
      std::min(pixels.size(), static_cast<std::size_t>(width) * static_cast<std::size_t>(height)));
  return canvas;
}

// (a copy owns its pixels when the original did, else draws over the same)
Canvas::Canvas(const Canvas &other)
    : width_(other.width_), height_(other.height_), owned_(other.owned_),
      pixels_(other.owned_.empty() ? other.pixels_ : std::span<Colour>(owned_)),
      font_(other.font_) {}

Canvas &Canvas::operator=(const Canvas &other) {
  if (this != &other) {
    width_ = other.width_;
    height_ = other.height_;
    owned_ = other.owned_;
    pixels_ = other.owned_.empty() ? other.pixels_ : std::span<Colour>(owned_);
    font_ = other.font_;
  }
  return *this;
}

Canvas::Colour Canvas::at(int x, int y) const {
  return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                 static_cast<std::size_t>(x)];
}

void Canvas::clear(Colour colour) { std::fill(pixels_.begin(), pixels_.end(), colour); }

void Canvas::copy(std::span<const Colour> pixels) {
  if (pixels.size() == pixels_.size())
    std::copy(pixels.begin(), pixels.end(), pixels_.begin());
}

void Canvas::fillRect(float x, float y, float w, float h, Colour colour) {
  const int x0 = std::max(0, static_cast<int>(std::lround(x)));
  const int y0 = std::max(0, static_cast<int>(std::lround(y)));
  const int x1 = std::min(width_, static_cast<int>(std::lround(x + w)));
  const int y1 = std::min(height_, static_cast<int>(std::lround(y + h)));
  for (int py = y0; py < y1; ++py)
    std::fill_n(&pixels_[static_cast<std::size_t>(py) * static_cast<std::size_t>(width_) +
                         static_cast<std::size_t>(x0)],
                std::max(0, x1 - x0), colour);
}

void Canvas::blendRect(float x, float y, float w, float h, Colour colour, float opacity) {
  const int x0 = std::max(0, static_cast<int>(std::lround(x)));
  const int y0 = std::max(0, static_cast<int>(std::lround(y)));
  const int x1 = std::min(width_, static_cast<int>(std::lround(x + w)));
  const int y1 = std::min(height_, static_cast<int>(std::lround(y + h)));
  // (in 256ths, whole numbers: no floating point per pixel on a DS)
  const int a = std::clamp(static_cast<int>(std::lround(opacity * 256)), 0, 256);
  if (a == 0)
    return;
  const auto mix = [a](std::uint8_t under, std::uint8_t over) {
    return static_cast<std::uint8_t>((under * (256 - a) + over * a) >> 8);
  };
  for (int py = y0; py < y1; ++py)
    for (int px = x0; px < x1; ++px) {
      Colour &under = pixels_[static_cast<std::size_t>(py) * static_cast<std::size_t>(width_) +
                              static_cast<std::size_t>(px)];
      under = {mix(under.r, colour.r), mix(under.g, colour.g), mix(under.b, colour.b), under.a};
    }
}

void Canvas::outline(float x, float y, float w, float h, Colour colour, float thickness) {
  fillRect(x, y, w, thickness, colour);
  fillRect(x, y + h - thickness, w, thickness, colour);
  fillRect(x, y, thickness, h, colour);
  fillRect(x + w - thickness, y, thickness, h, colour);
}

void Canvas::text(float x, float y, std::string_view words, Colour colour, float scale) {
  // In the overlay's font: a pixel font's runs filled, a smooth font's
  // letters blended in by their alpha
  const Scene::Font &font = *font_;
  thread_local std::vector<Scene::TextPiece> pieces; // (no allocation a text)
  font.layOut(words, scale, pieces);
  for (const Scene::TextPiece &piece : pieces) {
    if (piece.solid())
      fillRect(x + piece.x, y + piece.y, piece.width, piece.height, colour);
    else
      blendLetter(font.atlas(), piece, x, y, colour);
  }
}

void Canvas::blendLetter(const Scene::FontAtlas &atlas, const Scene::TextPiece &piece, float x,
                         float y, Colour colour) {
  // On whole pixels (a letter between them is blurred), its picture
  // scaled to the piece's size
  const int left = static_cast<int>(std::lround(x + piece.x));
  const int top = static_cast<int>(std::lround(y + piece.y));
  const int w = std::max(1, static_cast<int>(std::lround(piece.width)));
  const int h = std::max(1, static_cast<int>(std::lround(piece.height)));
  const float stepX = static_cast<float>(piece.atlasWidth) / static_cast<float>(w);
  const float stepY = static_cast<float>(piece.atlasHeight) / static_cast<float>(h);
  for (int py = std::max(0, -top); py < h && top + py < height_; ++py) {
    const float sy = (static_cast<float>(py) + 0.5f) * stepY - 0.5f;
    for (int px = std::max(0, -left); px < w && left + px < width_; ++px) {
      const float sx = (static_cast<float>(px) + 0.5f) * stepX - 0.5f;
      const int a = static_cast<int>(letterAlpha(atlas, piece, sx, sy)) * colour.a / 255;
      if (a == 0)
        continue;
      Colour &d = pixels_[static_cast<std::size_t>(top + py) * static_cast<std::size_t>(width_) +
                          static_cast<std::size_t>(left + px)];
      const auto mix = [a](int from, int to) {
        return static_cast<std::uint8_t>(from + (to - from) * a / 255);
      };
      d = {mix(d.r, colour.r), mix(d.g, colour.g), mix(d.b, colour.b), d.a};
    }
  }
}

void Canvas::textCentred(float x, float y, std::string_view words, Colour colour, float scale) {
  const Scene::Overlay::Ink ink = font_->ink(words, scale);
  text(x - ink.left - ink.width / 2, y - ink.top - ink.height / 2, words, colour, scale);
}

void Canvas::blend(const std::vector<Colour> &sprite, int w, int h, int x, int y, float opacity) {
  if (sprite.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(h))
    return;
  const int keep = static_cast<int>(std::lround(std::clamp(opacity, 0.0f, 1.0f) * 256));
  for (int sy = std::max(0, -y); sy < h && y + sy < height_; ++sy)
    for (int sx = std::max(0, -x); sx < w && x + sx < width_; ++sx) {
      const Colour &s = sprite[static_cast<std::size_t>(sy) * static_cast<std::size_t>(w) +
                               static_cast<std::size_t>(sx)];
      const int a = s.a * keep / 256; // (whole numbers: no floating point on a DS)
      if (a == 0)
        continue;
      Colour &d = pixels_[static_cast<std::size_t>(y + sy) * static_cast<std::size_t>(width_) +
                          static_cast<std::size_t>(x + sx)];
      const auto mix = [a](int from, int to) {
        return static_cast<std::uint8_t>(from + (to - from) * a / 255);
      };
      d = {mix(d.r, s.r), mix(d.g, s.g), mix(d.b, s.b), 255};
    }
}

} // namespace ScratchPadd
