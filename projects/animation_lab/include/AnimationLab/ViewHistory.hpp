// ViewHistory.hpp - Rewind: undo for the view. Every view you stopped at,
// in order, to step back and forward through or scrub along (an idea from
// "Safe 3D Navigation", Fitzmaurice et al., Autodesk, 2008: lost newcomers
// tried to undo their moves by hand, and experts used Rewind at once).
//
//   ViewHistory history;
//   history.watch(view.pose, seconds, view.dragging() || view.gliding()); // each frame
//   if (auto pose = history.back(view.destination())) view.glideTo(*pose); // Alt+Left
//   view.pose = history.along(place);   // scrubbing the strip: 0 to count() - 1
//
// A step is a view that's been still a moment (kStillSeconds) after it
// changed: a drag, a wheel's run of zooming, a numpad view, a glide's end.
// Nothing's lost: moving after going back adds a step at the end, and the
// steps you went back past stay. The oldest go after kMostSteps.
#pragma once

#include <AnimationLab/OrbitView.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace AnimationLab {

class ViewHistory {
public:
  static constexpr std::size_t kMostSteps = 50;
  static constexpr float kStillSeconds = 0.3f;

  // Each frame: the view now, the seconds since the last frame, and whether
  // it's still being moved (a drag held, a glide going). A view that's
  // changed and then stayed still kStillSeconds becomes a step
  void watch(const OrbitView::Pose &now, float seconds, bool moving);
  // The step before the one you're at, and after it (none at either end).
  // `now` is where the view is going (OrbitView::destination(), so a
  // second press mid-glide steps on, rather than noting the glide's
  // middle). A view not yet a step (moved, not yet still) is made one
  // first, so forward comes back to it
  std::optional<OrbitView::Pose> back(const OrbitView::Pose &now);
  std::optional<OrbitView::Pose> forward(const OrbitView::Pose &now);
  // Going to a place on the strip (0 to count() - 1), a step or between
  // two; letting go there makes the nearest step the one you're at
  OrbitView::Pose along(float place) const;
  void stopAt(float place);

  // Making `pose` a step now, if it isn't one (before scrubbing: the view
  // you were at stays on the strip)
  void keep(const OrbitView::Pose &pose);

  std::size_t count() const { return steps_.size(); }
  std::size_t at() const { return at_; } // (the step you're at)
  const OrbitView::Pose &step(std::size_t i) const { return steps_[i]; }

private:
  std::vector<OrbitView::Pose> steps_;
  std::size_t at_ = 0;
  std::optional<OrbitView::Pose> seen_; // (last frame's view)
  float stillFor_ = 0;
};

} // namespace AnimationLab
