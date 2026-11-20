#include <AnimationLab/OrbitView.hpp>

#include <algorithm>
#include <cmath>

namespace AnimationLab {

namespace Scene = ScratchPadd::Scene;

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kMostPitch = kPi / 2 - 0.001f; // (never quite straight down: up stays up)
} // namespace

Scene::Vector3 OrbitView::towardEye() const {
  return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
}
Scene::Vector3 OrbitView::right() const { return {std::cos(yaw), 0, -std::sin(yaw)}; }
Scene::Vector3 OrbitView::up() const { return towardEye().cross(right()); }

Scene::Camera OrbitView::camera() const {
  Scene::Camera c;
  c.target = target;
  c.eye = target + towardEye() * distance;
  c.up = up(); // (the view's own: well defined looking straight down too)
  c.fieldOfView = fieldOfView;
  c.nearPlane = std::max(0.01f, distance * 0.01f);
  c.farPlane = distance * 20 + 100;
  return c;
}

void OrbitView::orbit(float dx, float dy) {
  // (dragging right turns the scene right: the eye goes left round it)
  yaw -= dx * kRadiansAPixel;
  pitch = std::clamp(pitch + dy * kRadiansAPixel, -kMostPitch, kMostPitch);
}

void OrbitView::pan(float dx, float dy, float height) {
  if (height <= 0)
    return;
  // (world units a pixel, at the target's depth)
  const float perPixel = 2 * distance * std::tan(fieldOfView * kPi / 360) / height;
  target = target - right() * (dx * perPixel) + up() * (dy * perPixel);
}

void OrbitView::zoomAt(float steps, float x, float y, float w, float h) {
  if (w <= 0 || h <= 0)
    return;
  const float factor = std::pow(kZoomStep, -steps);
  const float next = std::clamp(distance * factor, kNearest, kFarthest);
  // (the point under the cursor at the target's depth stays under it: the
  // target moves toward it by how much nearer the view comes)
  const float perPixel = 2 * distance * std::tan(fieldOfView * kPi / 360) / h;
  const Scene::Vector3 underCursor =
      target + right() * ((x - w / 2) * perPixel) - up() * ((y - h / 2) * perPixel);
  target = underCursor + (target - underCursor) * (next / distance);
  distance = next;
}

void OrbitView::look(Side side) {
  switch (side) {
  case Side::Front:
    yaw = 0, pitch = 0;
    break;
  case Side::Back:
    yaw = kPi, pitch = 0;
    break;
  case Side::Right:
    yaw = kPi / 2, pitch = 0;
    break;
  case Side::Left:
    yaw = -kPi / 2, pitch = 0;
    break;
  case Side::Top:
    yaw = 0, pitch = kMostPitch;
    break;
  case Side::Bottom:
    yaw = 0, pitch = -kMostPitch;
    break;
  }
}

void OrbitView::frame(const Scene::Vector3 &low, const Scene::Vector3 &high, float aspect) {
  target = (low + high) * 0.5f;
  const float radius = std::max((high - low).length() * 0.5f, 0.1f);
  const float halfHigh = std::tan(fieldOfView * kPi / 360);
  const float fits = std::min(halfHigh, halfHigh * aspect);
  distance = std::clamp(radius / fits * 1.1f, kNearest, kFarthest);
}

void OrbitView::cancel() {
  if (!before_)
    return;
  target = before_->target;
  yaw = before_->yaw;
  pitch = before_->pitch;
  distance = before_->distance;
  before_.reset();
}

} // namespace AnimationLab
