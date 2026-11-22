// Font.hpp - the overlay's letters as data every backend can draw, and text
// laid out and measured in them. Two kinds, one interface:
//
// - a pixel font (Silkscreen, the DS's): each letter's lit pixels as runs
//   along its rows, drawn at whole multiples of its size, filled into the
//   DS's text layer or a Canvas as rectangles (no texture);
// - a smooth font (Orbitron, the 3DS's and desktops'): each letter baked at
//   a few sizes into one alpha atlas, drawn as textured quads or blended
//   into a Canvas, the nearest size scaled.
//
// make_fonts.py writes their data (SilkscreenFont.cpp, OrbitronFont.cpp)
// from the fonts in third_party/fonts. Every printable ASCII character is
// there; any other is drawn as '?'.
//
//   const Font &font = Font::platform();
//   font.width("Build", 2);     // points, as Overlay::textWidth
#pragma once

#include <ScratchPadd/Scene/Overlay.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace ScratchPadd::Scene {

// The characters a font has, ' ' to '~', in order
constexpr char kFirstCharacter = ' ', kLastCharacter = '~';
constexpr std::size_t kCharacterCount = kLastCharacter - kFirstCharacter + 1;
// Where `c`'s letter is in a font's tables ('?''s for a character it hasn't)
std::size_t glyphIndex(char c);

// A pixel font's letter: its lit pixels (runCount runs from firstRun), and
// how far it moves the pen (4 bytes: no padding)
struct PixelGlyph {
  std::uint16_t firstRun;
  std::uint8_t runCount, advance;
};
// Lit pixels along a row, from the pen at the line's top
struct PixelRun {
  std::uint8_t x, y, length;
};
struct PixelFontData {
  int lineHeight;                     // pixels
  std::span<const PixelGlyph> glyphs; // (kCharacterCount)
  std::span<const PixelRun> runs;
};

// A smooth font's letter at one baked size: its picture in the atlas, and
// where it goes from the pen at the line's top (the size's pixels). A
// space has no picture (width 0)
struct AtlasGlyph {
  std::uint16_t x, y;
  std::uint8_t width, height;
  std::int8_t left, top;
};
// The letters baked for an overlay scale
struct BakedSize {
  float scale;
  std::span<const AtlasGlyph> glyphs; // (kCharacterCount)
};
struct SmoothFontData {
  float lineHeight;                 // points at scale 1
  std::span<const float> advances;  // points at scale 1 (kCharacterCount)
  std::span<const BakedSize> sizes; // smallest first
  int atlasWidth, atlasHeight;
  std::span<const std::uint8_t> atlas; // alpha, rows from the top
};

// An alpha picture a backend makes a texture of: a smooth font's letters,
// with a solid corner (the top-left 2 x 2) for the overlay's shapes
struct FontAtlas {
  int width, height;
  std::span<const std::uint8_t> alpha; // rows from the top
  // Texture coordinates inside the solid corner
  float solidU() const { return 1.0f / static_cast<float>(width); }
  float solidV() const { return 1.0f / static_cast<float>(height); }
};

// Part of a laid-out text, in points from the text's top left: a solid
// rectangle (a pixel font's run) or a letter's picture from the atlas
struct TextPiece {
  float x, y, width, height;
  std::uint16_t atlasX = 0, atlasY = 0, atlasWidth = 0, atlasHeight = 0;
  bool solid() const { return atlasWidth == 0; }
};

// A pixel font's run in its own pixels, the text starting at (0, 0): whole
// numbers, for a backend without floating point hardware (the DS), which
// sizes them itself (Font::sized)
struct FontRectangle {
  std::int16_t x, y, width, height;
};

class Font {
public:
  constexpr explicit Font(const PixelFontData &pixels) : pixels_(&pixels) {}
  constexpr explicit Font(const SmoothFontData &smooth) : smooth_(&smooth) {}

  static const Font &silkscreen();
  static const Font &orbitron();
  // The font this platform's overlay and Canvas draw: Silkscreen on the
  // DS, Orbitron elsewhere
  static const Font &platform();

  // How tall a line is, and how wide `text`'s widest line, in points at
  // `scale` ('\n' starts a line)
  float lineHeight(float scale = 1) const;
  float width(std::string_view text, float scale = 1) const;
  // The room added between letters at `scale`, in points (tracking): a
  // smooth font's letters crowd when small (the user's), so a point between
  // them at the small scales, less as they grow, none for titles. Width and
  // layout both add it, so what's measured is what's drawn. A pixel font's
  // letters keep the font's own one-pixel gap
  float tracking(float scale) const;
  // Where `text`'s letters' own pixels are, from its top left
  Overlay::Ink ink(std::string_view text, float scale = 1) const;
  // `text` as pieces into `out` (cleared first; kept by the caller, so no
  // allocation once it's grown)
  void layOut(std::string_view text, float scale, std::vector<TextPiece> &out) const;

  // A pixel font's size at `scale`, in half points: each of its pixels this
  // many halves of a point across and down (the half at or below the
  // scale, at least a whole point). Silkscreen's letters are as wide as
  // stb_easy_font's were at the same scale, so text is never wider than it
  // was laid out for, and as large: at whole points only, a title asking
  // for 1.5 was drawn at 1 (the user's). A smooth font's is 2
  int pixelHalves(float scale) const;
  // A pixel font's run at `halves` (pixelHalves'), on whole points from the
  // text's top left: at a half size its pixels alternate one and two points
  // (as stb_easy_font's strokes did at 1.5), each edge where the half
  // rounds down, so neighbouring runs meet. Whole numbers, for the DS
  static FontRectangle sized(const FontRectangle &run, int halves) {
    const auto at = [halves](int v) { return static_cast<std::int16_t>((v * halves) >> 1); };
    return {at(run.x), at(run.y), static_cast<std::int16_t>(at(run.x + run.width) - at(run.x)),
            static_cast<std::int16_t>(at(run.y + run.height) - at(run.y))};
  }
  // A pixel font's runs for `text` in its own pixels into `out` (cleared
  // first); none for a smooth font
  void pixelRectangles(std::string_view text, std::vector<FontRectangle> &out) const;

  // The picture a backend draws letters from (a pixel font's is its solid
  // corner alone)
  FontAtlas atlas() const;
  const PixelFontData *pixels() const { return pixels_; }
  const SmoothFontData *smooth() const { return smooth_; }

private:
  const BakedSize &sizeFor(float scale) const;
  void layOutPixels(std::string_view text, float scale, std::vector<TextPiece> &out) const;
  void layOutSmooth(std::string_view text, float scale, std::vector<TextPiece> &out) const;

  const PixelFontData *pixels_ = nullptr;
  const SmoothFontData *smooth_ = nullptr;
};

} // namespace ScratchPadd::Scene
