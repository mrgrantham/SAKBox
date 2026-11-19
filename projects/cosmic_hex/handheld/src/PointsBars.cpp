#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>

#include <algorithm>

namespace CosmicHex::Handheld::PointsBars {

std::vector<Bar> of(const State &state, int screenWidth, std::optional<float> outlineRow) {
  const int target = state.settings.winPoints;
  const float width =
      static_cast<float>(target) * kDashWidth + static_cast<float>(target - 1) * kDashGap;
  const float left = (static_cast<float>(screenWidth) - width) / 2;
  std::vector<Bar> bars;
  const auto rowTop = [](int player) {
    return kTop + static_cast<float>(player) * (kRowHeight + kRowGap);
  };
  // The outline: a pixel clear of its row all round, inside the gap between
  // rows
  if (state.turn >= 0 && state.turn < state.players) {
    const float row = outlineRow.value_or(static_cast<float>(state.turn));
    const float x = left - 1 - kOutline, y = kTop + row * (kRowHeight + kRowGap) - 1 - kOutline;
    const float w = width + 2 + 2 * kOutline, h = kRowHeight + 2 + 2 * kOutline;
    bars.push_back({x, y, w, kOutline, kOutlineColour});
    bars.push_back({x, y + h - kOutline, w, kOutline, kOutlineColour});
    bars.push_back({x, y, kOutline, h, kOutlineColour});
    bars.push_back({x + w - kOutline, y, kOutline, h, kOutlineColour});
  }
  for (int p = 0; p < state.players; ++p) {
    const int points = std::min(CosmicHex::points(state, p), target);
    for (int k = 0; k < target; ++k)
      bars.push_back({left + static_cast<float>(k) * (kDashWidth + kDashGap), rowTop(p), kDashWidth,
                      kRowHeight, k < points ? BoardColours::ofPlayer(p) : kStillToWin});
  }
  return bars;
}

float bottom(int players) {
  return kTop + static_cast<float>(players) * (kRowHeight + kRowGap) + kOutline;
}

} // namespace CosmicHex::Handheld::PointsBars
