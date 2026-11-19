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

#include <string_view>
#include <vector>

namespace CosmicHex::Handheld {

class Canvas {
public:
  using Colour = ScratchPadd::Scene::Colour;
  Canvas(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }
  const std::vector<Colour> &pixels() const { return pixels_; }
  Colour at(int x, int y) const;

  void clear(Colour colour);
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
  int width_, height_;
  std::vector<Colour> pixels_;
};

} // namespace CosmicHex::Handheld
