#include "DisplayList.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <type_traits>

namespace ScratchPadd::Scene::DisplayList {

namespace {

// The largest 4.12 vertex coordinate
constexpr std::int32_t kMostVertex = 0x7FFF;

// Collects commands and their parameters, and packs them four commands a
// word, each word followed by its commands' parameters in order
class Packer {
public:
  void add(Command command, std::initializer_list<std::uint32_t> parameters) {
    if (commands_.size() == 4)
      flush();
    commands_.push_back(command);
    parameters_.insert(parameters_.end(), parameters);
  }
  std::vector<std::uint32_t> finish() {
    flush();
    words_[0] = static_cast<std::uint32_t>(words_.size() - 1);
    return std::move(words_);
  }

private:
  void flush() {
    if (commands_.empty())
      return;
    std::uint32_t word = 0;
    for (std::size_t i = 0; i < commands_.size(); ++i)
      word |= static_cast<std::uint32_t>(commands_[i]) << (8 * i);
    words_.push_back(word);
    words_.insert(words_.end(), parameters_.begin(), parameters_.end());
    commands_.clear();
    parameters_.clear();
  }

  std::vector<std::uint32_t> words_{0}; // the count first
  std::vector<std::uint8_t> commands_;
  std::vector<std::uint32_t> parameters_;
};

} // namespace

namespace {
// A template, so only the branch for this platform's Scalar is compiled
template <typename Number> std::int32_t fixedOf(Number value, int fractionBits) {
  if constexpr (std::is_floating_point_v<Number>) {
    return static_cast<std::int32_t>(std::lround(std::ldexp(value, fractionBits)));
  } else {
    // From Q16.16: a shift, rounded (or more fraction bits, exactly)
    const int shift = Number::kFractionalBits - fractionBits;
    if (shift <= 0)
      return value.valueFixed() << -shift;
    return (value.valueFixed() + (1 << (shift - 1))) >> shift;
  }
}
} // namespace

std::int32_t toFixed(Scalar value, int fractionBits) { return fixedOf(value, fractionBits); }

std::uint32_t packNormal(const Vector3 &unit) {
  auto component = [](Scalar v) {
    return static_cast<std::uint32_t>(std::clamp<std::int32_t>(toFixed(v, 9), -512, 511)) & 0x3FF;
  };
  return component(unit.x) | (component(unit.y) << 10) | (component(unit.z) << 20);
}

namespace {
// A colour's place in `palette` (as the DS has it, 15-bit, alpha aside),
// added if it isn't there and there's room, or else the nearest
std::uint8_t texelFor(std::vector<Colour> &palette, Colour colour) {
  const auto five = [](std::uint8_t c) { return c >> 3; };
  int best = 0, bestDistance = 1 << 30;
  for (std::size_t i = 0; i < palette.size(); ++i) {
    const int dr = five(palette[i].r) - five(colour.r), dg = five(palette[i].g) - five(colour.g),
              db = five(palette[i].b) - five(colour.b);
    const int distance = dr * dr + dg * dg + db * db;
    if (distance < bestDistance) {
      best = static_cast<int>(i);
      bestDistance = distance;
    }
  }
  if (bestDistance == 0 || palette.size() == static_cast<std::size_t>(kTexels))
    return static_cast<std::uint8_t>(best);
  palette.push_back({colour.r, colour.g, colour.b, 255});
  return static_cast<std::uint8_t>(palette.size() - 1);
}
} // namespace

std::vector<std::uint16_t> textureFor(const Source &source, Colour material) {
  std::vector<std::uint16_t> texels(kTexels, 0x8000);
  for (std::size_t i = 0; i < source.palette.size(); ++i)
    texels[i] = static_cast<std::uint16_t>(rgb15(multiply(source.palette[i], material)) | 0x8000);
  return texels;
}

Source prepare(const MeshData &mesh) {
  Source source;
  // The furthest a vertex is from the origin along any axis, in 4.12
  std::int32_t furthest = 0;
  for (const Vertex &v : mesh.vertices)
    for (Scalar c : {v.position.x, v.position.y, v.position.z})
      furthest = std::max<std::int32_t>(furthest, std::abs(toFixed(c, 12)));
  while ((furthest >> source.scaleShift) > kMostVertex)
    ++source.scaleShift;

  auto coordinate = [&](Scalar c) {
    return static_cast<std::int16_t>(
        std::clamp<std::int32_t>(toFixed(c, 12 - source.scaleShift), -kMostVertex, kMostVertex));
  };
  // The mesh's runs, or if it has none (one made in code), the DS's best
  Primitives built;
  std::span<const std::uint32_t> indices = mesh.indices;
  std::span<const Primitive> runs = mesh.primitives;
  if (runs.empty()) {
    built = buildPrimitives(mesh);
    indices = built.indices;
    runs = built.runs;
  }
  source.corners.reserve(indices.size());
  for (std::uint32_t index : indices) {
    const Vertex &v = mesh.vertices[index];
    source.corners.push_back({coordinate(v.position.x), coordinate(v.position.y),
                              coordinate(v.position.z), packNormal(v.normal), v.colour,
                              texelFor(source.palette, v.colour), toFixed(v.uv.u, 12),
                              toFixed(v.uv.v, 12)});
  }
  std::uint32_t first = 0;
  for (const Primitive &run : runs) {
    source.runs.push_back({run.kind, run.alpha, first, run.indexCount});
    first += run.indexCount;
  }
  return source;
}

namespace {
// One alpha's runs as a display list
std::vector<std::uint32_t> packAlpha(const Source &source, const Variant &variant,
                                     std::uint8_t alpha) {
  Packer packer;
  bool first = true;
  std::uint16_t colour = 0;
  std::uint32_t normal = 0;
  int texel = -1;
  std::uint32_t lastCoordinate = 0xFFFFFFFF;
  const bool image = variant.imageWidth > 0 && variant.imageHeight > 0;
  for (const Source::Run &run : source.runs) {
    if (run.alpha != alpha)
      continue;
    packer.add(KBegin, {static_cast<std::uint32_t>(run.kind)});
    for (std::uint32_t c = run.first; c < run.first + run.count; ++c) {
      const Source::Corner &corner = source.corners[c];
      // (textured: the colour's in the texture, the vertex lit as white)
      const Colour tinted = variant.textured || image ? Colour{255, 255, 255, corner.colour.a}
                                                      : multiply(corner.colour, variant.colour);
      const std::uint16_t thisColour = rgb15(tinted);
      const bool newColour = first || thisColour != colour;
      if (variant.lit) {
        if (newColour) {
          const std::uint16_t emission = rgb15(multiply(tinted, variant.ambient));
          packer.add(KDiffuseAmbient, {thisColour}); // no ambient reflection
          packer.add(KSpecularEmission, {static_cast<std::uint32_t>(emission) << 16});
        }
        // A normal is lit when it's sent, with the material as it is then
        if (newColour || corner.normal != normal)
          packer.add(KNormal, {corner.normal});
      } else if (newColour) {
        packer.add(KColour, {thisColour});
      }
      if (image) {
        // Its place on the image, in texels as 12.4 (the image's top left 0)
        const auto texels = [](std::int32_t fraction, int side) {
          return static_cast<std::uint32_t>(static_cast<std::uint16_t>(
              static_cast<std::int16_t>((static_cast<std::int64_t>(fraction) * side * 16) >> 12)));
        };
        const std::uint32_t coordinate =
            texels(corner.u, variant.imageWidth) | (texels(corner.v, variant.imageHeight) << 16);
        if (coordinate != lastCoordinate)
          packer.add(KTexCoord, {coordinate});
        lastCoordinate = coordinate;
      } else if (variant.textured && corner.texel != texel) {
        // The middle of its texel, in 12.4
        const int s = (corner.texel % kTextureSide) * 16 + 8,
                  t = (corner.texel / kTextureSide) * 16 + 8;
        packer.add(KTexCoord,
                   {static_cast<std::uint32_t>(s) | (static_cast<std::uint32_t>(t) << 16)});
        texel = corner.texel;
      }
      packer.add(KVertex16,
                 {static_cast<std::uint16_t>(corner.x) |
                      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(corner.y)) << 16),
                  static_cast<std::uint16_t>(corner.z)});
      colour = thisColour;
      normal = corner.normal;
      first = false;
    }
  }
  return packer.finish();
}
} // namespace

std::vector<Packed> pack(const Source &source, const Variant &variant) {
  std::vector<std::uint8_t> alphas;
  for (const Source::Run &run : source.runs)
    if (std::ranges::find(alphas, run.alpha) == alphas.end())
      alphas.push_back(run.alpha);
  std::ranges::sort(alphas, std::greater<>());
  std::vector<Packed> lists;
  lists.reserve(alphas.size());
  for (std::uint8_t alpha : alphas)
    lists.push_back({alpha, packAlpha(source, variant, alpha)});
  return lists;
}

} // namespace ScratchPadd::Scene::DisplayList
