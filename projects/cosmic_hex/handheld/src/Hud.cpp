#include <CosmicHex/Handheld/Hud.hpp>

#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;

namespace {

// How big a pop draws something (ScorePops' size): its overshoot made
// plain (Settle's is a tenth: on a dash 2 pixels high, nothing to see)
float popped(float size) { return size + 3 * std::max(0.0f, size - 1); }

// Points just won popping in over their dashes: grey till its turn, then
// growing past its size in white and settling in the player's colour
void pointsPopping(const TopFrame &f, const State &state, const ScorePops &pops) {
  if (!pops.popping(f.now))
    return;
  const int target = state.settings.winPoints;
  const float width = static_cast<float>(target) * PointsBars::kDashWidth +
                      static_cast<float>(target - 1) * PointsBars::kDashGap;
  const float left = (f.width - width) / 2;
  for (int p = 0; p < state.players; ++p)
    for (int d = 0; d < target; ++d) {
      const float size = pops.dash(p, d, f.now);
      if (size == 1)
        continue;
      const float x =
          left + static_cast<float>(d) * (PointsBars::kDashWidth + PointsBars::kDashGap);
      const float y =
          PointsBars::kTop + static_cast<float>(p) * (PointsBars::kRowHeight + PointsBars::kRowGap);
      f.overlay.rectangle(x, y, PointsBars::kDashWidth, PointsBars::kRowHeight,
                          PointsBars::kStillToWin);
      const float w = PointsBars::kDashWidth * popped(size);
      const float h = PointsBars::kRowHeight * (1 + 15 * std::max(0.0f, size - 1));
      f.overlay.rectangle(x + PointsBars::kDashWidth / 2 - w / 2,
                          y + PointsBars::kRowHeight / 2 - h / 2, w, h,
                          size > 1 ? Palette::kWords : BoardColours::ofPlayer(p));
    }
}

// The top corners (the user's): the round top left, in a turquoise hexagon
// with ROUND under it (calmer than the roll's amber: it's always there),
// and the badges top right, towards the next point (3 make one at once)
void corners(const TopFrame &f, const State &state, const HudLook &look) {
  const float k = f.k();
  Scene::Overlay &overlay = f.overlay;
  constexpr Scene::Colour kEmpty{90, 100, 120};
  const auto hexagonEdge = [&](float x, float y, float r, Scene::Colour colour) {
    for (int i = 0; i < 6; ++i) {
      const float a = 3.14159265f / 6 + static_cast<float>(i) * 3.14159265f / 3;
      const float b = a + 3.14159265f / 3;
      overlay.line(x + r * std::cos(a), y + r * std::sin(a), x + r * std::cos(b),
                   y + r * std::sin(b), colour, 1);
    }
  };
  const float rx = 22 * k, ry = 19 * k;
  hexagonEdge(rx, ry, 13 * k, Palette::kAccent);
  overlay.textCentred(rx, ry, std::to_string(state.round), Palette::kWords, 2.0f);
  overlay.textCentred(rx, ry + 20 * k, "ROUND", Palette::kDimWords);
  const int whose = look.badgesOf;
  const int badges = whose >= 0 && whose < state.players
                         ? state.player[static_cast<std::size_t>(whose)].badges
                         : 0;
  const float bx = f.width - 36 * k;
  for (int i = 0; i < 3; ++i) {
    const float x = bx + static_cast<float>(i) * 12 * k, y = 16 * k;
    // (a badge just won popping in)
    const float size = look.pops ? look.pops->badge(whose, i, f.now) : 1;
    if (i < badges && size > 0)
      overlay.hexagon(x, y, 5 * k * popped(size), Palette::kHighlight, true);
    if (i >= badges || size < 1)
      hexagonEdge(x, y, 5 * k, kEmpty);
  }
  overlay.textCentred(bx + 12 * k, 30 * k, "BADGES", Palette::kDimWords);
}

} // namespace

void drawHud(const TopFrame &f, const State &state, const HudLook &look) {
  for (const PointsBars::Bar &bar :
       PointsBars::of(state, static_cast<int>(f.width), look.outlineRow))
    f.overlay.rectangle(bar.x, bar.y, bar.width, bar.height, bar.colour);
  if (look.pops)
    pointsPopping(f, state, *look.pops);
  corners(f, state, look);
  if (!look.lastMove.empty())
    f.overlay.text(4, f.height - 14, look.lastMove, Palette::kSoftWords);
  if (!look.winner.empty() && state.winner >= 0)
    f.overlay.textCentred(f.width / 2, f.height / 2, look.winner,
                          BoardColours::ofPlayer(state.winner), f.width >= 400 ? 3.0f : 2.5f);
}

} // namespace CosmicHex::Handheld
