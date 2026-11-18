#include <CosmicHex/Handheld/DicePieces.hpp>

#include <ScratchPadd/Motion/Motion.hpp>

#include <CosmicHex/Dice/DiceTray.hpp>

#include <cmath>
#include <utility>
#include <vector>

namespace CosmicHex::Handheld {

namespace {
constexpr Scene::Colour kDie{212, 214, 218}; // (round 89: a little greyer)
constexpr float kRound = 0.12f;              // how much of a corner is cut off, rounding it
constexpr Scene::Colour kPip{30, 30, 40};
constexpr float kPipHalf = 0.075f;  // a pip's half-width
constexpr float kPipSpread = 0.24f; // how far from the face's middle the outer pips sit

// Each number's pips, in steps of kPipSpread across and down its face
std::vector<std::pair<int, int>> pipsOf(int number) {
  switch (number) {
  case 1:
    return {{0, 0}};
  case 2:
    return {{-1, -1}, {1, 1}};
  case 3:
    return {{-1, -1}, {0, 0}, {1, 1}};
  case 4:
    return {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  case 5:
    return {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}};
  default:
    return {{-1, -1}, {-1, 0}, {-1, 1}, {1, -1}, {1, 0}, {1, 1}};
  }
}
} // namespace

DicePieces::DicePieces(float size) {
  // A chamfered cube (round 89: corners that aren't sharp): each face a
  // smaller square, a bevel along each edge, a triangle at each corner; 44
  // triangles, `size` across
  const float h = 0.5f * size, in = h - kRound * size;
  const auto corner = [&](int sx, int sy, int sz, int along) {
    // A corner of the face whose normal is along axis `along`: full out
    // along that axis, inset along the others
    return Scene::Vector3{static_cast<float>(sx) * (along == 0 ? h : in),
                          static_cast<float>(sy) * (along == 1 ? h : in),
                          static_cast<float>(sz) * (along == 2 ? h : in)};
  };
  // Each piece wound to face out: the die is convex and centred, so a
  // piece faces out when its normal points away from the middle
  const auto outwardQuad = [&](Scene::Vector3 a, Scene::Vector3 b, Scene::Vector3 c,
                               Scene::Vector3 d) {
    if ((b - a).cross(c - a).dot(a + b + c + d) < 0)
      std::swap(b, d);
    die.quad(a, b, c, d, kDie);
  };
  const auto outwardTriangle = [&](Scene::Vector3 a, Scene::Vector3 b, Scene::Vector3 c) {
    if ((b - a).cross(c - a).dot(a + b + c) < 0)
      std::swap(b, c);
    die.triangle(a, b, c, kDie);
  };
  // The faces: for each axis and side, the inset square
  for (int axis = 0; axis < 3; ++axis)
    for (int side : {-1, 1}) {
      const int u = (axis + 1) % 3, v = (axis + 2) % 3;
      std::array<Scene::Vector3, 4> q{};
      const int su[4] = {-1, 1, 1, -1}, sv[4] = {-1, -1, 1, 1};
      for (int k = 0; k < 4; ++k) {
        int s3[3] = {0, 0, 0};
        s3[axis] = side;
        s3[u] = su[k];
        s3[v] = sv[k];
        q[static_cast<std::size_t>(k)] = corner(s3[0], s3[1], s3[2], axis);
      }
      outwardQuad(q[0], q[1], q[2], q[3]);
    }
  // The bevels: along each edge, between the two faces that meet there
  for (int axis = 0; axis < 3; ++axis) // (the edge runs along `axis`)
    for (int a : {-1, 1})
      for (int b : {-1, 1}) {
        const int u = (axis + 1) % 3, v = (axis + 2) % 3;
        int lo[3], hi[3];
        lo[axis] = -1;
        hi[axis] = 1;
        lo[u] = hi[u] = a;
        lo[v] = hi[v] = b;
        outwardQuad(corner(lo[0], lo[1], lo[2], u), corner(hi[0], hi[1], hi[2], u),
                    corner(hi[0], hi[1], hi[2], v), corner(lo[0], lo[1], lo[2], v));
      }
  // The corners: a triangle where three bevels meet
  for (int sx : {-1, 1})
    for (int sy : {-1, 1})
      for (int sz : {-1, 1})
        outwardTriangle(corner(sx, sy, sz, 0), corner(sx, sy, sz, 1), corner(sx, sy, sz, 2));
  // Each face: its outward normal, two axes across it (u x v = the normal,
  // so its pips wind as the box's faces do), and its number
  struct Face {
    Scene::Vector3 normal, u, v;
    int number;
  };
  const Face faces[] = {
      {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, 1}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, 6},
      {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}, 2}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}, 5},
      {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, 3}, {{0, 0, -1}, {0, 1, 0}, {1, 0, 0}, 4},
  };
  for (const Face &face : faces) {
    const Scene::Vector3 middle = face.normal * (h + 0.002f); // (a hair above the face)
    for (const auto &[across, down] : pipsOf(face.number)) {
      const Scene::Vector3 c = middle + face.u * (kPipSpread * size * static_cast<float>(across)) +
                               face.v * (kPipSpread * size * static_cast<float>(down));
      const Scene::Vector3 u = face.u * (kPipHalf * size), v = face.v * (kPipHalf * size);
      die.quad(c - u - v, c + u - v, c + u + v, c - u + v, kPip);
    }
  }
}

