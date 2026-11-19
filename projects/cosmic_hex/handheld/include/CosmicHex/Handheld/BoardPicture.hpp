// BoardPicture.hpp - Cosmic Hex's board drawn flat, into pixels, for a
// console's bottom screen to show and be touched (round 71: the 3DS and the
// DS share it; the DS can't draw 3D on both screens). The tiles' faces are
// the same SVG drawings as the 3D board's (Art.hpp), drawn at the hexes'
// size; the pieces are simple shapes in the players' colours. It zooms (the
// whole board, or closer, moved about by dragging) and says what a touch is
// on: the corner nearest it, and the hex it's in.
//
//   BoardPicture picture(320, 240);       // the 3DS's bottom screen
//   picture.draw(state);
//   picture.pixels()                      // RGBA, rows from the top
//   picture.at(touchX, touchY)            // the corner (and hex) touched
//
// No console here: the app copies the pixels to its screen.
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Rules.hpp>

#include <HexGrid/Layout.hpp>
#include <ScratchPadd/Scene/Types.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace CosmicHex::Handheld {

class BoardPicture {
public:
  // `smoothing`: the tile faces' edges (TextureRecipe; Coarse on the DS)
  BoardPicture(int width, int height,
               ScratchPadd::TextureRecipe::Smoothing smoothing =
                   ScratchPadd::TextureRecipe::Smoothing::Fine);

  int width() const { return width_; }
  int height() const { return height_; }

  // 1: the whole board; 2 and 3 closer (kMostZoom), round the same centre
  static constexpr int kMostZoom = 3;
  int zoom() const { return zoom_; }
  void setZoom(int zoom);
  // Moves the board under the screen by (dx, dy) pixels, as a drag does,
  // keeping some of it in view
  void pan(float dx, float dy);

  // Draws one of the tile faces or number chips, if one's left, and says
  // whether they're all ready. Each is drawn from its recipe (about a tenth
  // of a second on a DS, nearly 3 s for all of them), so an app calls this
  // in spare frames: first the whole board's (zoom 1, kept for good; round
  // 92: the DS's first map stalled the start of a game, so draw() finishes
  // those), then the current zoom's. Until a zoom's own are drawn, draw()
  // shows the whole board's scaled up, so zooming is at once and sharpens
  // as they come (round 105: it took seconds, every face drawn first)
  bool prepare();
  // Whether every face and chip at this zoom is drawn sharp
  bool ready() const;

  // The board as `state` has it, into pixels()
  void draw(const State &state);
  const std::vector<ScratchPadd::Scene::Colour> &pixels() const { return pixels_; }
  // The same, to draw over (the bottom screen's buttons, onto the map
  // itself: Canvas::over)
  std::span<ScratchPadd::Scene::Colour> pixelsToDrawOver() { return pixels_; }

  // What's under a touch at (x, y): the hex it's in, and that hex's corner
  // nearest it (round 93, the user's: a touch picks a corner, where the
  // pieces go, not a whole tile); nothing off the board
  struct Spot {
    Hex hex;
    Corner corner;
    bool operator==(const Spot &) const = default;
  };
  std::optional<Spot> at(int x, int y) const;
  // Marks a corner over the drawn board (after draw) as a dot, or a hex as
  // an outline (the blockade goes on a hex), in the highlight's colour
  void markCorner(Corner corner);
  void markHex(Hex hex);

  // Where a hex's centre or a corner is on the screen, in pixels
  HexGrid::Point where(Hex hex) const;
  HexGrid::Point where(Corner corner) const;

private:
  HexGrid::Layout layout() const;
  float hexSize() const; // centre to corner, in pixels, at this zoom

  void buildGrid(); // (gridPixels_ for this zoom and pan)
  void blendFace(Art::Face face, HexGrid::Point centre);
  void blendSprite(const std::vector<ScratchPadd::Scene::Colour> &sprite, int size,
                   HexGrid::Point centre);
  void fillRect(float x, float y, float w, float h, ScratchPadd::Scene::Colour colour);
  void line(HexGrid::Point a, HexGrid::Point b, int thickness, ScratchPadd::Scene::Colour colour);
  template <typename Plot>
  void forEachLinePixel(HexGrid::Point a, HexGrid::Point b, int thickness, Plot plot) const;
  void fillTriangle(HexGrid::Point a, HexGrid::Point b, HexGrid::Point c,
                    ScratchPadd::Scene::Colour colour);

  int width_, height_;
  ScratchPadd::TextureRecipe::Smoothing smoothing_;
  int zoom_ = 1;
  HexGrid::Point offset_{}; // the board moved by panning, in pixels
  std::vector<ScratchPadd::Scene::Colour> pixels_;
  // A set of the faces and chips at one size: each face a square of pixels
  // facesSize a side (RGBA), each chip (indexed by its number) chipsSize
  struct Sprites {
    int facesSize = 0, chipsSize = 0;
    std::array<std::vector<ScratchPadd::Scene::Colour>, Art::kFaces> faces;
    std::array<std::vector<ScratchPadd::Scene::Colour>, Art::kHighestChip + 1> chips;
    int next = 0; // the next to draw: faces first, then chips
    bool done() const;
    // Draws the next; whether that was the last
    void drawNext(ScratchPadd::TextureRecipe::Smoothing smoothing);
    void startAt(int faceSize, int chipSize);
  };
  Sprites whole_;  // (at zoom 1, kept)
  Sprites zoomed_; // (at the current zoom, when it's not 1)
  int facesSize() const;
  int chipsSize() const;
  // A face or chip at this zoom: its own when drawn, else the whole
  // board's, scaled up as it's blended
  void blendSpriteOf(const std::vector<ScratchPadd::Scene::Colour> &own,
                     const std::vector<ScratchPadd::Scene::Colour> &whole, int ownSize,
                     int wholeSize, HexGrid::Point centre);
  // The grid's pixels (indexes into pixels_), worked out once for a zoom
  // and pan rather than its 360 edges drawn each time (round 92)
  std::vector<std::uint32_t> gridPixels_;
  int gridZoom_ = 0;
  HexGrid::Point gridOffset_{};
};

} // namespace CosmicHex::Handheld
