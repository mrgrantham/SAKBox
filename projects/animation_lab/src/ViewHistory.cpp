#include <AnimationLab/ViewHistory.hpp>

#include <algorithm>
#include <cmath>

namespace AnimationLab {

void ViewHistory::keep(const OrbitView::Pose &pose) {
  if (!steps_.empty() && OrbitView::same(steps_[at_], pose))
    return; // (still where a step is)
  steps_.push_back(pose);
  if (steps_.size() > kMostSteps)
    steps_.erase(steps_.begin());
  at_ = steps_.size() - 1;
}

void ViewHistory::watch(const OrbitView::Pose &now, float seconds, bool moving) {
  if (steps_.empty()) { // (the first view is the first step)
    keep(now);
    seen_ = now;
    return;
  }
  const bool changed = !seen_ || !OrbitView::same(*seen_, now);
  seen_ = now;
  stillFor_ = changed || moving ? 0 : stillFor_ + seconds;
  if (stillFor_ >= kStillSeconds)
    keep(now);
}

std::optional<OrbitView::Pose> ViewHistory::back(const OrbitView::Pose &now) {
  keep(now);
  if (at_ == 0)
    return std::nullopt;
  --at_;
  return steps_[at_];
}

std::optional<OrbitView::Pose> ViewHistory::forward(const OrbitView::Pose &now) {
  keep(now);
  if (at_ + 1 >= steps_.size())
    return std::nullopt;
  ++at_;
  return steps_[at_];
}

OrbitView::Pose ViewHistory::along(float place) const {
  if (steps_.empty())
    return {};
  place = std::clamp(place, 0.0f, static_cast<float>(steps_.size() - 1));
  const auto i = static_cast<std::size_t>(std::floor(place));
  if (i + 1 >= steps_.size())
    return steps_.back();
  return OrbitView::between(steps_[i], steps_[i + 1], place - static_cast<float>(i));
}

void ViewHistory::stopAt(float place) {
  if (steps_.empty())
    return;
  place = std::clamp(place, 0.0f, static_cast<float>(steps_.size() - 1));
  at_ = static_cast<std::size_t>(std::lround(place));
}

} // namespace AnimationLab