Scene::Transform glassFrame(const Scene::Camera &camera, float aspect, float width, float depth,
                            float distance) {
  const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
  const Scene::Vector3 right = forward.cross(camera.up).normalised();
  const Scene::Vector3 up = right.cross(forward);
  // What the screen shows at that distance, and the scale that fits the
  // container in it
  const float tall = 2 * distance * std::tan(camera.fieldOfView * 3.14159265f / 360);
  const float wide = tall * aspect;
  // (kGlassShare of the screen: a die's top is nearer the camera than the floor, so
  // looks bigger, and one resting at a wall would go past the screen's edge)
  const float scale = kGlassShare * std::fmin(wide / width, tall / depth);
  return Scene::Transform::fromAxes(right * scale, -forward * scale, -up * scale,
                                    camera.eye + forward * distance);
}

std::optional<std::array<float, 2>> onScreen(const Scene::Camera &camera, float width, float height,
                                             const Scene::Vector3 &world) {
  const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
  const Scene::Vector3 right = forward.cross(camera.up).normalised();
  const Scene::Vector3 up = right.cross(forward);
  const Scene::Vector3 d = world - camera.eye;
  const float ahead = d.dot(forward);
  if (ahead <= camera.nearPlane)
    return std::nullopt;
  const float halfTall = std::tan(camera.fieldOfView * 3.14159265f / 360);
  const float across = d.dot(right) / (ahead * halfTall * (width / height));
  const float along = d.dot(up) / (ahead * halfTall);
  return std::array<float, 2>{(across + 1) / 2 * width, (1 - along) / 2 * height};
}

namespace {
// The pull's acceleration in screen widths a second, a second: the glass's
// units, the glass kGlassShare of the screen across
float pullScreens() {
  return static_cast<float>(Dice::DiceTray::kPullAcceleration / Dice::DiceTray::kWidth) *
         kGlassShare;
}
} // namespace

float slideSeconds() { return 2 * std::sqrt(1 / pullScreens()); } // (half the way each)

float slideAcross(float seconds) {
  // (the Motion library's Pull: half the time speeding up, half slowing; at
  // the glass's acceleration it's slideSeconds() long)
  return ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::Pull, seconds / slideSeconds());
}

Scene::Transform dieOnGlass(const Scene::Transform &glass, const Scene::Vector3 &position,
                            const std::array<float, 4> &turn, float offset) {
  const auto [w, x, y, z] = turn;
  // The quaternion's rotation as the axes a die's x, y and z become
  const Scene::Vector3 ax{1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y)};
  const Scene::Vector3 ay{2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)};
  const Scene::Vector3 az{2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)};
  return glass * Scene::Transform::fromAxes(ax, ay, az, position - Scene::Vector3{offset, 0, 0});
}

} // namespace CosmicHex::Handheld
