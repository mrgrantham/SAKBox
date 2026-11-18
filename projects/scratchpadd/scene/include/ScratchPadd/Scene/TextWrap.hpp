// TextWrap.hpp - text broken into lines that fit a width, at the spaces,
// for anywhere text is drawn (round 93, the user's: a card's words ran off
// its edge). The width is measured by whatever draws it: the overlay's font
// (Overlay::paragraph), or a picture's own.
//
//   wrapText("Take 2 Fuel from the bank", 60, measure)
//     // {"Take 2 Fuel", "from the bank"}, if those fit in 60 and more doesn't
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ScratchPadd::Scene {

// How wide a piece of text is drawn, in the caller's units
using MeasureText = std::function<float(std::string_view)>;

// `text`'s lines, each no wider than `width` where it can be: words go on a
// line while they fit, with one space between them. A '\n' always starts a
// new line (an empty one too, for "\n\n"); runs of spaces count as one. A
// word wider than a whole line is broken where it must be, at the last
// letter that fits (at least one letter a line).
std::vector<std::string> wrapText(std::string_view text, float width, const MeasureText &measure);

} // namespace ScratchPadd::Scene
