// Canvas.hpp - a picture for a console's bottom screen, drawn in pixels
// (round 101): rectangles, outlines, words in the overlay's font (centred
// by the letters' own pixels, as Overlay::textCentred) and sprites (tile
// faces, card art) blended over. The screens drawn on the bottom (the build
// screen, the pickers) share it; the app copies its pixels to the screen.
//
//   Canvas canvas(256, 192);
//   canvas.clear(kBackground);
//   canvas.fillRect(4, 4, 60, 20, kButton);
//   canvas.textCentred(34, 14, "Build", kWords);
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <span>
#include <string_view>
#include <vector>

namespace CosmicHex::Handheld {

class Canvas {
public:
  using Colour = ScratchPadd::Scene::Colour;
  // A picture of its own, `width` x `height`
  Canvas(int width, int height);
  // Drawing over pixels it doesn't own (another picture's, the same size),
  // so a picture drawn by something else needn't be copied to be drawn on
  // (round 105: the board's buttons drawn straight onto the map, not onto a
  // copy of it, 196 KB a redraw on a DS)
  static Canvas over(std::span<Colour> pixels, int width, int height);

  Canvas(const Canvas &other);
  Canvas &operator=(const Canvas &other);
  Canvas(Canvas &&) noexcept = default;
  Canvas &operator=(Canvas &&) noexcept = default;

  int width() const { return width_; }
  int height() const { return height_; }
  std::span<const Colour> pixels() const { return pixels_; }
  Colour at(int x, int y) const;

  void clear(Colour colour);
  // Another picture of the same size, as it is (nothing if it isn't)
  void copy(std::span<const Colour> pixels);
  void fillRect(float x, float y, float w, float h, Colour colour);
  void outline(float x, float y, float w, float h, Colour colour, float thickness = 1);
  // Words from their top left, or with their letters' pixels centred on
  // (x, y); `scale` as the overlay's
  void text(float x, float y, std::string_view words, Colour colour, float scale = 1);
  void textCentred(float x, float y, std::string_view words, Colour colour, float scale = 1);
  // `sprite` (RGBA, `w` x `h`, rows from the top) over the picture, its
  // top left at (x, y), by its alpha; `opacity` dims it (0 to 1)
  void blend(const std::vector<Colour> &sprite, int w, int h, int x, int y, float opacity = 1);

private:
  Canvas() = default;
  int width_ = 0, height_ = 0;
  std::vector<Colour> owned_; // (empty when drawing over another's)
  std::span<Colour> pixels_;  // what's drawn on: owned_'s, or another's
};

} // namespace CosmicHex::Handheld
