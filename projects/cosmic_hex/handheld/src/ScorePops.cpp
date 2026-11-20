#include <CosmicHex/Handheld/ScorePops.hpp>

#include <ScratchPadd/Motion/Motion.hpp>

#include <algorithm>

namespace CosmicHex::Handheld {

void ScorePops::risen(std::vector<Rise> &rises, const std::vector<int> &values, double at,
                      bool first) {
  rises.resize(values.size());
  for (std::size_t p = 0; p < values.size(); ++p) {
    Rise &rise = rises[p];
    if (first || values[p] < rise.to) { // (the start, or fewer: nothing pops)
      rise = {values[p], values[p], -1e9};
      continue;
    }
    if (values[p] > rise.to)
      rise = {rise.to, values[p], at};
  }
}

void ScorePops::saw(const std::vector<int> &points, const std::vector<int> &badges, double now) {
  risen(points_, points, now, !seen_);
  risen(badges_, badges, now, !seen_);
  seen_ = true;
}

float ScorePops::sizeOf(const std::vector<Rise> &rises, int player, int index, double now) {
  if (player < 0 || static_cast<std::size_t>(player) >= rises.size())
    return 1;
  const Rise &rise = rises[static_cast<std::size_t>(player)];
  if (index < rise.from || index >= rise.to)
    return 1;
  const auto t =
      static_cast<float>(now - rise.at) - kStagger * static_cast<float>(index - rise.from);
  if (t <= 0)
    return 0;
  return ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::Settle,
                                    std::min(1.0f, t / kSeconds));
}

float ScorePops::dash(int player, int dash, double now) const {
  return sizeOf(points_, player, dash, now);
}

float ScorePops::badge(int player, int badge, double now) const {
  return sizeOf(badges_, player, badge, now);
}

bool ScorePops::popping(double now) const {
  const auto busy = [&](const std::vector<Rise> &rises) {
    return std::any_of(rises.begin(), rises.end(), [&](const Rise &r) {
      return r.to > r.from &&
             now - r.at < kSeconds + kStagger * static_cast<double>(r.to - r.from - 1);
    });
  };
  return busy(points_) || busy(badges_);
}

} // namespace CosmicHex::Handheld
