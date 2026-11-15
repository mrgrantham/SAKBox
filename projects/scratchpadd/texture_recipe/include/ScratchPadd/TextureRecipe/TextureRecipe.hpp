// TextureRecipe.hpp - textures drawn on the device, at start-up, from a
// recipe: an SVG drawing turned by the build into filled outlines and their
// colours (tools/svg_to_recipe.py), drawn here into pixels at whatever size
// and colour depth the device profile allows. No image files ship, and no
// device needs an SVG parser.
//
//   // BUILD: texture_recipe(name = "tile_faces", svg = "tile_faces.svg",
//   //                        namespace = "Art", symbol = "kTileFaces")
//   const auto recipe = TextureRecipe::read(std::string_view(
//       reinterpret_cast<const char *>(Art::kTileFaces), Art::kTileFacesSize)); // or an error
//   TextureRecipe::Image image = TextureRecipe::draw(*recipe, 64, 64,
//   TextureRecipe::Format::Rgba8);
//
// Everything is integer arithmetic (a DS has no floating point), and the
// same recipe draws the same pixels on every platform.
//
// Formats:
//   Rgba8       4 bytes a pixel, any colours, edges smoothed (16 samples a pixel)
//   Palette256  1 byte a pixel into `palette`: [0] transparent, then up to 255
//               colours; sharp edges (the pixel's centre decides)
//   Palette16   the same with up to 15 colours (a DS's 4-bit textures; still a
//               byte a pixel here, packed when it goes to the hardware)
// The palette is made for each drawing: its own colours, exactly, if they
// fit. If they don't, colours no pixel uses are dropped, then the closest
// are merged (each pair's distance weighted by how many pixels would change)
// until they fit, and the reduction is logged as a loud error and kept in
// `Image::reduction`. The same recipe then draws in full on a desktop and as
// near as fits on a DS, with nothing changed in the drawing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace ScratchPadd::TextureRecipe {

struct Colour {
  std::uint8_t r = 0, g = 0, b = 0, a = 255;
  bool operator==(const Colour &) const = default;
};

struct Point {
  std::int32_t x = 0, y = 0; // in 1/16ths of the drawing's units
  bool operator==(const Point &) const = default;
};

enum class FillRule : std::uint8_t { NonZero, EvenOdd };

// One shape: its colour, fill rule, and outlines (several make holes)
struct Fill {
  int colour = 0;
  FillRule rule = FillRule::NonZero;
  std::vector<std::vector<Point>> contours;
};

struct Recipe {
  int width = 0, height = 0; // the drawing's size, in its units
  std::vector<Colour> colours;
  std::vector<Fill> fills; // painted in order, each over the ones before
};

// Recipe units per drawing unit
inline constexpr int kUnitsPerDrawingUnit = 16;

// A recipe from its text, or what's wrong with it ("line 4: ...")
std::expected<Recipe, std::string> read(std::string_view text);

enum class Format : std::uint8_t { Rgba8, Palette256, Palette16 };

// How many colours a format's palette holds besides transparent (0: no palette)
constexpr std::size_t paletteLimit(Format format) {
  return format == Format::Palette256 ? 255 : format == Format::Palette16 ? 15 : 0;
}

// What making a palette did to a drawing's colours
struct Reduction {
  int coloursUsed = 0; // the recipe's colours, less any unused when it didn't fit
  int coloursKept = 0; // in the palette, after merging
  int worstShift = 0;  // the most any colour moved in one channel, of 255
  bool reduced() const { return coloursKept < coloursUsed; }
};

struct Image {
  int width = 0, height = 0;
  Format format = Format::Rgba8;
  std::vector<std::uint8_t> pixels; // rows top to bottom: 4 bytes a pixel, or 1
  std::vector<Colour> palette;      // palette formats: [0] transparent, then the colours
  Reduction reduction;              // palette formats

  Colour at(int x, int y) const; // a pixel's colour, either format
};

// The recipe drawn into a width x height image, the drawing stretched to fill
// it. `name` is for the log, if its colours have to be reduced.
Image draw(const Recipe &recipe, int width, int height, Format format, std::string_view name = {});

} // namespace ScratchPadd::TextureRecipe
