#include <CosmicHex/Handheld/Hud.hpp>

#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>

#include <algorithm>
#include <array>
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
  for (int p = 0; p < state.players; ++p)
    for (int d = 0; d < target; ++d) {
      const float size = pops.dash(p, d, f.now);
      if (size == 1)
        continue;
      const PointsBars::Row row = PointsBars::rowOf(p, f.width, target);
      const float x = row.dashX(d), y = row.top;
      f.overlay.rectangle(x, y, PointsBars::kDashWidth, PointsBars::kRowHeight,
                          PointsBars::kStillToWin);
      const float w = PointsBars::kDashWidth * popped(size);
      const float h = PointsBars::kRowHeight * (1 + 15 * std::max(0.0f, size - 1));
      f.overlay.rectangle(x + PointsBars::kDashWidth / 2 - w / 2,
                          y + PointsBars::kRowHeight / 2 - h / 2, w, h,
                          size > 1 ? Palette::kWords : BoardColours::ofPlayer(p));
    }
}

// A pointy-topped hexagon's outline, 1 point wide, `radius` to its corners
void hexagonOutline(Scene::Overlay &overlay, float x, float y, float radius, Scene::Colour colour) {
  constexpr float kPi = 3.14159265f;
  for (int i = 0; i < 6; ++i) {
    const float a = kPi / 6 + static_cast<float>(i) * kPi / 3, b = a + kPi / 3;
    overlay.line(x + radius * std::cos(a), y + radius * std::sin(a), x + radius * std::cos(b),
                 y + radius * std::sin(b), colour, 1);
  }
}

// The top corners (the user's): the round top left, in a turquoise hexagon
// with ROUND under it (calmer than the roll's amber: it's always there),
// and the badges top right, towards the next point (3 make one at once)
void corners(const TopFrame &f, const State &state, const HudLook &look) {
  const float k = f.k();
  Scene::Overlay &overlay = f.overlay;
  const float rx = kRoundHexagonX * k, ry = kRoundHexagonY * k;
  hexagonOutline(overlay, rx, ry, 13 * k, Palette::kAccent);
  overlay.textCentred(rx, ry, std::to_string(state.round), Palette::kWords, 2.0f);
  overlay.textCentred(rx, ry + 20 * k, "ROUND", Palette::kDimWords);
  const int whose = look.badgesOf;
  const int badges = whose >= 0 && whose < state.players
                         ? state.player[static_cast<std::size_t>(whose)].badges
                         : 0;
  // (2.6 apart on a DS: kBadgeRadius's hexagons are 10.4 across)
  constexpr float kBadgeSpacing = 13;
  const float bx = f.width - 38 * k;
  for (int i = 0; i < 3; ++i) {
    const float x = bx + static_cast<float>(i) * kBadgeSpacing * k, y = 16 * k;
    // (a badge just won popping in)
    const float size = look.pops ? look.pops->badge(whose, i, f.now) : 1;
    if (i >= badges || size < 1)
      drawBadge(overlay, x, y, kBadgeRadius * k, false);
    if (i < badges && size > 0)
      drawBadge(overlay, x, y, kBadgeRadius * k * popped(size), true);
  }
  overlay.textCentred(bx + kBadgeSpacing * k, 30 * k, "BADGES", Palette::kDimWords);
}

} // namespace

std::array<ScreenPoint, 10> badgeStar(float x, float y, float radius) {
  constexpr float kPi = 3.14159265f;
  // (inside the hexagon's inner circle, clear of its 1-point outline; its
  // inner corners half way out: fat, so it reads as a star when small)
  const float outer = std::max(0.0f, radius * std::cos(kPi / 6) - 1.5f), inner = 0.5f * outer;
  std::array<ScreenPoint, 10> points{};
  for (std::size_t i = 0; i < points.size(); ++i) {
    const float a = -kPi / 2 + static_cast<float>(i) * kPi / 5; // (from the top, clockwise)
    const float r = i % 2 == 0 ? outer : inner;
    points[i] = {x + r * std::cos(a), y + r * std::sin(a)};
  }
  return points;
}

void drawBadge(Scene::Overlay &overlay, float x, float y, float radius, bool earned) {
  if (!earned) {
    hexagonOutline(overlay, x, y, radius, kEmptyBadge);
    return;
  }
  hexagonOutline(overlay, x, y, radius, Palette::kHighlight);
  // (each point: two triangles from the middle, to it from the inner
  // corners either side)
  const std::array<ScreenPoint, 10> star = badgeStar(x, y, radius);
  for (std::size_t i = 0; i < star.size(); i += 2) {
    const ScreenPoint &before = star[(i + star.size() - 1) % star.size()], &tip = star[i],
                      &after = star[i + 1];
    overlay.triangle(x, y, before.x, before.y, tip.x, tip.y, Palette::kHighlight);
    overlay.triangle(x, y, tip.x, tip.y, after.x, after.y, Palette::kHighlight);
  }
}

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
