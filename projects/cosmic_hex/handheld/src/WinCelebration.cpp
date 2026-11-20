#include <CosmicHex/Handheld/WinCelebration.hpp>

#include <ScratchPadd/Motion/Motion.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace CosmicHex::Handheld {

using ScratchPadd::Scene::Vector3;

WinCelebration::WinCelebration(const std::vector<Vector3> &ships,
                               const std::vector<Vector3> &colonies, double now)
    : from_(ships), own_(ships.size()), start_(now) {
  // (the rest of three, at the colonies in turn: one at the board's middle
  // if there are none)
  for (std::size_t i = 0; from_.size() < kAtLeast; ++i)
    from_.push_back(colonies.empty() ? Vector3{} : colonies[i % colonies.size()]);
}

Vector3 WinCelebration::at(std::size_t ship, float t) const {
  // Its place in the ring (spaced evenly, all going round together)...
  const float pi = std::numbers::pi_v<float>;
  const float angle =
      2 * pi * (kTurnsASecond * t + static_cast<float>(ship) / static_cast<float>(from_.size()));
  const Vector3 ring{kRadius * std::cos(angle), kHeight, -kRadius * std::sin(angle)};
  // ...joined from where it was, rising (Gentle: easing off the board and
  // into the ring)
  const float joined = ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::Gentle,
                                                  std::clamp(t / kSecondsUp, 0.0f, 1.0f));
  return from_[ship] + (ring - from_[ship]) * joined;
}

ShipPose WinCelebration::pose(std::size_t ship, double now) const {
  ShipPose pose;
  if (ship >= from_.size())
    return pose;
  const auto t = static_cast<float>(now - start_);
  pose.at = at(ship, t);
  // Nose first; the push it needs (sideways: holding the turn) swivels its
  // flame, which burns throughout (it's flying for joy)
  constexpr float kH = 1.0f / 64;
  const Vector3 before = at(ship, t - kH), after = at(ship, t + kH);
  const Vector3 velocity = (after - before) * (1 / (2 * kH));
  const Vector3 push = (after + before - pose.at * 2) * (1 / (kH * kH));
  if (std::hypot(velocity.x, velocity.z) > 1e-4f)
    pose.heading = std::atan2(-velocity.z, velocity.x);
  const float c = std::cos(pose.heading), s = std::sin(pose.heading);
  const float forward = push.x * c - push.z * s, sideways = push.x * s + push.z * c;
  pose.flame = 0.8f;
  pose.gimbal = std::clamp(std::atan2(-sideways, std::max(forward, 1.0f)),
                           -BoardMotion::kMostGimbal, BoardMotion::kMostGimbal);
  return pose;
}

float WinCelebration::fadedIn(std::size_t ship, double now) const {
  if (ship < own_)
    return 1;
  return std::clamp(static_cast<float>(now - start_) / kSecondsFadeIn, 0.0f, 1.0f);
}

} // namespace CosmicHex::Handheld
