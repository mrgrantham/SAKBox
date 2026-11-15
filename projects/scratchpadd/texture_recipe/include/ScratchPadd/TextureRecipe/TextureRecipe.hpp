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
//   Rgba8    4 bytes a pixel, edges smoothed (16 samples a pixel)
//   Indexed  1 byte a pixel: 0 transparent, then the recipe's colours (1 is
//            its first), sharp edges (the pixel's centre decides). A recipe's
//            few colours are its palette exactly, so a DS's 16-colour
//            textures need recipes of 15 colours or fewer.
#pragma once

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

enum class Format : std::uint8_t { Rgba8, Indexed };

struct Image {
  int width = 0, height = 0;
  Format format = Format::Rgba8;
  std::vector<std::uint8_t> pixels; // rows top to bottom: 4 bytes a pixel, or 1
  std::vector<Colour> palette;      // Indexed: [0] transparent, then the recipe's colours

  Colour at(int x, int y) const; // a pixel's colour, either format
};

// The recipe drawn into a width x height image, the drawing stretched to fill it
Image draw(const Recipe &recipe, int width, int height, Format format);

} // namespace ScratchPadd::TextureRecipe
