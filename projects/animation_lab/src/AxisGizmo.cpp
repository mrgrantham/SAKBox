#include <AnimationLab/AxisGizmo.hpp>

#include <algorithm>
#include <cmath>

namespace AnimationLab {

namespace Scene = ScratchPadd::Scene;

namespace {
Scene::Vector3 endOf(int axis, int sign) {
  Scene::Vector3 v{0, 0, 0};
  (axis == 0 ? v.x : axis == 1 ? v.y : v.z) = static_cast<float>(sign);
  return v;
}
} // namespace

std::array<AxisGizmo::Ball, 6> AxisGizmo::balls(const OrbitView &view, ScreenPoint centre,
                                                float radius) {
  std::array<Ball, 6> out;
  for (int i = 0; i < 6; ++i) {
    Ball &ball = out[static_cast<std::size_t>(i)];
    ball.axis = i / 2;
    ball.sign = i % 2 == 0 ? 1 : -1;
    const Scene::Vector3 seen = view.inView(endOf(ball.axis, ball.sign));
    ball.at = {centre.x + seen.x * radius, centre.y - seen.y * radius};
    ball.depth = seen.z;
  }
  std::sort(out.begin(), out.end(), [](const Ball &a, const Ball &b) { return a.depth < b.depth; });
  return out;
}

std::optional<AxisGizmo::Ball> AxisGizmo::pick(const std::array<Ball, 6> &balls, ScreenPoint at,
                                               float ballRadius) {
  // (nearest first: the last drawn is on top)
  for (auto ball = balls.rbegin(); ball != balls.rend(); ++ball)
    if (std::hypot(at.x - ball->at.x, at.y - ball->at.y) <= ballRadius)
      return *ball;
  return std::nullopt;
}

Scene::Vector3 AxisGizmo::lookFrom(const OrbitView &view, const Ball &ball) {
  const Scene::Vector3 end = endOf(ball.axis, ball.sign);
  constexpr float kAlreadyThere = 0.9999f; // (within about a degree)
  OrbitView there = view;                  // (where it's going, if it's gliding)
  there.pose = view.destination();
  return there.towardEye().dot(end) > kAlreadyThere ? end * -1.0f : end;
}

} // namespace AnimationLab
