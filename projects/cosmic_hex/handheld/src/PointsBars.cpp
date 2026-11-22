#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>

#include <algorithm>

namespace CosmicHex::Handheld::PointsBars {

float rowTop(float row) { return kTop + row * (kRowHeight + kRowGap); }

Row rowOf(int player, float screenWidth, int target) {
  const float width =
      static_cast<float>(target) * kDashWidth + static_cast<float>(target - 1) * kDashGap;
  return {
      .left = (screenWidth - width) / 2, .top = rowTop(static_cast<float>(player)), .width = width};
}

std::vector<Bar> of(const State &state, int screenWidth, std::optional<float> outlineRow) {
  const int target = state.settings.winPoints;
  const auto screen = static_cast<float>(screenWidth);
  std::vector<Bar> bars;
  // The outline: a pixel clear of its row all round, inside the gap between
  // rows
  if (state.turn >= 0 && state.turn < state.players) {
    const Row row = rowOf(0, screen, target);
    const float x = row.left - 1 - kOutline,
                y = rowTop(outlineRow.value_or(static_cast<float>(state.turn))) - 1 - kOutline;
    const float w = row.width + 2 + 2 * kOutline, h = kRowHeight + 2 + 2 * kOutline;
    bars.push_back({x, y, w, kOutline, kOutlineColour});
    bars.push_back({x, y + h - kOutline, w, kOutline, kOutlineColour});
    bars.push_back({x, y, kOutline, h, kOutlineColour});
    bars.push_back({x + w - kOutline, y, kOutline, h, kOutlineColour});
  }
  for (int p = 0; p < state.players; ++p) {
    const Row row = rowOf(p, screen, target);
    const int points = std::min(CosmicHex::points(state, p), target);
    for (int k = 0; k < target; ++k)
      bars.push_back({row.dashX(k), row.top, kDashWidth, kRowHeight,
                      k < points ? BoardColours::ofPlayer(p) : kStillToWin});
  }
  return bars;
}

float bottom(int players) { return rowTop(static_cast<float>(players)) + kOutline; }

} // namespace CosmicHex::Handheld::PointsBars
