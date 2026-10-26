// SnapshotTimeline.hpp - smooth drawing from the physics padd's snapshots.
//
// The physics padd steps on its own clock and sends a snapshot each tick, so
// snapshots arrive at its rate, not the screen's: drawing the newest one as it
// is makes motion jump whenever the two rates don't line up (a 120 Hz screen,
// 20 steps a second). The timeline draws the ragdoll a little in the past
// instead, blended between the snapshots either side of that moment, so it
// moves smoothly at any rate, a step or so behind the physics.
#pragma once

#include "RagdollMessages.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

namespace CyclonePadds {

class SnapshotTimeline {
public:
  // An offset between the physics' time and the window's further off than
  // this (a pause, a stall) is taken at once rather than eased towards
  static constexpr double kResyncSeconds = 0.25;
  // How far each snapshot eases the offset towards its own (smoothing out
  // when snapshots happen to arrive)
  static constexpr double kOffsetEasing = 0.05;
  static constexpr std::size_t kMostSnapshots = 32;

  // How far behind the physics to draw. Enough for a snapshot on each side:
  // one physics tick plus one step.
  void setDelay(double seconds) { delay_ = seconds; }
  double delay() const { return delay_; }

  // Adds a snapshot that arrived at window time `now` (seconds). A new
  // generation (the ragdoll was reset) replaces the old ones, so the reset
  // isn't blended.
  void add(const RagdollSnapshot &snapshot, double now) {
    if (!snapshots_.empty() && snapshot.generation != snapshots_.back().generation) {
      snapshots_.clear();
    }
    const double offset = snapshot.time - now;
    if (!synced_ || std::abs(offset - offset_) > kResyncSeconds) {
      offset_ = offset;
      synced_ = true;
    } else {
      offset_ += kOffsetEasing * (offset - offset_);
    }
    snapshots_.push_back(snapshot);
    while (snapshots_.size() > kMostSnapshots) {
      snapshots_.pop_front();
    }
  }

  const RagdollSnapshot *newest() const {
    return snapshots_.empty() ? nullptr : &snapshots_.back();
  }

  // The physics' time to draw at window time `now`
  double drawTime(double now) const { return now + offset_ - delay_; }

  // The bones to draw at window time `now`: blended between the snapshots
  // either side of drawTime(now) (the oldest or newest outside them), or,
  // not blending, the newest as it is.
  std::vector<BoneSnapshot> bonesAt(double now, bool blend) const {
    if (snapshots_.empty()) {
      return {};
    }
    if (!blend) {
      return snapshots_.back().bones;
    }
    const double time = drawTime(now);
    auto after = std::find_if(snapshots_.begin(), snapshots_.end(),
                              [&](const RagdollSnapshot &s) { return s.time > time; });
    if (after == snapshots_.begin()) {
      return snapshots_.front().bones;
    }
    if (after == snapshots_.end()) {
      return snapshots_.back().bones;
    }
    const RagdollSnapshot &before = *(after - 1);
    const double t = (time - before.time) / (after->time - before.time);
    return blended(before.bones, after->bones, static_cast<float>(t));
  }

  // Each bone `t` of the way from `a` to `b` (0 to 1): positions in a
  // straight line, orientations along the shortest turn
  static std::vector<BoneSnapshot> blended(const std::vector<BoneSnapshot> &a,
                                           const std::vector<BoneSnapshot> &b, float t) {
    std::vector<BoneSnapshot> bones = b;
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
      bones[i].position = glm::mix(a[i].position, b[i].position, t);
      bones[i].orientation = glm::slerp(a[i].orientation, b[i].orientation, t);
    }
    return bones;
  }

private:
  std::deque<RagdollSnapshot> snapshots_; // oldest first, all one generation
  double delay_ = 2.0 / 60;
  double offset_ = 0; // the physics' time less the window's
  bool synced_ = false;
};

} // namespace CyclonePadds
