#include <AnimationLab/ViewCube.hpp>

#include <cmath>

namespace AnimationLab {

namespace Scene = ScratchPadd::Scene;

namespace {

// A point in the cube's own units (its faces at -1 and 1) on the screen
ScreenPoint onScreen(const OrbitView &view, ScreenPoint centre, float half,
                     const Scene::Vector3 &p) {
  const Scene::Vector3 seen = view.inView(p);
  return {centre.x + seen.x * half, centre.y - seen.y * half};
}

// Face `face` (0 +x, 1 -x, 2 +y, 3 -y, 4 +z, 5 -z): the axis it's across,
// and its sign
int axisOf(int face) { return face / 2; }
int signOf(int face) { return face % 2 == 0 ? 1 : -1; }

Scene::Vector3 vectorOf(const std::array<float, 3> &v) { return {v[0], v[1], v[2]}; }

// Which side of the line a to b `p` is on
float sideOf(ScreenPoint a, ScreenPoint b, ScreenPoint p) {
  return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

} // namespace

std::vector<ViewCube::Cell> ViewCube::cells(const OrbitView &view, ScreenPoint centre, float half) {
  // A face's three bands across each of its two directions: -1 the low
  // edge's strip, 0 the middle, 1 the high edge's strip
  const std::array<std::array<float, 2>, 3> bands = {
      {{-1, -kMiddle}, {-kMiddle, kMiddle}, {kMiddle, 1}}};
  std::vector<Cell> out;
  for (int face = 0; face < 6; ++face) {
    const int axis = axisOf(face), sign = signOf(face);
    std::array<float, 3> normal{};
    normal[axis] = static_cast<float>(sign);
    const float facing = view.inView(vectorOf(normal)).z;
    if (facing <= 1e-3f)
      continue;                                       // (turned away)
    const int u = (axis + 1) % 3, v = (axis + 2) % 3; // (the face's two directions)
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j) {
        Cell cell;
        cell.face = face;
        cell.facing = facing;
        cell.way[axis] = sign;
        cell.way[u] = i - 1;
        cell.way[v] = j - 1;
        const auto &bu = bands[i];
        const auto &bv = bands[j];
        const std::array<std::array<float, 2>, 4> square = {
            {{bu[0], bv[0]}, {bu[1], bv[0]}, {bu[1], bv[1]}, {bu[0], bv[1]}}};
        for (int k = 0; k < 4; ++k) {
          std::array<float, 3> p{};
          p[axis] = static_cast<float>(sign);
          p[u] = square[k][0];
          p[v] = square[k][1];
          cell.corners[k] = onScreen(view, centre, half, vectorOf(p));
        }
        out.push_back(cell);
      }
  }
  return out;
}

std::optional<ScreenPoint> ViewCube::faceMiddle(const OrbitView &view, ScreenPoint centre,
                                                float half, int face) {
  std::array<float, 3> normal{};
  normal[axisOf(face)] = static_cast<float>(signOf(face));
  if (view.inView(vectorOf(normal)).z <= 1e-3f)
    return std::nullopt;
  return onScreen(view, centre, half, vectorOf(normal));
}

ViewCube::FaceAxes ViewCube::faceAxes(const OrbitView &view, ScreenPoint centre, float half,
                                      int face) {
  std::array<float, 3> normal{};
  normal[axisOf(face)] = static_cast<float>(signOf(face));
  const Scene::Vector3 n = vectorOf(normal);
  // (the side faces read upright under the world's up; the top and bottom
  // read as they do seen from the front, tipped over)
  const bool side = axisOf(face) != 1;
  const Scene::Vector3 right = side ? Scene::Vector3{0, 1, 0}.cross(n) : Scene::Vector3{1, 0, 0};
  const Scene::Vector3 down =
      side ? Scene::Vector3{0, -1, 0} : Scene::Vector3{0, 0, static_cast<float>(signOf(face))};
  // (a direction's length on the screen: the orthographic view's x and y)
  const auto across = [&](const Scene::Vector3 &d) {
    const Scene::Vector3 seen = view.inView(d);
    return ScreenPoint{seen.x * half, -seen.y * half};
  };
  return {onScreen(view, centre, half, n), across(right), across(down)};
}

std::optional<ViewCube::Way> ViewCube::pick(const std::vector<Cell> &cells, ScreenPoint at) {
  for (const Cell &cell : cells) {
    // (inside a convex quad: on the same side of all four of its sides)
    bool below = false, above = false;
    for (int k = 0; k < 4; ++k) {
      const float side = sideOf(cell.corners[k], cell.corners[(k + 1) % 4], at);
      below = below || side < 0;
      above = above || side > 0;
    }
    if (!(below && above))
      return cell.way;
  }
  return std::nullopt;
}

std::vector<ScreenPoint> ViewCube::compass(const OrbitView &view, ScreenPoint centre, float half,
                                           int points) {
  std::vector<ScreenPoint> ring;
  for (int i = 0; i < points; ++i) {
    const float a = 2 * OrbitView::kPi * static_cast<float>(i) / static_cast<float>(points);
    ring.push_back(
        onScreen(view, centre, half,
                 {kCompassRadius * std::cos(a), -kCompassBelow, kCompassRadius * std::sin(a)}));
  }
  return ring;
}

std::array<ScreenPoint, 4> ViewCube::compassLetters(const OrbitView &view, ScreenPoint centre,
                                                    float half) {
  const float r = kCompassRadius + 0.3f;
  // (N away from the front, -z; E to the right, +x)
  return {onScreen(view, centre, half, {0, -kCompassBelow, -r}),
          onScreen(view, centre, half, {r, -kCompassBelow, 0}),
          onScreen(view, centre, half, {0, -kCompassBelow, r}),
          onScreen(view, centre, half, {-r, -kCompassBelow, 0})};
}

bool ViewCube::onCompass(const OrbitView &view, ScreenPoint centre, float half, ScreenPoint at,
                         float band) {
  const std::vector<ScreenPoint> ring = compass(view, centre, half);
  for (std::size_t i = 0; i < ring.size(); ++i) {
    // (the distance from `at` to each short piece of the ring)
    const ScreenPoint a = ring[i], b = ring[(i + 1) % ring.size()];
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float length2 = dx * dx + dy * dy;
    float t = length2 > 0 ? ((at.x - a.x) * dx + (at.y - a.y) * dy) / length2 : 0;
    t = std::fmax(0.0f, std::fmin(1.0f, t));
    if (std::hypot(at.x - (a.x + t * dx), at.y - (a.y + t * dy)) <= band)
      return true;
  }
  return false;
}

Scene::Vector3 ViewCube::directionOf(const Way &way) {
  return Scene::Vector3{static_cast<float>(way[0]), static_cast<float>(way[1]),
                        static_cast<float>(way[2])}
      .normalised();
}

std::optional<ViewCube::Way> ViewCube::nearest(const OrbitView &view, float radians) {
  const Scene::Vector3 eye = view.towardEye();
  std::optional<Way> best;
  float bestCos = std::cos(radians);
  for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y)
      for (int z = -1; z <= 1; ++z) {
        if (x == 0 && y == 0 && z == 0)
          continue;
        const Way way{x, y, z};
        const float c = directionOf(way).dot(eye);
        if (c >= bestCos) {
          bestCos = c;
          best = way;
        }
      }
  return best;
}

} // namespace AnimationLab
