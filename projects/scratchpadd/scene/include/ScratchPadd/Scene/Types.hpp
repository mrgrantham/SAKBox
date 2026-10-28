// Types.hpp - the scene layer's values: vectors, colours, transforms, and
// what a frame is drawn with (camera, lights, fog, materials). The same on
// every platform; only Scalar differs (see DESIGN.md).
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace ScratchPadd::Scene {

// The scene's number type: float on desktops. (On the DS it will be
// flexmath's FlexPoint16, Q16.16, as the DS has no floating-point hardware;
// see DESIGN.md.)
using Scalar = float;

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
  Scalar length() const { return std::sqrt(dot(*this)); }
  // This vector at length 1 (or zero, if it's zero)
  Vector3 normalised() const {
    const Scalar l = length();
    return l > 0 ? *this * (1 / l) : Vector3{};
  }
};

// A colour, 0-255 a channel. Alpha is how opaque it is (for vertex colours
// and the overlay; a material's own opacity is Material::alpha).
struct Colour {
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

// Fog fades what's far toward `colour`: fogged = 1 - exp(-density x distance)
struct Fog {
  Colour colour;
  Scalar density = 0; // 0: none
};

// What a frame is drawn in: the background, the lights (as the DS has, at
// most four), the ambient light lit materials get anyway, and fog
struct Environment {
  static constexpr unsigned kMostLights = 4;

  Colour background{0, 0, 0};
  Colour ambient{80, 80, 80};
  std::array<Light, kMostLights> lights{};
  unsigned lightCount = 0;
  Fog fog;

  Environment &addLight(const Light &light) {
    if (lightCount < kMostLights)
      lights[lightCount++] = light;
    return *this;
  }
};

// How a mesh is drawn: the things the DS does in hardware
struct Material {
  enum class Shading { unlit, lit };
  Shading shading = Shading::lit;
  // Multiplies the mesh's vertex colours
  Colour colour;
  // How opaque, 0-31 as the DS has it: 31 is solid; below that it's
  // drawn after everything solid, farthest first
  std::uint8_t alpha = 31;
  // Only the edges of its triangles
  bool wireframe = false;
};

} // namespace ScratchPadd::Scene
