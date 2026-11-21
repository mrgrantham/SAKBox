// LineStack.hpp - lines of text stacked one under another so they never
// overlap, on any screen and in any font: each line as tall as the font
// draws it at its scale (Overlay::lineHeight), a gap between one and the
// next, each line's place worked out from where the stack starts. Lines
// placed at fixed offsets instead overlap where a screen's height doesn't
// grow with its width (the 3DS's 400 x 240 against the DS's 256 x 192) or
// a font's lines are taller.
//
//   const auto lines = stackDown(4, std::array{1.0f, 2.0f}, 2);
//   overlay.textCentred(x, lines[0].middle(), "Juno discovers", colour, 1);
//   overlay.textCentred(x, lines[1].middle(), "Salvage", colour, 2);
#pragma once

#include <ScratchPadd/Scene/Overlay.hpp>

#include <array>
#include <cstddef>

namespace ScratchPadd::Scene {

// Where a line goes: its top and bottom, in points down the screen
struct LineBox {
  float top = 0, bottom = 0;
  float height() const { return bottom - top; }
  // Where Overlay::textCentred puts its letters' middle: inside the box
  float middle() const { return (top + bottom) / 2; }
};

// How tall a line is at a scale, in points: the platform's font
// (Overlay::lineHeight) unless another is asked for (a test's, say)
using LineHeight = float (*)(float scale);

// How tall lines at `scales` are, stacked with `gap` points between
template <std::size_t N>
float stackHeight(const std::array<float, N> &scales, float gap,
                  LineHeight lineHeight = Overlay::lineHeight) {
  float total = 0;
  for (const float scale : scales)
    total += lineHeight(scale);
  return total + gap * static_cast<float>(N > 0 ? N - 1 : 0);
}

// Lines at `scales` one under another, the first's top at `top`, `gap`
// points between: each line's box, in order
template <std::size_t N>
std::array<LineBox, N> stackDown(float top, const std::array<float, N> &scales, float gap,
                                 LineHeight lineHeight = Overlay::lineHeight) {
  std::array<LineBox, N> boxes{};
  float y = top;
  for (std::size_t i = 0; i < N; ++i) {
    boxes[i] = {y, y + lineHeight(scales[i])};
    y = boxes[i].bottom + gap;
  }
  return boxes;
}

// The same lines ending at `bottom` (the last line's bottom), first to last
template <std::size_t N>
std::array<LineBox, N> stackUp(float bottom, const std::array<float, N> &scales, float gap,
                               LineHeight lineHeight = Overlay::lineHeight) {
  return stackDown(bottom - stackHeight(scales, gap, lineHeight), scales, gap, lineHeight);
}

// The same lines centred on `middle`
template <std::size_t N>
std::array<LineBox, N> stackAround(float middle, const std::array<float, N> &scales, float gap,
                                   LineHeight lineHeight = Overlay::lineHeight) {
  return stackDown(middle - stackHeight(scales, gap, lineHeight) / 2, scales, gap, lineHeight);
}

} // namespace ScratchPadd::Scene
