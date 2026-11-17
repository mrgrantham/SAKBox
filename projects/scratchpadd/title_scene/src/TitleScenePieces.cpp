// The title scene's pieces, made in code: the title's letters and the sky.
// (Apart from TitleScene.cpp, which draws with a platform's renderer, so a
// test of these needs no renderer.)
#include <ScratchPadd/TitleScene/TitleScene.hpp>

#include <stb/stb_easy_font.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ScratchPadd::TitleScene {

namespace {
constexpr float kPi = 3.14159265f;
} // namespace

Letters makeLetters(const std::string &text, float height, float depth, Scene::Colour colour) {
  // stb_easy_font's strokes: rectangles of 4 vertices (x, y, z, colour), at
  // most 11 a letter (OverlayText's count); a capital is 7 high
  struct Vertex {
    float x, y, z;
    unsigned char colour[4];
  };
  std::string copy = text;
  std::vector<Vertex> vertices((copy.size() + 1) * 11 * 4);
  const int rects = stb_easy_font_print(0, 0, copy.data(), nullptr, vertices.data(),
                                        static_cast<int>(vertices.size() * sizeof(Vertex)));
  Letters out;
  float right = 0;
  for (int r = 0; r < rects; ++r)
    right = std::max(right, vertices[static_cast<std::size_t>(r) * 4 + 2].x);
  const float scale = height / 7;
  out.width = right * scale;
  const float halfDepth = depth / 2;
  for (int r = 0; r < rects; ++r) {
    const Vertex *v = &vertices[static_cast<std::size_t>(r) * 4];
    // (stb's y runs down; ours up, the line's middle at 0)
    const float x0 = v[0].x * scale - out.width / 2, x1 = v[2].x * scale - out.width / 2;
    const float y0 = (3.5f - v[2].y) * scale, y1 = (3.5f - v[0].y) * scale;
    out.mesh.box({(x0 + x1) / 2, (y0 + y1) / 2, 0}, {(x1 - x0) / 2, (y1 - y0) / 2, halfDepth},
                 colour);
    const float z = halfDepth + 0.01f; // (just in front of the face)
    const Scene::Vector3 a{x0, y0, z}, b{x1, y0, z}, c{x1, y1, z}, d{x0, y1, z};
    out.outlines.insert(out.outlines.end(), {a, b, b, c, c, d, d, a});
  }
  return out;
}

Scene::MeshBuilder makeSky(int stars, float radius, unsigned seed) {
  Scene::MeshBuilder sky;
  std::uint32_t state = seed * 2654435761u + 1;
  const auto next = [&] { // (a small LCG: the sky's look, not a game's numbers)
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
  };
  for (int i = 0; i < stars; ++i) {
    // A point on the sphere, evenly spread (z uniform, angle uniform)
    const float z = 2 * next() - 1, angle = 2 * kPi * next();
    const float ring = std::sqrt(1 - z * z);
    const Scene::Vector3 at{ring * std::cos(angle) * radius, z * radius,
                            ring * std::sin(angle) * radius};
    // A square facing the middle: two axes across the direction to it
    const Scene::Vector3 in = (-at).normalised();
    const Scene::Vector3 side =
        (std::abs(in.y) < 0.9f ? in.cross({0, 1, 0}) : in.cross({1, 0, 0})).normalised();
    const Scene::Vector3 other = in.cross(side);
    const float size = 0.06f + 0.1f * next();
    const auto bright = static_cast<std::uint8_t>(150 + 105 * next());
    const Scene::Colour colour{bright, bright,
                               static_cast<std::uint8_t>(std::min(255, bright + 20))};
    sky.quad(at - side * size - other * size, at + side * size - other * size,
             at + side * size + other * size, at - side * size + other * size, colour);
  }
  return sky;
}

} // namespace ScratchPadd::TitleScene
