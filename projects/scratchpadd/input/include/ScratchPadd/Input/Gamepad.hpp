// Gamepad.hpp - the controller, the same on every platform: one struct that
// each platform's reader fills in (Reader.hpp), and GamePadd publishes as
// shared state.
//
//   const Input::Gamepad pad = currentState<Input::Gamepad>().value;
//   if (pad.isHeld(Input::Button::FaceBottom)) jump();
//   const float steer = Input::axis<float>(pad.left.x);
//   overlay.text(x, y, std::format("{} to jump", pad.labelOf(Input::Button::FaceBottom)));
//
// - Face buttons are named by where they are, not what they're called:
//   Nintendo's A is the right one, Xbox's A the bottom one. labelOf() gives
//   the name on the pad in hand, from its profile.
// - A part a pad doesn't have reads as at rest (a stick centred, a button
//   up); `features` says what it has.
// - Numbers are integers, as the DS has no floating-point hardware; axis()
//   turns them into a float or a fixed-point type.
// - held says what's down now; presses and releases count each button's,
//   so a reader comparing with its last copy (Edges) sees every press once,
//   even one that came and went between its reads.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ScratchPadd::Input {

enum class Button : std::uint8_t {
  FaceLeft,
  FaceRight,
  FaceTop,
  FaceBottom,
  L,
  R,
  ZL,
  ZR,
  Start,
  Select,
  Up, // the D-pad
  Down,
  Left,
  Right,
  LeftStickClick,
  RightStickClick,
};
inline constexpr std::size_t kButtons = 16;

constexpr std::uint32_t bitOf(Button button) { return 1u << static_cast<unsigned>(button); }

// What a pad has, beyond the D-pad, face buttons, L, R, Start, Select and a
// left stick (the DS's left stick is its D-pad)
enum class Feature : std::uint32_t {
  RightStick = 1,
  ZButtons = 2,
  AnalogTriggers = 4,
  StickClicks = 8,
  Touch = 16,
};

enum class Kind : std::uint8_t {
  None, // nothing to read: no pad, no keyboard
  Keyboard,
  Xbox,
  PlayStation,
  SwitchPro,
  Ds,
  ThreeDs,
  NewThreeDs,
  Other, // a pad of no kind above, named as an Xbox pad
};

// -32767 (left, down) to 32767 (right, up), the dead zone taken out
struct Stick {
  std::int16_t x = 0, y = 0;
};
inline constexpr std::int16_t kStickFull = 32767;

// The touch screen, while touched: pixels from its top-left
struct Touch {
  bool down = false;
  std::int16_t x = 0, y = 0;
};

struct Gamepad {
  Kind kind = Kind::None;
  std::uint32_t features = 0; // Feature bits
  std::uint32_t held = 0;     // bitOf(Button) for each button down
  // How many times each button has gone down and come up since reading
  // began (wrapping round)
  std::array<std::uint16_t, kButtons> presses{}, releases{};
  Stick left, right;
  std::uint16_t leftTrigger = 0, rightTrigger = 0; // 0 to 65535; on a pad
                                                   // without analog
                                                   // triggers, ZL and ZR
                                                   // as 0 or 65535
  Touch touch;

  bool isHeld(Button button) const { return (held & bitOf(button)) != 0; }
  bool has(Feature feature) const { return (features & static_cast<std::uint32_t>(feature)) != 0; }
  // What `button` is called on this pad ("A", "Cross", "LB", "K"...)
  const char *labelOf(Button button) const;
};

// What a kind of pad has and calls its buttons
struct Profile {
  Kind kind;
  const char *name; // "PlayStation", "Xbox"...
  std::uint32_t features;
  std::array<const char *, kButtons> labels; // in Button's order
};
const Profile &profileOf(Kind kind);
// The keyboard's labels are its keys', from the key map (KeyMap.hpp)
const char *labelOf(Button button, Kind kind);

// The presses and releases between a reader's last copy and this one
class Edges {
public:
  Edges(const Gamepad &before, const Gamepad &now);
  bool pressed(Button button) const { return presses(button) > 0; }
  bool released(Button button) const { return releases(button) > 0; }
  unsigned presses(Button button) const { return presses_[static_cast<std::size_t>(button)]; }
  unsigned releases(Button button) const { return releases_[static_cast<std::size_t>(button)]; }
  // A bit for each button pressed at least once (bitOf)
  std::uint32_t pressedBits() const;

private:
  std::array<std::uint16_t, kButtons> presses_{}, releases_{};
};

// A stick's or trigger's value as a number from -1 to 1 (0 to 1 for a
// trigger): float, double, or a fixed-point type made from a float
template <typename Number> Number axis(std::int16_t value) {
  return Number(static_cast<float>(value) / kStickFull);
}
template <typename Number> Number trigger(std::uint16_t value) {
  return Number(static_cast<float>(value) / 65535.0f);
}

} // namespace ScratchPadd::Input
