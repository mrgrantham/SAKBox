// PointsBars.hpp - each player's points at the top of the top screen
// (the user's design): a thin row for each player, a dash
// in their colour for each point and a grey one for each still to win,
// centred, with no background; the row of the player whose turn it is
// outlined in a colour none of the players have. Only rectangles, so every
// console draws them the same.
//
//   for (const PointsBars::Bar &bar : PointsBars::of(state, screenWidth))
//     overlay.rectangle(bar.x, bar.y, bar.width, bar.height, bar.colour);
#pragma once

#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Rules.hpp>

#include <ScratchPadd/Scene/Types.hpp>

#include <optional>
#include <vector>

namespace CosmicHex::Handheld::PointsBars {

// Sizes, in pixels (the DS's and the 3DS's are the same size)
inline constexpr float kDashWidth = 9, kDashGap = 2;
inline constexpr float kRowHeight = 2, kRowGap = 3;
inline constexpr float kTop = 6;
// The outline round the row whose turn it is: 1 pixel, turquoise (the
// players are red, blue, white and yellow)
inline constexpr float kOutline = 1;
inline constexpr ScratchPadd::Scene::Colour kOutlineColour = Palette::kAccent;
inline constexpr ScratchPadd::Scene::Colour kStillToWin{90, 100, 120};

struct Bar {
  float x, y, width, height;
  ScratchPadd::Scene::Colour colour;
};

// The rectangles for `state` on a screen `screenWidth` wide: the outline's
// four sides first, then the dashes (a player past the target shows the
// target's worth). The outline is round the turn's row, or at `outlineRow`
// when given: a row, or part way between two, as it moves from one
// player's row to the next when a turn ends (the user's)
std::vector<Bar> of(const State &state, int screenWidth,
                    std::optional<float> outlineRow = std::nullopt);

// How far down the screen they reach, for what goes under them
float bottom(int players);

} // namespace CosmicHex::Handheld::PointsBars
