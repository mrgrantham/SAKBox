#pragma once

// The controls every AudioPadd has, whichever plays (see AudioPadd.hpp):
// master, sounds and music volume (0 to 1), and mute.

#include <ScratchPadd/Messages/Controls.hpp>

#include <utility>

inline ScratchPadd::ControlValueMap audioPaddControls() {
  const auto volume = [](double initial) {
    return ScratchPadd::ControlType::Double(initial, std::make_pair(0.0, 1.0));
  };
  return {{"master", volume(1.0)},
          {"sounds", volume(1.0)},
          {"music", volume(1.0)},
          {"mute", ScratchPadd::ControlType::Boolean(false)}};
}
