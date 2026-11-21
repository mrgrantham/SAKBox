// BoardPicture.hpp - Cosmic Hex's board drawn flat, into pixels, for a
// console's bottom screen to show and be touched (the 3DS and the
// DS share it; the DS can't draw 3D on both screens). The tiles' faces are
// the same SVG drawings as the 3D board's (Art.hpp), drawn at the hexes'
// size; the pieces are simple shapes in the players' colours. It zooms (the
// whole board, or closer, moved about by dragging) and says what a touch is
// on: the corner nearest it, and the hex it's in.
//
//   BoardPicture picture(320, 240);       // the 3DS's bottom screen
//   picture.draw(state, {.corners = {selected}});
//   picture.pixels()                      // RGBA, rows from the top
//   picture.changed()                     // the areas that draw changed
//   picture.at(touchX, touchY)            // the corner (and hex) touched
//
// A draw changes only what has changed since the last: each
// piece (a tile's face, a chip, a station, a ship, the blockade, a mark) is
// listed with what it looks like and the area it covers; the areas where a
// piece has gone, come or changed are drawn again from scratch (space, the
// grid, every piece over them, in order), and the rest of the picture is
// left as it is. So after a move only a few small areas are drawn, and the
// picture is always the same as drawing it all (BoardPicture_test checks
// that over whole games). Something else drawn over the picture (the
// bottom screen's buttons) asks for its area to be drawn again first
// (redrawNext).
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
  // as they come (it took seconds, every face drawn first)
  bool prepare();
  // Whether every face and chip at this zoom is drawn sharp
  bool ready() const;
  // The most memory a zoom's sharp faces and chips may take, in bytes.
  // Zoomed in, only the faces on screen are drawn sharp, as many as fit
  // (a cache: the one seen longest ago goes for one newly in view); zooming
  // stops at the closest zoom where its chips and kFewestSharpFaces fit
  // (mostZoom). On a DS all 19 faces at zoom 2 took 240 KB, and the game
  // ran out of memory and stopped
  void setZoomBudget(std::size_t bytes);
  static constexpr int kFewestSharpFaces = 6;
  // The closest zoom there is: kMostZoom, or less within the budget
  int mostZoom() const;
  // How many faces may be sharp at once at this zoom (within the budget)
  int mostSharpFaces() const;
  // How much memory is left to allocate (MemoryStats::Heap::room), told each
  // frame where it's known (the DS), and how much of it to leave for the
  // rest of the game: a zoom's sharp faces and chips are drawn only while
  // they leave `reserve`, and let go if what's left falls below half of it
  // (the stand-ins shown, the zoom kept). The game ran out of memory and
  // stopped zoomed in as a turn ended
  void setMemoryRoom(std::size_t room, std::size_t reserve);
  // A tile face as the whole board shows it (zoom 1), once prepare() has
  // drawn it, wholeFaceSize() a side; for other pictures of the same face
  // to be made from, rather than drawn from its recipe again
  const std::vector<ScratchPadd::Scene::Colour> &wholeFace(Art::Face face) const {
    return whole_.faces[static_cast<std::size_t>(face)];
  }
  int wholeFaceSize() const { return whole_.facesSize; }

  // What's marked over the board, in the highlight's colour: a hex outlined
  // (the blockade goes on a hex), and corners as dots
  struct Marks {
    std::optional<Hex> hex;
    // ...and hexes outlined thinly (where the blockade can go)
    std::vector<Hex> hexes;
    std::vector<Corner> corners;
    // ...and corners marked as an outline (a ship's moves that need fuel)
    std::vector<Corner> fuelCorners;
    // A piece shown where it would go, before it's placed (the start's
    // home colony or ship), over everything
    struct Preview {
      Corner corner;
      int owner = 0;
      bool ship = false; // (else a colony)
    };
    std::optional<Preview> preview;
    // The board's pointer (the top screen's chevron), flat and still: a
    // white chevron just above the corner in focus, over everything
    std::optional<Corner> pointer;
    // A tile just turned over that the map doesn't show yet: face down
    // (shown 0), its face fading in over it (to 1, as it is); the top
    // screen shows it turning over first (Discovery)
    struct Revealing {
      Hex hex;
      float shown = 0;
    };
    std::optional<Revealing> revealing;
    // Tiles turned over but still waiting their turn on the top screen:
    // shown face down (no copy of the state needed: the DS's main stack is
    // small)
    std::vector<Hex> faceDown;
  };
  // An area of the picture, in pixels
  struct Area {
    int x = 0, y = 0, width = 0, height = 0;
    auto operator<=>(const Area &) const = default;
  };

  // The board as `state` has it, with `marks`, into pixels(): only the
  // areas that changed since the last draw (all of it the first time, or
  // when zoomed or moved), which changed() then gives
  void draw(const State &state, const Marks &marks = {});
  const std::vector<Area> &changed() const { return changed_; }
  // `area` drawn again by the next draw, whatever changes: where something
  // else has drawn over the picture, or is about to
  void redrawNext(Area area);
  // All of it drawn again by the next draw
  void redrawAll();
  const std::vector<ScratchPadd::Scene::Colour> &pixels() const { return pixels_; }
  // The same, to draw over (the bottom screen's buttons, onto the map
  // itself: Canvas::over)
  std::span<ScratchPadd::Scene::Colour> pixelsToDrawOver() { return pixels_; }

  // What's under a touch at (x, y): the hex it's in, and that hex's corner
  // nearest it (the user's: a touch picks a corner, where the
  // pieces go, not a whole tile); nothing off the board
  struct Spot {
    Hex hex;
    Corner corner;
    bool operator==(const Spot &) const = default;
  };
  std::optional<Spot> at(int x, int y) const;
  // Where a hex's centre or a corner is on the screen, in pixels
  HexGrid::Point where(Hex hex) const;
  HexGrid::Point where(Corner corner) const;

