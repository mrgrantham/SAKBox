// Texture recipes: reading one, and drawing it into pixels with integer
// arithmetic (see TextureRecipe.hpp).
//
// Drawing: each fill's outlines become edges in sample space (16.16 fixed
// point; 4 x 4 samples a pixel for Rgba8, 1 for Indexed). For each row of
// samples, the edges crossing its centre are found and sorted, and the fill
// rule (the winding number, or crossings counted odd or even) says which
// spans are inside; each sample inside adds to its pixel's coverage. Then
// the fill's colour is laid over the image by coverage.
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <algorithm>
#include <charconv>
#include <cstdlib>

namespace ScratchPadd::TextureRecipe {

namespace {

std::vector<std::string_view> words(std::string_view line) {
  std::vector<std::string_view> out;
  while (!line.empty()) {
    const std::size_t start = line.find_first_not_of(' ');
    if (start == std::string_view::npos)
      break;
    line.remove_prefix(start);
    const std::size_t end = line.find(' ');
    out.push_back(line.substr(0, end));
    line.remove_prefix(end == std::string_view::npos ? line.size() : end);
  }
  return out;
}

// (a template: a DS's int32_t is a long, not an int)
template <typename Int> bool number(std::string_view text, Int &out) {
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
  return error == std::errc{} && end == text.data() + text.size();
}

struct Edge {
  std::int64_t x0, y0, x1, y1; // sample space, 16.16, y0 < y1
  int direction;               // +1 drawn downwards, -1 upwards
};

struct Crossing {
  std::int64_t x;
  int direction;
};

} // namespace

std::expected<Recipe, std::string> read(std::string_view text) {
  Recipe recipe;
  int lineNumber = 0;
  bool sawHeader = false;
  const auto fail = [&](const std::string &why) {
    return std::unexpected("line " + std::to_string(lineNumber) + ": " + why);
  };
  while (!text.empty()) {
    const std::size_t end = text.find('\n');
    const std::string_view line = text.substr(0, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    ++lineNumber;
    const std::vector<std::string_view> w = words(line);
    if (w.empty())
      continue;
    if (!sawHeader) {
      if (w.size() != 2 || w[0] != "recipe" || w[1] != "1")
        return fail("not a texture recipe (version 1)");
      sawHeader = true;
    } else if (w[0] == "size" && w.size() == 3) {
      if (!number(w[1], recipe.width) || !number(w[2], recipe.height) || recipe.width <= 0 ||
          recipe.height <= 0)
        return fail("a size that isn't two positive numbers");
    } else if (w[0] == "colour" && w.size() == 6) {
      int index = 0, rgba[4] = {};
      for (std::size_t i = 0; i < 4; ++i) {
        if (!number(w[2 + i], rgba[i]) || rgba[i] < 0 || rgba[i] > 255)
          return fail("a colour that isn't four numbers from 0 to 255");
      }
      if (!number(w[1], index) || index != static_cast<int>(recipe.colours.size()))
        return fail("colours out of order");
      recipe.colours.push_back(
          {static_cast<std::uint8_t>(rgba[0]), static_cast<std::uint8_t>(rgba[1]),
           static_cast<std::uint8_t>(rgba[2]), static_cast<std::uint8_t>(rgba[3])});
    } else if (w[0] == "fill" && w.size() == 3) {
      Fill fill;
      if (!number(w[1], fill.colour) || fill.colour < 0 ||
          fill.colour >= static_cast<int>(recipe.colours.size()))
        return fail("a fill whose colour isn't listed");
      if (w[2] == "evenodd")
        fill.rule = FillRule::EvenOdd;
      else if (w[2] != "nonzero")
        return fail("a fill rule that isn't nonzero or evenodd");
      recipe.fills.push_back(std::move(fill));
    } else if (w[0] == "contour") {
      if (recipe.fills.empty())
        return fail("an outline before any fill");
      if (w.size() < 7 || (w.size() - 1) % 2 != 0)
        return fail("an outline needs three points or more");
      std::vector<Point> points;
      for (std::size_t i = 1; i + 1 < w.size(); i += 2) {
        Point p;
        if (!number(w[i], p.x) || !number(w[i + 1], p.y))
          return fail("an outline's points aren't numbers");
        points.push_back(p);
      }
      recipe.fills.back().contours.push_back(std::move(points));
    } else {
      return fail("a line I can't read: " + std::string(line));
    }
  }
  if (!sawHeader || recipe.width == 0)
    return std::unexpected("not a whole recipe (no header or no size)");
  return recipe;
}

Colour Image::at(int x, int y) const {
  const std::size_t i =
      static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
  if (format == Format::Indexed)
    return palette[pixels[i]];
  return {pixels[4 * i], pixels[4 * i + 1], pixels[4 * i + 2], pixels[4 * i + 3]};
}

Image draw(const Recipe &recipe, int width, int height, Format format) {
  Image image;
  image.width = width;
  image.height = height;
  image.format = format;
  const int samples = format == Format::Rgba8 ? 4 : 1; // a side
  const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  image.pixels.assign(pixelCount * (format == Format::Rgba8 ? 4 : 1), 0);
  if (format == Format::Indexed) {
    image.palette.push_back({0, 0, 0, 0});
    image.palette.insert(image.palette.end(), recipe.colours.begin(), recipe.colours.end());
  }
  const int sampleWidth = width * samples, sampleHeight = height * samples;
  // A recipe point in sample space, 16.16
  const std::int64_t unitsWide = std::int64_t{recipe.width} * kUnitsPerDrawingUnit,
                     unitsHigh = std::int64_t{recipe.height} * kUnitsPerDrawingUnit;
  const auto sampleX = [&](std::int32_t x) {
    return (std::int64_t{x} * sampleWidth << 16) / unitsWide;
  };
  const auto sampleY = [&](std::int32_t y) {
    return (std::int64_t{y} * sampleHeight << 16) / unitsHigh;
  };

  std::vector<std::uint16_t> coverage(pixelCount);
  std::vector<Edge> edges;
  std::vector<Crossing> crossings;
  for (const Fill &fill : recipe.fills) {
    edges.clear();
    for (const std::vector<Point> &contour : fill.contours) {
      for (std::size_t i = 0; i < contour.size(); ++i) {
        const Point a = contour[i], b = contour[(i + 1) % contour.size()];
        const std::int64_t ax = sampleX(a.x), ay = sampleY(a.y), bx = sampleX(b.x),
                           by = sampleY(b.y);
        if (ay == by)
          continue; // flat: never crossed by a row's centre
        edges.push_back(ay < by ? Edge{ax, ay, bx, by, 1} : Edge{bx, by, ax, ay, -1});
      }
    }
    std::fill(coverage.begin(), coverage.end(), 0);
    for (int row = 0; row < sampleHeight; ++row) {
      const std::int64_t centre = (std::int64_t{row} << 16) + (1 << 15);
      crossings.clear();
      for (const Edge &e : edges) {
        if (centre < e.y0 || centre >= e.y1)
          continue;
        const std::int64_t x = e.x0 + (e.x1 - e.x0) * (centre - e.y0) / (e.y1 - e.y0);
        crossings.push_back({x, e.direction});
      }
      std::sort(crossings.begin(), crossings.end(),
                [](const Crossing &a, const Crossing &b) { return a.x < b.x; });
      int winding = 0;
      for (std::size_t i = 0; i + 1 < crossings.size(); ++i) {
        winding += fill.rule == FillRule::EvenOdd ? 1 : crossings[i].direction;
        const bool inside = fill.rule == FillRule::EvenOdd ? (winding % 2) != 0 : winding != 0;
        if (!inside)
          continue;
        // Samples whose centres are in [crossing i, crossing i+1)
        const std::int64_t from = (crossings[i].x - (1 << 15) + 0xffff) >> 16;
        const std::int64_t to = (crossings[i + 1].x - (1 << 15) + 0xffff) >> 16;
        for (std::int64_t column = std::max<std::int64_t>(from, 0);
             column < std::min<std::int64_t>(to, sampleWidth); ++column) {
          const std::size_t pixel =
              static_cast<std::size_t>(row / samples) * static_cast<std::size_t>(width) +
              static_cast<std::size_t>(column / samples);
          ++coverage[pixel];
        }
      }
    }
    // Laid over what's there, by coverage
    const Colour colour = recipe.colours[static_cast<std::size_t>(fill.colour)];
    const unsigned full = static_cast<unsigned>(samples * samples);
    for (std::size_t p = 0; p < pixelCount; ++p) {
      if (coverage[p] == 0)
        continue;
      if (format == Format::Indexed) {
        image.pixels[p] = static_cast<std::uint8_t>(fill.colour + 1);
        continue;
      }
      std::uint8_t *dst = &image.pixels[4 * p];
      // Source over, in 0..255 with coverage: alpha = colour.a x covered/full
      const unsigned alpha = colour.a * coverage[p] / full;
      const unsigned outAlpha = alpha + dst[3] * (255 - alpha) / 255;
      if (outAlpha == 0)
        continue;
      const auto mix = [&](std::uint8_t s, std::uint8_t d) {
        return static_cast<std::uint8_t>((s * alpha + d * dst[3] * (255 - alpha) / 255) / outAlpha);
      };
      dst[0] = mix(colour.r, dst[0]);
      dst[1] = mix(colour.g, dst[1]);
      dst[2] = mix(colour.b, dst[2]);
      dst[3] = static_cast<std::uint8_t>(outAlpha);
    }
  }
  return image;
}

} // namespace ScratchPadd::TextureRecipe
