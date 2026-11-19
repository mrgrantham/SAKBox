// Types.hpp - the scene layer's values: vectors, colours, transforms, and
// what a frame is drawn with (camera, lights, fog, materials). The same on
// every platform; only Scalar differs (see DESIGN.md).
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

#if defined(__NDS__) || defined(SCRATCHPADD_SCENE_FIXED_POINT)
#include <FixedPoint.hpp>
#endif

namespace ScratchPadd::Scene {

// The scene's number type: float on desktops, and on the DS flexmath's
// FlexPoint16 (Q16.16), as the DS has no floating-point hardware: what
// Cyclone runs in there, so its positions go straight in. Desktops can
// build with it too (SCRATCHPADD_SCENE_FIXED_POINT), to test it.
#if defined(__NDS__) || defined(SCRATCHPADD_SCENE_FIXED_POINT)
using Scalar = Flex::Math::FlexPoint16;
inline Scalar squareRoot(Scalar x) { return Flex::Math::flex_sqrt(x); }
inline Scalar sine(Scalar radians) { return Flex::Math::flex_sin(radians); }
inline Scalar cosine(Scalar radians) { return Flex::Math::flex_cos(radians); }
#else
using Scalar = float;
inline Scalar squareRoot(Scalar x) { return std::sqrt(x); }
inline Scalar sine(Scalar radians) { return std::sin(radians); }
inline Scalar cosine(Scalar radians) { return std::cos(radians); }
#endif

struct Vector3 {
  Scalar x = 0, y = 0, z = 0;

