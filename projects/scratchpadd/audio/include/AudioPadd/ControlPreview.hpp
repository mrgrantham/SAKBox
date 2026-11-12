#pragma once

// What AudioPadd plays when a control panel changes one of its controls, so
// the change is heard at its new level. Both AudioPadds (AudioPadd.hpp) play
// it, each on its own instrument:
//
// - master: a soft note, at the new master volume
// - sounds: a note among the sounds, at the new sounds volume
// - music:  a chord on the instrument, at the new music volume
// - mute:   unmuting plays a bright chord; muting is heard as the silence
//
// The previews control turns them off.
//
// A swipe changes a control many times a second, so a preview starts at
// most every kPreviewGap; in between, the one playing carries on, and the
// volumes apply to it as they change.

#include <ScratchPadd/Audio/Voice.hpp>
#include <ScratchPadd/Messages/Controls.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace AudioPaddPreview {

struct Preview {
  ScratchPadd::Audio::Group group; // the volume it plays under
  std::vector<int> keys;           // MIDI keys, played together
  float volume = 0.5f;             // each note's, before the controls' volumes
};

// The MIDI channel the instrument plays previews on (16): one apps leave
// alone, so a preview never cuts off a note of theirs
inline constexpr int kChannel = 15;
// How long a preview's notes are held before they're released
inline constexpr std::chrono::milliseconds kPreviewLength{250};
// The least time between previews' starts
inline constexpr std::chrono::milliseconds kPreviewGap{300};

// What to play for a change of control `name` from `previous` to `current`,
// if anything
inline std::optional<Preview> forChange(const std::string &name,
                                        const ScratchPadd::ControlTypeVariant &previous,
                                        const ScratchPadd::ControlTypeVariant &current) {
  using ScratchPadd::Audio::Group;
  using ScratchPadd::ControlType::Boolean;
  if (name == "master")
    return Preview{.group = Group::Sounds, .keys = {72}};
  if (name == "sounds")
    return Preview{.group = Group::Sounds, .keys = {79}};
  if (name == "music")
    return Preview{.group = Group::Music, .keys = {60, 64, 67}};
  if (name == "mute") {
    const auto *was = std::get_if<Boolean>(&previous);
    const auto *now = std::get_if<Boolean>(&current);
    if (was && now && was->value && !now->value)
      return Preview{.group = Group::Sounds, .keys = {84, 88, 91}};
  }
  return std::nullopt;
}

// Starts previews no closer together than kPreviewGap
class Pacer {
public:
  using Clock = std::chrono::steady_clock;

  // Whether a preview may start at `now` (and if so, it's counted as started)
  bool start(Clock::time_point now) {
    if (last_ && now - *last_ < kPreviewGap)
      return false;
    last_ = now;
    return true;
  }

private:
  std::optional<Clock::time_point> last_;
};

} // namespace AudioPaddPreview
