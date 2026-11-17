#include <CosmicHex/Handheld/DicePieces.hpp>

#include <cmath>
#include <utility>
#include <vector>

namespace CosmicHex::Handheld {

namespace {
constexpr Scene::Colour kDie{240, 240, 236};
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

DicePieces::DicePieces() {
  die.box({0, 0, 0}, {0.5f, 0.5f, 0.5f}, kDie);
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
    const Scene::Vector3 middle = face.normal * 0.502f; // (a hair above the face)
    for (const auto &[across, down] : pipsOf(face.number)) {
      const Scene::Vector3 c = middle + face.u * (kPipSpread * static_cast<float>(across)) +
                               face.v * (kPipSpread * static_cast<float>(down));
      const Scene::Vector3 u = face.u * kPipHalf, v = face.v * kPipHalf;
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
  const float scale = std::fmin(wide / width, tall / depth);
  return Scene::Transform::fromAxes(right * scale, -forward * scale, -up * scale,
                                    camera.eye + forward * distance);
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
