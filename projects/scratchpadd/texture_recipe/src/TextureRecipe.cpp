// Texture recipes: reading one, and drawing it into pixels with integer
// arithmetic (see TextureRecipe.hpp).
//
// Drawing: each fill's outlines become edges in sample space (16.16 fixed
// point; 4 x 4 samples a pixel for Rgba8, 1 for the palette formats). For each row of
// samples, the edges crossing its centre are found and sorted, and the fill
// rule (the winding number, or crossings counted odd or even) says which
// spans are inside; each sample inside adds to its pixel's coverage. Then
// the fill's colour is laid over the image by coverage.
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <algorithm>
#include <charconv>
#include <cstdlib>

namespace ScratchPadd::TextureRecipe {

namespace {

constexpr int kMaxColours = 4096; // (a pixel's recipe colour is 16-bit while drawing)

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

// --- Making a palette (the palette formats)
//
// A palette entry stands for a group of the recipe's colours: at first one
// colour each, and more when colours have to be merged to fit.
//
//   1. Count how many pixels each recipe colour covers.
//   2. Start a group for each colour (leaving out colours no pixel uses, if
//      there are too many colours).
//   3. While there are more groups than the palette holds, merge the pair
//      that changes the picture least.
//   4. Make the palette from the groups, and the pixels into its indices.
//   5. If any colours were merged, say so loudly.

// How different two colours look, squared: green counts most, as the eye
// sees it, then blue and alpha, then red
std::int64_t colourDistance(const Colour &a, const Colour &b) {
  const auto squared = [](int x, int y) { return std::int64_t{x - y} * (x - y); };
  return 2 * squared(a.r, b.r) + 4 * squared(a.g, b.g) + 3 * squared(a.b, b.b) +
         3 * squared(a.a, b.a);
}

// The most a colour moved in any one channel, 0 to 255
int colourShift(const Colour &was, const Colour &now) {
  return std::max({std::abs(was.r - now.r), std::abs(was.g - now.g), std::abs(was.b - now.b),
                   std::abs(was.a - now.a)});
}

struct ColourGroup {
  std::vector<std::size_t> recipeColours; // which of the recipe's colours it stands for
  // Its pixels' colours summed, so the group's colour is their average
  std::int64_t pixels = 0, red = 0, green = 0, blue = 0, alpha = 0;

  ColourGroup(std::size_t recipeColour, const Colour &colour, std::int64_t pixelCount)
      : recipeColours{recipeColour}, pixels(std::max<std::int64_t>(pixelCount, 1)) {
    red = colour.r * pixels;
    green = colour.g * pixels;
    blue = colour.b * pixels;
    alpha = colour.a * pixels;
  }

  Colour colour() const {
    const auto average = [&](std::int64_t sum) {
      return static_cast<std::uint8_t>((sum + pixels / 2) / pixels);
    };
    return {average(red), average(green), average(blue), average(alpha)};
  }

