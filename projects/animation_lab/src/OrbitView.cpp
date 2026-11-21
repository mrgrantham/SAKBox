#include <AnimationLab/OrbitView.hpp>

#include <algorithm>
#include <cmath>

namespace AnimationLab {

namespace Scene = ScratchPadd::Scene;

namespace {
// An angle as the same turn between -pi and pi (the short way round)
float shortWay(float radians) {
  constexpr float kTurn = 2 * OrbitView::kPi;
  radians = std::fmod(radians, kTurn);
  if (radians > OrbitView::kPi)
    radians -= kTurn;
  if (radians < -OrbitView::kPi)
    radians += kTurn;
  return radians;
}
// Slow at both ends, quick in the middle
float eased(float along) { return along * along * (3 - 2 * along); }
} // namespace

Scene::Vector3 OrbitView::towardEye() const {
  return {std::cos(pose.pitch) * std::sin(pose.yaw), std::sin(pose.pitch),
          std::cos(pose.pitch) * std::cos(pose.yaw)};
}
Scene::Vector3 OrbitView::right() const { return {std::cos(pose.yaw), 0, -std::sin(pose.yaw)}; }
Scene::Vector3 OrbitView::up() const { return towardEye().cross(right()); }

Scene::Vector3 OrbitView::inView(const Scene::Vector3 &direction) const {
  return {direction.dot(right()), direction.dot(up()), direction.dot(towardEye())};
}

Scene::Camera OrbitView::camera() const {
  Scene::Camera c;
  c.target = pose.target;
  c.eye = pose.target + towardEye() * pose.distance;
  c.up = up(); // (the view's own: well defined looking straight down too)
  c.fieldOfView = fieldOfView;
  c.nearPlane = std::max(0.01f, pose.distance * 0.01f);
  c.farPlane = pose.distance * 20 + 100;
  return c;
}

void OrbitView::orbit(float dx, float dy) {
  glide_.reset(); // (the hand takes over)
  // (dragging right turns the scene right: the eye goes left round it)
  pose.yaw -= dx * kRadiansAPixel;
  pose.pitch = std::clamp(pose.pitch + dy * kRadiansAPixel, -kMostPitch, kMostPitch);
}

void OrbitView::pan(float dx, float dy, float height) {
  if (height <= 0)
    return;
  glide_.reset();
  // (world units a pixel, at the target's depth)
  const float perPixel = 2 * pose.distance * std::tan(fieldOfView * kPi / 360) / height;
  pose.target = pose.target - right() * (dx * perPixel) + up() * (dy * perPixel);
}

void OrbitView::zoomAt(float steps, float x, float y, float w, float h) {
  if (w <= 0 || h <= 0)
    return;
  glide_.reset();
  const float factor = std::pow(kZoomStep, -steps);
  const float next = std::clamp(pose.distance * factor, kNearest, kFarthest);
  // (the point under the cursor at the target's depth stays under it: the
  // target moves toward it by how much nearer the view comes)
  const float perPixel = 2 * pose.distance * std::tan(fieldOfView * kPi / 360) / h;
  const Scene::Vector3 underCursor =
      pose.target + right() * ((x - w / 2) * perPixel) - up() * ((y - h / 2) * perPixel);
  pose.target = underCursor + (pose.target - underCursor) * (next / pose.distance);
  pose.distance = next;
}

OrbitView::Pose OrbitView::looking(Side side) const {
  Pose to = pose;
  switch (side) {
  case Side::Front:
    to.yaw = 0, to.pitch = 0;
    break;
  case Side::Back:
    to.yaw = kPi, to.pitch = 0;
    break;
  case Side::Right:
    to.yaw = kPi / 2, to.pitch = 0;
    break;
  case Side::Left:
    to.yaw = -kPi / 2, to.pitch = 0;
    break;
  case Side::Top: // (as Blender's: the front at the bottom of the screen)
    to.yaw = 0, to.pitch = kMostPitch;
    break;
  case Side::Bottom:
    to.yaw = 0, to.pitch = -kMostPitch;
    break;
  }
  return to;
}

OrbitView::Pose OrbitView::facing(const Scene::Vector3 &way) const {
  Pose to = pose;
  const float across = std::hypot(way.x, way.z);
  if (across < 1e-6f) {
    // (straight up or down: the nearest square turn, so nothing spins)
    to.yaw = std::round(pose.yaw / (kPi / 2)) * (kPi / 2);
    to.pitch = way.y > 0 ? kMostPitch : -kMostPitch;
  } else {
    to.yaw = std::atan2(way.x, way.z);
    to.pitch = std::clamp(std::atan2(way.y, across), -kMostPitch, kMostPitch);
  }
  return to;
}

OrbitView::Pose OrbitView::framing(const Scene::Vector3 &low, const Scene::Vector3 &high,
                                   float aspect) const {
  Pose to = pose;
  to.target = (low + high) * 0.5f;
  const float radius = std::max((high - low).length() * 0.5f, 0.1f);
  const float halfHigh = std::tan(fieldOfView * kPi / 360);
  const float fits = std::min(halfHigh, halfHigh * aspect);
  to.distance = std::clamp(radius / fits * 1.1f, kNearest, kFarthest);
  return to;
}

void OrbitView::glideTo(const Pose &to, float seconds) {
  if (seconds <= 0) {
    pose = to;
    glide_.reset();
    return;
  }
  glide_ = Glide{.from = pose, .to = to, .seconds = seconds};
}

void OrbitView::advance(float seconds) {
  if (!glide_)
    return;
  glide_->done += seconds;
  const float along = std::min(glide_->done / glide_->seconds, 1.0f);
  pose = between(glide_->from, glide_->to, eased(along));
  if (along >= 1) {
    pose = glide_->to;
    glide_.reset();
  }
}

OrbitView::Pose OrbitView::between(const Pose &a, const Pose &b, float along) {
  Pose p;
  p.target = a.target + (b.target - a.target) * along;
  p.yaw = a.yaw + shortWay(b.yaw - a.yaw) * along;
  p.pitch = a.pitch + (b.pitch - a.pitch) * along;
  // (evenly in ratio: twice as far takes as long as half as far)
  p.distance = a.distance * std::pow(b.distance / a.distance, along);
  return p;
}

bool OrbitView::same(const Pose &a, const Pose &b) {
  constexpr float kTurn = 1e-3f; // (radians: a twentieth of a degree)
  constexpr float kMove = 1e-3f; // (a thousandth of the distance)
  const float scale = std::max(a.distance, b.distance);
  return std::fabs(shortWay(a.yaw - b.yaw)) < kTurn && std::fabs(a.pitch - b.pitch) < kTurn &&
         std::fabs(a.distance - b.distance) < kMove * scale &&
         (a.target - b.target).length() < kMove * scale;
}

void OrbitView::begin() {
  glide_.reset();
  before_ = pose;
}

void OrbitView::cancel() {
  if (!before_)
    return;
  pose = *before_;
  before_.reset();
}

} // namespace AnimationLab
