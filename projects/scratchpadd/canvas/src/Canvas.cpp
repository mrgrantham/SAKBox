#include <ScratchPadd/Canvas/Canvas.hpp>

#include <ScratchPadd/Scene/OverlayText.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace ScratchPadd {

Canvas::Canvas(int width, int height)
    : width_(width), height_(height),
      owned_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)), pixels_(owned_) {}

Canvas Canvas::over(std::span<Colour> pixels, int width, int height) {
  Canvas canvas;
  canvas.width_ = width;
  canvas.height_ = height;
  canvas.pixels_ = pixels.first(
      std::min(pixels.size(), static_cast<std::size_t>(width) * static_cast<std::size_t>(height)));
  return canvas;
}

// (a copy owns its pixels when the original did, else draws over the same)
Canvas::Canvas(const Canvas &other)
    : width_(other.width_), height_(other.height_), owned_(other.owned_),
      pixels_(other.owned_.empty() ? other.pixels_ : std::span<Colour>(owned_)) {}

Canvas &Canvas::operator=(const Canvas &other) {
  if (this != &other) {
    width_ = other.width_;
    height_ = other.height_;
    owned_ = other.owned_;
    pixels_ = other.owned_.empty() ? other.pixels_ : std::span<Colour>(owned_);
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

void Canvas::outline(float x, float y, float w, float h, Colour colour, float thickness) {
  fillRect(x, y, w, thickness, colour);
  fillRect(x, y + h - thickness, w, thickness, colour);
  fillRect(x, y, thickness, h, colour);
  fillRect(x + w - thickness, y, thickness, h, colour);
}

void Canvas::text(float x, float y, std::string_view words, Colour colour, float scale) {
  for (const Scene::TextRectangle &r :
       Scene::textRectangles({x, y, std::string(words), colour, scale}))
    fillRect(r.x, r.y, std::max(1.0f, r.width), std::max(1.0f, r.height), colour);
}

void Canvas::textCentred(float x, float y, std::string_view words, Colour colour, float scale) {
  const Scene::Overlay::Ink ink = Scene::Overlay::inkOf(words, scale);
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