  void absorb(const ColourGroup &other) {
    recipeColours.insert(recipeColours.end(), other.recipeColours.begin(),
                         other.recipeColours.end());
    pixels += other.pixels;
    red += other.red;
    green += other.green;
    blue += other.blue;
    alpha += other.alpha;
  }
};

// Step 1: pixels covered by each recipe colour (`recipeColourOf` holds each
// pixel's recipe colour + 1, 0 for none)
std::vector<std::int64_t> countPixels(std::size_t colourCount,
                                      const std::vector<std::uint16_t> &recipeColourOf) {
  std::vector<std::int64_t> pixels(colourCount, 0);
  for (std::uint16_t colour : recipeColourOf) {
    if (colour != 0)
      ++pixels[colour - 1u];
  }
  return pixels;
}

// Step 2
std::vector<ColourGroup> startingGroups(const std::vector<Colour> &colours,
                                        const std::vector<std::int64_t> &pixels,
                                        std::size_t limit) {
  const bool tooMany = colours.size() > limit;
  std::vector<ColourGroup> groups;
  for (std::size_t c = 0; c < colours.size(); ++c) {
    if (tooMany && pixels[c] == 0)
      continue; // unused: drop it, making room for free
    groups.emplace_back(c, colours[c], pixels[c]);
  }
  return groups;
}

// Step 3: what merging two groups costs the picture: how far their colours
// are apart, times how many pixels would change (Ward's rule: n_a n_b /
// (n_a + n_b)). So a small detail moves to a big area's colour, not the
// other way round.
std::int64_t mergeCost(const ColourGroup &a, const ColourGroup &b) {
  return colourDistance(a.colour(), b.colour()) * a.pixels * b.pixels / (a.pixels + b.pixels);
}

void mergeCheapestPair(std::vector<ColourGroup> &groups) {
  std::size_t keep = 0, merge = 1;
  std::int64_t cheapest = mergeCost(groups[0], groups[1]);
  for (std::size_t a = 0; a < groups.size(); ++a) {
    for (std::size_t b = a + 1; b < groups.size(); ++b) {
      const std::int64_t cost = mergeCost(groups[a], groups[b]);
      if (cost < cheapest) {
        cheapest = cost;
        keep = a;
        merge = b;
      }
    }
  }
  groups[keep].absorb(groups[merge]);
  groups.erase(groups.begin() + static_cast<std::ptrdiff_t>(merge));
}

// Step 5
void reportReduction(const Image &image, std::string_view name) {
  const Reduction &r = image.reduction;
  auto log = Logger::loggerNamed("TextureRecipe");
  const std::string bar(80, '!');
  log->error("{}", bar);
  log->error("!!! TEXTURE COLOURS REDUCED: {} uses {} colours; {} holds {}, so {} were merged",
             name.empty() ? "a texture" : name, r.coloursUsed,
             image.format == Format::Palette16 ? "Palette16" : "Palette256",
             paletteLimit(image.format), r.coloursUsed - r.coloursKept);
  log->error("!!! The worst colour moved by {} (of 255) in one channel. Simplify the drawing's "
             "colours, or draw it as Palette256 or Rgba8, to keep them all.",
             r.worstShift);
  log->error("{}", bar);
}

// Steps 1 to 5: the image's palette, and its pixels as indices into it
void makePalette(Image &image, const std::vector<Colour> &colours,
                 const std::vector<std::uint16_t> &recipeColourOf, std::string_view name) {
  const std::size_t limit = paletteLimit(image.format);
  std::vector<ColourGroup> groups =
      startingGroups(colours, countPixels(colours.size(), recipeColourOf), limit);
  image.reduction.coloursUsed = static_cast<int>(groups.size());
  while (groups.size() > limit)
    mergeCheapestPair(groups);
  // Step 4. Entry 0 is transparent; a recipe colour dropped stays 0 (it
  // has no pixels to need it)
  image.palette.assign(1, Colour{0, 0, 0, 0});
  std::vector<std::uint8_t> entryFor(colours.size() + 1, 0); // by recipe colour + 1
  for (const ColourGroup &group : groups) {
    const auto entry = static_cast<std::uint8_t>(image.palette.size());
    image.palette.push_back(group.colour());
    for (std::size_t c : group.recipeColours) {
      entryFor[c + 1] = entry;
      image.reduction.worstShift =
          std::max(image.reduction.worstShift, colourShift(colours[c], group.colour()));
    }
  }
  image.reduction.coloursKept = static_cast<int>(groups.size());
  for (std::size_t p = 0; p < recipeColourOf.size(); ++p)
    image.pixels[p] = entryFor[recipeColourOf[p]];

  if (image.reduction.reduced())
    reportReduction(image, name);
}

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
      if (index >= kMaxColours)
        return fail("more than " + std::to_string(kMaxColours) + " colours");
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
  if (format != Format::Rgba8)
    return palette[pixels[i]];
  return {pixels[4 * i], pixels[4 * i + 1], pixels[4 * i + 2], pixels[4 * i + 3]};
}

Image draw(const Recipe &recipe, int width, int height, Format format, std::string_view name) {
  Image image;
  image.width = width;
  image.height = height;
  image.format = format;
  const bool rgba = format == Format::Rgba8;
  const int samples = rgba ? 4 : 1; // a side
  const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  image.pixels.assign(pixelCount * (rgba ? 4 : 1), 0);
  // Palette formats: each pixel's recipe colour + 1 (0 transparent), until
  // the palette is made at the end
  std::vector<std::uint16_t> recipeColourOf(rgba ? 0 : pixelCount, 0);
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
      if (!rgba) {
        recipeColourOf[p] = static_cast<std::uint16_t>(fill.colour + 1);
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
  if (!rgba)
    makePalette(image, recipe.colours, recipeColourOf, name);
  return image;
}

} // namespace ScratchPadd::TextureRecipe
