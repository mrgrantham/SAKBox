#include "DisplayList.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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
  source.corners.reserve(mesh.indices.size());
  for (std::uint32_t index : mesh.indices) {
    const Vertex &v = mesh.vertices[index];
    source.corners.push_back({coordinate(v.position.x), coordinate(v.position.y),
                              coordinate(v.position.z), packNormal(v.normal), v.colour});
  }
  return source;
}

std::vector<std::uint32_t> pack(const Source &source, const Variant &variant) {
  Packer packer;
  packer.add(kBegin, {0}); // separate triangles
  bool first = true;
  std::uint16_t colour = 0;
  std::uint32_t normal = 0;
  for (const Source::Corner &corner : source.corners) {
    const Colour tinted = multiply(corner.colour, variant.colour);
    const std::uint16_t thisColour = rgb15(tinted);
    const bool newColour = first || thisColour != colour;
    if (variant.lit) {
      if (newColour) {
        const std::uint16_t emission = rgb15(multiply(tinted, variant.ambient));
        packer.add(kDiffuseAmbient, {thisColour}); // no ambient reflection
        packer.add(kSpecularEmission, {static_cast<std::uint32_t>(emission) << 16});
      }
      // A normal is lit when it's sent, with the material as it is then
      if (newColour || corner.normal != normal)
        packer.add(kNormal, {corner.normal});
    } else if (newColour) {
      packer.add(kColour, {thisColour});
    }
    packer.add(kVertex16,
               {static_cast<std::uint16_t>(corner.x) |
                    (static_cast<std::uint32_t>(static_cast<std::uint16_t>(corner.y)) << 16),
                static_cast<std::uint16_t>(corner.z)});
    colour = thisColour;
    normal = corner.normal;
    first = false;
  }
  return packer.finish();
}

} // namespace ScratchPadd::Scene::DisplayList
