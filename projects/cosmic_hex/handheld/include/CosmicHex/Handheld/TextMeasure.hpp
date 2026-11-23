// TextMeasure.hpp - how a layout worked out before drawing measures text:
// the platform's font (the overlay's and the Canvas's: Silkscreen on the
// DS, Orbitron elsewhere) unless another is asked for (a test laying out a
// DS's screen in Silkscreen on a desktop). The top screen's layouts
// (otherPlayerLayout) and the bottom screen's (tradeLayout, cardsLayout)
// take one.
//
//   const float wide = TextMeasure{}.width("of 4", 1);
#pragma once

#include <ScratchPadd/Scene/LineStack.hpp>
#include <ScratchPadd/Scene/Overlay.hpp>

#include <string_view>

namespace CosmicHex::Handheld {

// How wide a line of text is at a scale, in points
using TextWidth = float (*)(std::string_view text, float scale);
struct TextMeasure {
  ScratchPadd::Scene::LineHeight lineHeight = ScratchPadd::Scene::Overlay::lineHeight;
  TextWidth width = ScratchPadd::Scene::Overlay::textWidth;
};

} // namespace CosmicHex::Handheld
