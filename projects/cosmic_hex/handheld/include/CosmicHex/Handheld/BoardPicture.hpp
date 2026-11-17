// BoardPicture.hpp - Cosmic Hex's board drawn flat, into pixels, for a
// console's bottom screen to show and be touched (round 71: the 3DS and the
// DS share it; the DS can't draw 3D on both screens). The tiles' faces are
// the same SVG drawings as the 3D board's (Art.hpp), drawn at the hexes'
// size; the pieces are simple shapes in the players' colours. It zooms (the
// whole board, or closer, moved about by dragging) and says what a touch is
// on: a hex, or a corner of one.
//
//   BoardPicture picture(320, 240);       // the 3DS's bottom screen
//   picture.draw(state);
//   picture.pixels()                      // RGBA, rows from the top
//   picture.at(touchX, touchY)            // the hex and corner touched
//
// No console here: the app copies the pixels to its screen.
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Rules.hpp>

#include <HexGrid/Layout.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <array>
#include <optional>
#include <vector>

namespace CosmicHex::Handheld {

class BoardPicture {
public:
  BoardPicture(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }

  // 1: the whole board; 2 and 3 closer (kMostZoom), round the same centre
  static constexpr int kMostZoom = 3;
  int zoom() const { return zoom_; }
  void setZoom(int zoom);
  // Moves the board under the screen by (dx, dy) pixels, as a drag does,
  // keeping some of it in view
  void pan(float dx, float dy);

  // The board as `state` has it, into pixels()
  void draw(const State &state);
  const std::vector<ScratchPadd::Scene::Colour> &pixels() const { return pixels_; }

  // What's under a touch at (x, y): the hex, and the corner of it when the
  // touch is close to one; nothing off the board
  struct Spot {
    Hex hex;
    std::optional<Corner> corner;
  };
  std::optional<Spot> at(int x, int y) const;
  // Marks a spot over the drawn board (after draw): its hex outlined, and
  // its corner as a dot, in the highlight's colour
  void mark(const Spot &spot);

  // Where a hex's centre or a corner is on the screen, in pixels
  HexGrid::Point where(Hex hex) const;
  HexGrid::Point where(Corner corner) const;

private:
  HexGrid::Layout layout() const;
  float hexSize() const; // centre to corner, in pixels, at this zoom
  void ensureFaces();    // each face drawn at this zoom's size
  void blendFace(Art::Face face, HexGrid::Point centre);
  void fillRect(float x, float y, float w, float h, ScratchPadd::Scene::Colour colour);
  void line(HexGrid::Point a, HexGrid::Point b, float thickness, ScratchPadd::Scene::Colour colour);
  void fillTriangle(HexGrid::Point a, HexGrid::Point b, HexGrid::Point c,
                    ScratchPadd::Scene::Colour colour);

  int width_, height_;
  int zoom_ = 1;
  HexGrid::Point offset_{}; // the board moved by panning, in pixels
  std::vector<ScratchPadd::Scene::Colour> pixels_;
  // Each face as a square of pixels at facesSize_ a side (RGBA)
  int facesSize_ = 0;
  std::array<std::vector<ScratchPadd::Scene::Colour>, Art::kFaces> faces_;
};

} // namespace CosmicHex::Handheld
