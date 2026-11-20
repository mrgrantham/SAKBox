#include <ScratchPadd/Motion/Motion.hpp>

#include <algorithm>

namespace ScratchPadd::Motion {

float along(Curve curve, float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  switch (curve) {
  case Curve::Linear:
    return t;
  case Curve::EaseIn:
    return t * t;
  case Curve::EaseOut:
    return 1 - (1 - t) * (1 - t);
  case Curve::EaseInOut:
    return t * t * (3 - 2 * t);
  case Curve::Gentle:
    return t * t * t * (t * (6 * t - 15) + 10);
  case Curve::Pull:
    // (each half a parabola reaching the middle at half the time)
    return t < 0.5f ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t);
  case Curve::Anticipate: {
    // (the mirror of Settle: back about a tenth at its lowest)
    constexpr float kBack = 1.70158f;
    return t * t * ((kBack + 1) * t - kBack);
  }
  case Curve::Settle: {
    // (the usual "back" curve: an overshoot of about 10% at its most)
    constexpr float kBack = 1.70158f;
    const float u = t - 1;
    return 1 + (kBack + 1) * u * u * u + kBack * u * u;
  }
  }
  return t;
}

Script::Step Script::push(float start, float seconds) {
  steps_.push_back({start, std::max(0.0f, seconds)});
  return steps_.size() - 1;
}

Script::Step Script::add(float seconds, float delay) { return push(delay, seconds); }

Script::Step Script::after(Step previous, float seconds, float delay) {
  return push(endOf(previous) + delay, seconds);
}

Script::Step Script::with(Step other, float seconds, float delay) {
  return push(startOf(other) + delay, seconds);
}

float Script::progress(Step step, float elapsed, Curve curve) const {
  const Span &span = steps_[step];
  if (span.seconds <= 0)
    return elapsed >= span.start ? 1.0f : 0.0f;
  return along(curve, (elapsed - span.start) / span.seconds);
}

float Script::seconds() const {
  float end = 0;
  for (const Span &span : steps_)
    end = std::max(end, span.start + span.seconds);
  return end;
}

int FixedSteps::due(double now, int most) {
  int steps = 0;
  while (next_ + step_ <= now && steps < most) {
    next_ += step_;
    ++steps;
  }
  if (next_ + step_ <= now)
    next_ = now - step_ * 0.5; // (behind by more than `most`: the rest dropped)
  return steps;
}

} // namespace ScratchPadd::Motion