  Vector3 operator+(const Vector3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vector3 operator-(const Vector3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vector3 operator-() const { return {-x, -y, -z}; }
  Vector3 operator*(Scalar s) const { return {x * s, y * s, z * s}; }
  bool operator==(const Vector3 &) const = default;

  Scalar dot(const Vector3 &o) const { return x * o.x + y * o.y + z * o.z; }
  Vector3 cross(const Vector3 &o) const {
    return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
  }
  // Its length, and it at length 1 (or zero, if it's zero). In Q16.16
  // (the DS's) the squares can leave its range (a vector 182 long) or lose
  // its precision (one under 1/16), so a vector outside those is first
  // divided by its largest component; within them, as in float, the squares
  // are taken straight away (round 105: the divides, three a component,
  // were most of a DS draw's time)
  Scalar length() const {
    const Scalar most = largestComponent();
    if (!(most > Scalar(0)))
      return Scalar(0);
    if (squaresFit(most))
      return squareRoot(dot(*this));
    const Vector3 scaled{x / most, y / most, z / most};
    return most * squareRoot(scaled.dot(scaled));
  }
  Vector3 normalised() const {
    const Scalar l = length();
    if (!(l > Scalar(0)))
      return {};
    return {x / l, y / l, z / l};
  }
  // Whether a vector whose largest component is `most` squares safely
  static bool squaresFit(Scalar most) {
#if defined(__NDS__) || defined(SCRATCHPADD_SCENE_FIXED_POINT)
    return most > Scalar(0.0625f) && most < Scalar(104);
#else
    (void)most;
    return true;
#endif
  }
  Scalar largestComponent() const {
    auto size = [](Scalar v) { return v < Scalar(0) ? -v : v; };
    const Scalar a = size(x), b = size(y), c = size(z);
    return a > b ? (a > c ? a : c) : (b > c ? b : c);
  }
};

// A colour, 0-255 a channel. Alpha is how opaque it is (for vertex colours
// and the overlay; a material's own opacity is Material::alpha). A mesh with
// a see-through vertex is drawn translucent, whatever its material.
// Aligned as a word, so a whole colour is copied, filled or compared as one
// (round 105: byte-aligned, the DS, which can't load a word from just any
// byte, moved every pixel of its pictures a byte at a time)
struct alignas(4) Colour {
  std::uint8_t r = 255, g = 255, b = 255, a = 255;
  bool operator==(const Colour &) const = default;

  // From channels 0-1, as many shaders and tools give them
  static Colour fromUnit(float r, float g, float b, float a = 1) {
    auto channel = [](float v) {
      return static_cast<std::uint8_t>(std::lround(std::fmin(std::fmax(v, 0.0f), 1.0f) * 255));
    };
    return {channel(r), channel(g), channel(b), channel(a)};
  }
};
static_assert(sizeof(Colour) == 4, "a colour is four bytes: pixels are packed");

// Where a part is: a 3 x 4 affine matrix (rotation and scale in the first
// three columns, position in the fourth) taking the part's own coordinates
// to the world's. Rigid parts and boxes scaled to size are all it needs.
struct Transform {
  std::array<std::array<Scalar, 4>, 3> m{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};

  static Transform identity() { return {}; }
  static Transform translation(const Vector3 &v);
  static Transform scaling(const Vector3 &v);
  // A turn of `radians` about `axis` (right-handed)
  static Transform rotation(const Vector3 &axis, Scalar radians);
  // A transform from the axes the part's x, y and z become, and its position
  static Transform fromAxes(const Vector3 &x, const Vector3 &y, const Vector3 &z,
                            const Vector3 &position);

  // This transform after `o`: (a * b).apply(p) == a.apply(b.apply(p))
  Transform operator*(const Transform &o) const;
  Vector3 apply(const Vector3 &point) const;
  // `direction` turned (and scaled) but not moved
  Vector3 applyToDirection(const Vector3 &direction) const;
  Vector3 position() const { return {m[0][3], m[1][3], m[2][3]}; }
};

// What the frame is seen from. The aspect ratio is the display's.
struct Camera {
  Vector3 eye{0, 0, 5}, target{0, 0, 0}, up{0, 1, 0};
  Scalar fieldOfView = 60; // vertical, degrees
  Scalar nearPlane = 0.1f, farPlane = 100;
};

// A directional light: `direction` is the way its light travels
struct Light {
  Vector3 direction{0, -1, 0};
  Colour colour;
};

// What a frame drew, or a mesh costs, as the DS counts it: polygons, and
// the vertices they take in its vertex RAM (a triangle 3, a quad 4, a strip
// 2 more than its triangles; see Primitives.hpp). A line is a triangle
// there. Desktops count the 3D the same way; the DS reads its hardware's
// counters, which count the overlay's shapes too. The 3DS draws triangles,
// so it counts triangles (3 vertices each). `draws` is the frame's draws
// (each mesh drawn, and each drawLines), which cost the CPU time on the 3DS;
// `culled` the meshes not drawn as they were wholly out of view (Culling.hpp).
struct FrameCounts {
  unsigned polygons = 0, vertices = 0, draws = 0, culled = 0;
  FrameCounts &operator+=(const FrameCounts &o) {
    polygons += o.polygons;
    vertices += o.vertices;
    draws += o.draws;
    culled += o.culled;
    return *this;
  }
  bool operator==(const FrameCounts &) const = default;
};

// Fog fades what's far toward `colour`: fogged = 1 - exp(-density x distance)
struct Fog {
  Colour colour;
  Scalar density = 0; // 0: none
};

// Outlines: a dark line around a shape (Material::outline), drawn one of
// two ways on desktops (the 3DS always draws a hull; the DS its hardware's
// edge marking, 1 pixel, whatever the style):
//   Hull         the mesh again, `width` larger along its normals (world
//                units), its front faces culled, so only its rim shows
//   ScreenSpace  lines found on the screen once the frame's drawn, `pixels`
//                thick, wherever the depth steps or the surface turns
//                sharply: around the shape and along its creases too
//                (desktops; elsewhere a hull). The frame is drawn into an
//                offscreen target first, without the window's anti-aliasing.
struct Outline {
  enum class Style : std::uint8_t { Hull, ScreenSpace };
  Colour colour{12, 12, 20};
  Scalar width = 0.03f;
  Style style = Style::Hull;
  std::uint8_t pixels = 2;
};

// How the DS draws toon bands (its hardware has one table of them for the
// whole frame, picked by how much light a polygon gets; other platforms
// ignore it). A toon mesh's colours come from a small texture of them
// (ds/src/DisplayList.hpp), which the table's band then shades:
//   Shades      the colour times the band's grey, dark to full: as desktops
//   Highlights  the colour lit smoothly, the band's light added on top
enum class DsToonTable : std::uint8_t { Shades, Highlights };

// What a frame is drawn in: the background, the lights (as the DS has, at
// most four), the ambient light lit materials get anyway, and fog
struct Environment {
  static constexpr unsigned kMostLights = 4;

  Colour background{0, 0, 0};
  Colour ambient{80, 80, 80};
  std::array<Light, kMostLights> lights{};
  unsigned lightCount = 0;
  Fog fog;
  // Toon shading's bands of light (Material::Shading::Toon), dark to fully
  // lit, unless a material has its own count (2 to 8)
  std::uint8_t toonBands = 3;
  Outline outline;
  DsToonTable dsToonTable = DsToonTable::Shades;

  Environment &addLight(const Light &light) {
    if (lightCount < kMostLights)
      lights[lightCount++] = light;
    return *this;
  }

  // The background as seen: fogged as a mesh at the camera's far plane
  // would be (as the DS's hardware fogs it). With the fog's colour the
  // background's, as fog usually is, just the background.
  Colour backgroundSeen(Scalar farPlane) const;
};

// A loaded texture, used by handle (Renderer::load(TextureImage)). 0 is
// no texture.
struct Texture {
  std::uint32_t id = 0;
  explicit operator bool() const { return id != 0; }
  bool operator==(const Texture &) const = default;
};

// An image to make a texture from: `width` x `height` colours, rows from
// the top (a texture recipe's RGBA pixels, say). Sides that are powers of
// two (8 to 1024) work everywhere; the 3DS and the DS need them.
struct TextureImage {
  int width = 0, height = 0;
  std::span<const Colour> pixels;
};

// Where a vertex sits on its material's texture: (0, 0) the image's top
// left, (1, 1) its bottom right
struct TexCoord {
  Scalar u = 0, v = 0;
  bool operator==(const TexCoord &) const = default;
};

// How a mesh is drawn: the things the DS does in hardware
struct Material {
  // Unlit: its colours as they are. Lit: by the ambient light and the
  // lights, smoothly. Toon: lit in a few hard bands (Environment::toonBands,
  // or the material's own toonBands)
  enum class Shading { Unlit, Lit, Toon };
  Shading shading = Shading::Lit;
  // Multiplies the mesh's vertex colours
  Colour colour;
  // An image on it, by its vertices' texture coordinates, multiplying its
  // colours too (and its alpha, the texture's); none: plain colours
  Texture texture;
  // How opaque, 0-31 as the DS has it: 31 is solid; below that it's
  // drawn after everything solid, farthest first
  std::uint8_t alpha = 31;
  // Only the edges of its triangles
  bool wireframe = false;
  // Both sides of its triangles drawn, though its mesh culls its back faces
  // (MeshData::doubleSided)
  bool doubleSided = false;
  // Toon shading's bands for this mesh (2 to 8), or 0 for the environment's
  std::uint8_t toonBands = 0;
  // A line around its shape (see Outline), in the environment's colour
  // unless it has its own
  bool outline = false;
  std::optional<Colour> outlineColour;
};

// The bands toon shading uses for `material` in `environment` (2 to 8)
inline int toonBandsFor(const Material &material, const Environment &environment) {
  const int bands = material.toonBands != 0 ? material.toonBands : environment.toonBands;
  return bands < 2 ? 2 : (bands > 8 ? 8 : bands);
}

// How much of a light toon shading lets through, given how much smooth
// lighting would (0-1: the light's cosine on the surface): the same number
// of steps as bands, the darkest none, the brightest all of it.
//   3 bands: under 1/3 none, then a half, from 2/3 all
inline float toonStep(float lit, int bands) {
  const float step = std::floor(lit * static_cast<float>(bands)) / static_cast<float>(bands - 1);
  return step < 0.0f ? 0.0f : (step > 1.0f ? 1.0f : step);
}

} // namespace ScratchPadd::Scene
