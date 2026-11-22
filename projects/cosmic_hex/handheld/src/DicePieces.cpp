#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>

#include <ScratchPadd/Motion/Motion.hpp>

#include <CosmicHex/Dice/DiceTray.hpp>

#include <cmath>
#include <utility>
#include <vector>

namespace CosmicHex::Handheld {

namespace {
constexpr Scene::Colour kDie{212, 214, 218}; // (a little greyer)
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
  // A chamfered cube (corners that aren't sharp): each face a
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

namespace {
// A point on the home die's hexagon at `angle` about x, `radius` from it:
// angle 0 straight at +z, turning towards -y
Scene::Vector3 aroundX(float x, float angle, float radius) {
  return {x, -std::sin(angle) * radius, std::cos(angle) * radius};
}
} // namespace

float homeDieTurn(int face) { return -static_cast<float>(face) * 3.14159265f / 3; }

Scene::MeshBuilder homeDie(const std::array<Scene::Colour, 5> &resources, Scene::Colour star) {
  constexpr float kPi = 3.14159265f, kSixth = kPi / 3;
  constexpr float kApothem = 0.5f;                      // (1 across its sides)
  const float corner = kApothem / std::cos(kSixth / 2); // (its corners' radius)
  // (a warm grey, not white, the user's: in the light, white washed out the
  // yellow star and the paler sides)
  constexpr Scene::Colour kBody{196, 188, 172};
  const float a = kHomeDieLength / 2;
  Scene::MeshBuilder m;
  for (int side = 0; side < 6; ++side) {
    const float mid = static_cast<float>(side) * kSixth;
    const float l = mid - kSixth / 2, r = mid + kSixth / 2;
    // The side: a long white quad, facing out (its normal at `mid`)
    m.quad(aroundX(-a, r, corner), aroundX(a, r, corner), aroundX(a, l, corner),
           aroundX(-a, l, corner), kBody);
    // The far end's cap: a triangle of its hexagon (the near end, +x, is
    // the home's tile: homeDieFace)
    m.triangle({-a, 0, 0}, aroundX(-a, r, corner), aroundX(-a, l, corner), kBody);
    // On the side, just proud of it: a coloured hexagon (a resource) or the
    // star, in the side's own plane
    // (a twentieth of its width proud: closer, the DS's depth buffer mixed
    // the two at a slant)
    const Scene::Vector3 out = aroundX(0, mid, kApothem + 0.05f);
    const Scene::Vector3 along{1, 0, 0};
    const Scene::Vector3 across = aroundX(0, mid + kPi / 2, 1); // (in the side's plane)
    const auto on = [&](float u, float v) { return out + along * u + across * v; };
    const float size = 0.26f;
    if (side < 5) {
      for (int t = 0; t < 6; ++t) {
        const float t0 = static_cast<float>(t) * kSixth, t1 = t0 + kSixth;
        m.triangle(out, on(std::cos(t1) * size, std::sin(t1) * size),
                   on(std::cos(t0) * size, std::sin(t0) * size),
                   resources[static_cast<std::size_t>(side)]);
      }
    } else {
      // A five-pointed star, its points out and in by turns
      for (int t = 0; t < 10; ++t) {
        const float t0 = static_cast<float>(t) * kPi / 5, t1 = t0 + kPi / 5;
        const float r0 = t % 2 == 0 ? size * 1.15f : size * 0.48f;
        const float r1 = t % 2 == 0 ? size * 0.48f : size * 1.15f;
        m.triangle(out, on(-std::sin(t1) * r1, std::cos(t1) * r1),
                   on(-std::sin(t0) * r0, std::cos(t0) * r0), star);
      }
    }
  }
  return m;
}

// ------------------------------------------------------- The home die's poses

namespace {
constexpr float kPi = 3.14159265f;
// Half its length, in the board's units
constexpr float kHomeDieHalf = kHomeDieLength / 2 * kHomeDieScale;
// How far its bottom rises clear of the board before it turns: room for
// its corners as it tips
constexpr float kClearOfTheBoard = 0.3f;
// How far it's turned from straight across the screen, its face towards
// you (radians): enough for the face to show beside the side facing you
constexpr float kFaceShows = 0.5f;
} // namespace

Scene::Transform homeDieInBoard(const Scene::Vector3 &tile) {
  // (its x, the long axis, up; its z, side 0's way, along the board's x:
  // its hexagon's sides then face as the tile's do, pointy-topped)
  return Scene::Transform::fromAxes({0, 1, 0}, {0, 0, 1}, {1, 0, 0},
                                    tile - Scene::Vector3{0, kHomeDieHalf, 0});
}

Scene::Transform homeDieFacing(const Scene::Camera &camera, float width, float height, float x,
                               float y, float across) {
  // (as far from the camera as puts its sides `across` pixels apart)
  const float distance =
      kHomeDieScale * height /
      (across * 2 * std::tan(static_cast<float>(camera.fieldOfView) * kPi / 360));
  const Scene::Vector3 at = solidOnScreen(camera, width, height, x, y, 1, distance).position();
  const auto [forward, right, up] = viewAxes(camera);
  const Scene::Vector3 back = -forward;
  const float c = std::cos(kFaceShows), s = std::sin(kFaceShows);
  const Scene::Vector3 along = right * c + back * s, side = back * c - right * s;
  return Scene::Transform::fromAxes(along, side.cross(along), side, at);
}

Scene::Transform homeDiePose(const Scene::Transform &inBoard, const Scene::Transform &facing,
                             const Scene::Vector3 &bow, float lifted, float sink, float turn) {
  const Scene::Vector3 inPlace = inBoard.position();
  const Scene::Vector3 risen = inPlace + Scene::Vector3{0, 2 * kHomeDieHalf + kClearOfTheBoard, 0};
  const Scene::Vector3 lowered{0, -sink, 0};
  const Scene::Transform spin = Scene::Transform::rotation({1, 0, 0}, turn);
  if (lifted <= kHomeDieRisen) {
    const float up = std::max(0.0f, lifted) / kHomeDieRisen;
    return rigidBetween(inBoard, facing, 0, inPlace + (risen - inPlace) * up + lowered) * spin;
  }
  const float across = (lifted - kHomeDieRisen) / (1 - kHomeDieRisen);
  const Scene::Vector3 place =
      ScratchPadd::Motion::arc(risen, facing.position(), bow, across) + lowered;
  return rigidBetween(inBoard, facing, across, place) * spin;
}

Scene::Transform homeDieBody(const Scene::Transform &pose) {
  return pose * Scene::Transform::scaling({kHomeDieScale, kHomeDieScale, kHomeDieScale});
}

Scene::Transform homeDieFace(const Scene::Transform &pose) {
  // (the tile's own axes as the die's, standing in the board: its up the
  // die's x, its x the die's z, its z the die's y; homeDieInBoard)
  return pose * Scene::Transform::fromAxes({0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {kHomeDieHalf, 0, 0});
}

Scene::Transform homeDieHole(const Scene::Vector3 &tile, float below) {
  return Scene::Transform::translation(tile - Scene::Vector3{0, below, 0}) *
         Scene::Transform::scaling({kHomeDieHoleWide, 1, kHomeDieHoleWide});
}

} // namespace CosmicHex::Handheld