private:
  HexGrid::Layout layout() const;
  float hexSize() const; // centre to corner, in pixels, at this zoom
  float hexSizeAt(int zoom) const;

  void buildGrid(); // (gridPixels_ for this zoom and pan)

  // One thing drawn on the board: what it is, where (a hex, and a corner's
  // index), what it looks like (a face and whether it's sharp, a chip's
  // number, a player and whether a colony), and the pixels it can touch
  struct Piece {
    enum class Kind : std::uint8_t {
      Face,
      Chip,
      Station,
      Ship,
      Blockade,
      MarkHex,
      MarkCorner,
      Pointer
    };
    Kind kind;
    std::array<std::int16_t, 3> place;
    std::array<std::int16_t, 2> look;
    Area area;
    auto operator<=>(const Piece &) const = default;
  };
  // Everything drawn for `state` and `marks`, in the order it's drawn
  void listPieces(const State &state, const Marks &marks, std::vector<Piece> &out) const;
  void drawPiece(const Piece &piece);
  // `area` drawn from scratch: space, the grid, the pieces over it
  void drawArea(Area area);
  static Area triangleArea(HexGrid::Point a, HexGrid::Point b, HexGrid::Point c);

  std::uint8_t fade_ = 255; // (blendSprite's opacity: a face fading in)
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
  Sprites whole_;                                         // (at zoom 1, kept)
  std::size_t zoomBudget_ = static_cast<std::size_t>(-1); // (setZoomBudget)
  std::size_t zoomBytes(int zoom, int faces) const; // (a zoom's chips and `faces` sharp faces)
  Sprites zoomed_; // (at the current zoom, when it's not 1: its chips, and
                   // the faces on screen)
  std::size_t room_ = static_cast<std::size_t>(-1), reserve_ = 0; // (setMemoryRoom)
  bool roomFor(std::size_t bytes) const;
  // When each face was last on screen (draws_ counts draws): the faces in
  // the last draw are wanted sharp; the one seen longest ago goes first
  std::array<std::uint32_t, Art::kFaces> seen_{};
  std::uint32_t draws_ = 0;
  // The face to draw sharp next (making room, if it must, by letting one go
  // that isn't on screen), or -1: none wanted, or no room
  int faceToDraw();
  bool faceWaiting() const; // (one wanted, not sharp, that could be drawn)
  int facesSize() const;
  int facesSizeAt(int zoom) const;
  int chipsSize() const;
  int chipsSizeAt(int zoom) const;
  // A face or chip at this zoom: its own when drawn, else the whole
  // board's, scaled up as it's blended
  void blendSpriteOf(const std::vector<ScratchPadd::Scene::Colour> &own,
                     const std::vector<ScratchPadd::Scene::Colour> &whole, int ownSize,
                     int wholeSize, HexGrid::Point centre);
  // What's drawn, and was last time (compared, to find what changed)
  std::vector<Piece> pieces_, drawnPieces_;
  // Seen as last drawn: zoom and pan (none: all of it drawn next)
  struct View {
    int zoom;
    float x, y;
    bool operator==(const View &) const = default;
  };
  std::optional<View> drawnView_;
  std::vector<Area> waiting_; // (redrawNext's, for the next draw)
  std::vector<Area> changed_; // (the last draw's)
  // More separate areas than this are drawn as one round them all
  static constexpr std::size_t kMostAreas = 12;
  Area clip_; // where the drawing functions draw
  inline static const std::vector<ScratchPadd::Scene::Colour> kNoSprite{};
  // The grid's pixels (indexes into pixels_), worked out once for a zoom
  // and pan rather than its 360 edges drawn each time
  std::vector<std::uint32_t> gridPixels_;
  int gridZoom_ = 0;
  HexGrid::Point gridOffset_{};
};

} // namespace CosmicHex::Handheld
