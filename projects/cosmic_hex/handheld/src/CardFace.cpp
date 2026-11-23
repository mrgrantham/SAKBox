#include <CosmicHex/Handheld/CardFace.hpp>

#include <CosmicHex/Handheld/Palette.hpp>

#include <algorithm>
#include <cstddef>

namespace CosmicHex::Handheld::CardFace {

namespace Scene = ScratchPadd::Scene;

namespace {

Colour colourOf(Art::Rgb rgb) { return {rgb.r, rgb.g, rgb.b, 255}; }

constexpr float kPi = 3.14159265f;

// A shape's corners, in the card's units
float cornerOf(Shape shape) { return shape == Shape::Card ? Art::kCardCorner : Art::kBandCorner; }

// A rounded rectangle, in pixels (or texels): its top left, its size and
// its corners' radius
struct Rounded {
  float x, y, width, height, radius;
};

// How much of the pixel with its top left at (px, py) `shape` covers, 0
// to 1: whole inside its straight edges, else from 4 x 4 samples
float coverage(float px, float py, const Rounded &shape) {
  const float left = shape.x, top = shape.y, right = shape.x + shape.width,
              bottom = shape.y + shape.height, r = shape.radius;
  if (px + 1 <= left || px >= right || py + 1 <= top || py >= bottom)
    return 0;
  const bool insideX = px >= left && px + 1 <= right, insideY = py >= top && py + 1 <= bottom;
  if (insideX && insideY &&
      ((px >= left + r && px + 1 <= right - r) || (py >= top + r && py + 1 <= bottom - r)))
    return 1;
  constexpr int kSamples = 4;
  int inside = 0;
  for (int sy = 0; sy < kSamples; ++sy)
    for (int sx = 0; sx < kSamples; ++sx) {
      const float x = px + (static_cast<float>(sx) + 0.5f) / kSamples;
      const float y = py + (static_cast<float>(sy) + 0.5f) / kSamples;
      if (x < left || x >= right || y < top || y >= bottom)
        continue;
      // (past the nearest corner's centre, in a corner: within its radius)
      const float dx = x - std::clamp(x, left + r, right - r);
      const float dy = y - std::clamp(y, top + r, bottom - r);
      inside += dx * dx + dy * dy <= r * r ? 1 : 0;
    }
  return static_cast<float>(inside) / (kSamples * kSamples);
}

// `top` over `under`, `top`'s alpha times `amount` (straight alpha)
Colour over(Colour top, float amount, Colour under) {
  const float a = amount * static_cast<float>(top.a) / 255;
  const float b = static_cast<float>(under.a) / 255 * (1 - a);
  const float out = a + b;
  if (out <= 0)
    return {0, 0, 0, 0};
  const auto mix = [&](std::uint8_t t, std::uint8_t u) {
    return static_cast<std::uint8_t>(
        std::lround((static_cast<float>(t) * a + static_cast<float>(u) * b) / out));
  };
  return {mix(top.r, under.r), mix(top.g, under.g), mix(top.b, under.b),
          static_cast<std::uint8_t>(std::lround(out * 255))};
}

// `shape` drawn in `colour` over `pixels` (`width` x `height`)
void fill(std::vector<Colour> &pixels, int width, int height, const Rounded &shape, Colour colour) {
  const int x0 = std::max(0, static_cast<int>(std::floor(shape.x)));
  const int y0 = std::max(0, static_cast<int>(std::floor(shape.y)));
  const int x1 = std::min(width, static_cast<int>(std::ceil(shape.x + shape.width)));
  const int y1 = std::min(height, static_cast<int>(std::ceil(shape.y + shape.height)));
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      const float cover = coverage(static_cast<float>(x), static_cast<float>(y), shape);
      if (cover <= 0)
        continue;
      Colour &pixel = pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                             static_cast<std::size_t>(x)];
      pixel = over(colour, cover, pixel);
    }
}

} // namespace

Colour bandOf(Art::Card card) { return colourOf(Art::bandOf(card)); }

Colour titleInkOf(Art::Card card) { return Palette::inkOnBand(CardFace::bandOf(card)); }

// (the resources come first, Crystals the last of them)
bool hasWords(Art::Card card) { return card > Art::Card::Crystals; }

Art::CardArea artOf(Art::Card card) {
  if (hasWords(card))
    return Art::kCardArt;
  // (down to the card's foot, in from it as far as the title band is from
  // its top)
  const float inset = Art::kCardTitle.y;
  return {Art::kCardArt.x, Art::kCardArt.y, Art::kCardArt.width,
          Art::kCardHeight - inset - Art::kCardArt.y};
}

Art::CardArea iconOf(Art::Card card) {
  Art::CardArea icon = Art::kCardIcon;
  if (!hasWords(card))
    icon.y = (Art::kCardHeight - icon.height) / 2;
  return icon;
}

