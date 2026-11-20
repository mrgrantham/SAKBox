#include <ScratchPadd/Motion/BakedMotion.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ScratchPadd::Motion {

namespace {
// Where `f` (0 to 1) falls in a list of `count` evenly spaced entries: the
// entry before it, and how far on to the next
std::pair<std::size_t, float> between(float f, std::size_t count) {
  if (count < 2)
    return {0, 0};
  const float place = std::clamp(f, 0.0f, 1.0f) * static_cast<float>(count - 1);
  const auto before = std::min(static_cast<std::size_t>(place), count - 2);
  return {before, place - static_cast<float>(before)};
}
} // namespace

float BakedMotion::along(float t) const {
  if (timing.empty() || seconds <= 0)
    return t > 0 ? 1.0f : 0.0f;
  if (timing.size() == 1)
    return timing[0];
  const auto [i, u] = between(t / seconds, timing.size());
  return timing[i] + (timing[i + 1] - timing[i]) * u;
}

BakedMotion::Point BakedMotion::onPath(float s) const {
  if (path.empty())
    return {};
  if (path.size() == 1)
    return path[0];
  const auto [i, u] = between(s, path.size());
  // Catmull-Rom through the points either side: a smooth curve that
  // passes through every point. Past the ends, a point carried on in a
  // straight line (not the end repeated, which would slow it there: the
  // points are evenly spaced, and the timing counts on it)
  const auto beyond = [](const Point &end, const Point &next) {
    return Point{2 * end[0] - next[0], 2 * end[1] - next[1], 2 * end[2] - next[2]};
  };
  const Point &p1 = path[i], &p2 = path[i + 1];
  const Point p0 = i == 0 ? beyond(p1, p2) : path[i - 1];
  const Point p3 = i + 2 < path.size() ? path[i + 2] : beyond(p2, p1);
  const float u2 = u * u, u3 = u2 * u;
  Point out{};
  for (std::size_t c = 0; c < 3; ++c)
    out[c] =
        0.5f * (2 * p1[c] + (p2[c] - p0[c]) * u + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * u2 +
                (3 * p1[c] - p0[c] - 3 * p2[c] + p3[c]) * u3);
  return out;
}

} // namespace ScratchPadd::Motion
