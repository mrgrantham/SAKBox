// Fade.hpp - how bright a screen is part way through a fade, as the DS's
// brightness registers count it (StartupSplash's fades)
#pragma once

#include <algorithm>

namespace ScratchPadd {

// `frame` refreshes into a fade of `frames`: -16 black, 0 as drawn (in,
// from black up; out, back down)
constexpr int brightnessAt(int frame, int frames, bool in) {
  const int shown = std::clamp(frames > 0 ? frame * 16 / frames : 16, 0, 16);
  return in ? shown - 16 : -shown;
}

} // namespace ScratchPadd