std::array<Layer, kLayers> layersOf(Art::Card card) {
  // (the body inside the edge by a line's width, as the drawings' stroke)
  constexpr float kEdge = 0.75f;
  return {{{{0, 0, Art::kCardWidth, Art::kCardHeight}, Shape::Card, colourOf(Art::kCardEdge)},
           {{kEdge, kEdge, Art::kCardWidth - 2 * kEdge, Art::kCardHeight - 2 * kEdge},
            Shape::Card,
            colourOf(Art::kCardBody)},
           {Art::kCardTitle, Shape::TitleBand, CardFace::bandOf(card)},
           {artOf(card), hasWords(card) ? Shape::ArtBand : Shape::TallArtBand,
            colourOf(Art::kCardArtBand)}}};
}

ShapeArea areaOf(Shape shape) {
  // Each in its own proportions: the card down the left, 30 texels wide;
  // the bands to its right, one under the other, 30 wide (a resource's
  // tall art band 26, to fit under the others)
  constexpr int kWide = 30, kBandsX = kWide + 3;
  const auto band = [](int y, int wide, const Art::CardArea &area) {
    const float texels = static_cast<float>(wide) / area.width;
    return ShapeArea{kBandsX, y, wide, static_cast<int>(std::lround(area.height * texels)),
                     Art::kBandCorner * texels};
  };
  switch (shape) {
  case Shape::Card:
    return {1, 1, kWide, heightOf(kWide), Art::kCardCorner * kWide / Art::kCardWidth};
  case Shape::TitleBand:
    return band(1, kWide, Art::kCardTitle);
  case Shape::ArtBand:
    return band(11, kWide, Art::kCardArt);
  case Shape::TallArtBand:
    return band(29, 26, artOf(Art::Card::Minerals));
  }
  return {};
}

std::vector<Colour> shapesPixels() {
  std::vector<Colour> pixels(static_cast<std::size_t>(kShapesSize) * kShapesSize,
                             Colour{255, 255, 255, 0});
  for (const Shape shape : kShapes) {
    const ShapeArea area = areaOf(shape);
    fill(pixels, kShapesSize, kShapesSize,
         {static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.width),
          static_cast<float>(area.height), area.radius},
         {255, 255, 255, 255});
  }
  return pixels;
}

Scene::MeshBuilder layersMesh(Art::Card card) {
  Scene::MeshBuilder mesh;
  const std::array<Layer, kLayers> layers = layersOf(card);
  for (std::size_t i = 0; i < layers.size(); ++i) {
    const Art::CardArea &a = layers[i].area;
    const ShapeArea t = areaOf(layers[i].shape);
    const float left = a.x / Art::kCardWidth, right = (a.x + a.width) / Art::kCardWidth;
    const float top = 1 - a.y / Art::kCardHeight, bottom = 1 - (a.y + a.height) / Art::kCardHeight;
    const float z = static_cast<float>(i) * kLayerStep;
    constexpr auto kSize = static_cast<float>(kShapesSize);
    const float u0 = static_cast<float>(t.x) / kSize,
                u1 = static_cast<float>(t.x + t.width) / kSize;
    const float v0 = static_cast<float>(t.y) / kSize,
                v1 = static_cast<float>(t.y + t.height) / kSize;
    mesh.quad({left, bottom, z}, {right, bottom, z}, {right, top, z}, {left, top, z},
              layers[i].colour,
              {Scene::TexCoord{u0, v1}, Scene::TexCoord{u1, v1}, Scene::TexCoord{u1, v0},
               Scene::TexCoord{u0, v0}});
  }
  return mesh;
}

PixelArea iconPixelsOn(Art::Card card, int width) {
  const float scale = static_cast<float>(width) / Art::kCardWidth;
  const auto at = [scale](float units) { return static_cast<int>(std::lround(units * scale)); };
  const Art::CardArea icon = iconOf(card);
  return {at(icon.x), at(icon.y), at(icon.width), at(icon.height)};
}

std::vector<Colour> pixelsOf(Art::Card card, int width, std::span<const Colour> icon) {
  const int height = heightOf(width);
  std::vector<Colour> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                             Colour{0, 0, 0, 0});
  const float scale = static_cast<float>(width) / Art::kCardWidth;
  for (const Layer &layer : layersOf(card))
    fill(pixels, width, height,
         {layer.area.x * scale, layer.area.y * scale, layer.area.width * scale,
          layer.area.height * scale, cornerOf(layer.shape) * scale},
         layer.colour);
  const PixelArea at = iconPixelsOn(card, width);
  if (icon.size() < static_cast<std::size_t>(at.width) * static_cast<std::size_t>(at.height))
    return pixels;
  for (int y = 0; y < at.height && at.y + y < height; ++y)
    for (int x = 0; x < at.width && at.x + x < width; ++x) {
      Colour &pixel = pixels[static_cast<std::size_t>(at.y + y) * static_cast<std::size_t>(width) +
                             static_cast<std::size_t>(at.x + x)];
      pixel = over(icon[static_cast<std::size_t>(y) * static_cast<std::size_t>(at.width) +
                        static_cast<std::size_t>(x)],
                   1, pixel);
    }
  return pixels;
}

Turn turnOf(float turned) {
  const float t = std::clamp(turned, 0.0f, 1.0f);
  return {std::abs(std::cos(t * kPi)), t >= 0.5f};
}

} // namespace CosmicHex::Handheld::CardFace
