// Devices.hpp - each platform's controller, as plain numbers, made into a
// Gamepad. The platform files (src/Platform*.cpp, PlatformMac.mm) read
// their device into these and call them; the tests call them with made-up
// states, on any desktop. The sticks' dead zone and the press counts are
// the Reader's, after.
#pragma once

#include <ScratchPadd/Input/Gamepad.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace ScratchPadd::Input {

// The platform's pad now (the most recently used, of several), or
// Kind::None. Defined by the platform's file; for the Reader.
Gamepad readPlatformPad();

// Each button's presses and releases so far, where the platform counts them
// itself rather than the Reader from what it reads: on a DS, a thread of its
// own samples the buttons at every screen refresh, so a tap between two
// reads isn't lost when the reads come late. False elsewhere.
bool readPlatformPresses(std::array<std::uint16_t, kButtons> &presses,
                         std::array<std::uint16_t, kButtons> &releases);

// A stick from -1..1 floats (y up)
Stick stickFrom(float x, float y);
// The dead zone (a fraction of the reach) taken out of `stick`, the rest
// stretched to reach full again (so a slight push is a small value, not a
// jump to the dead zone's edge)
Stick withoutDeadZone(Stick stick, float deadZone);

// --- Nintendo: the DS's and 3DS's keys (libnds' and libctru's bits) ---

struct NintendoKeys {
  static constexpr std::uint32_t kA = 1u << 0, kB = 1u << 1, kSelect = 1u << 2, kStart = 1u << 3,
                                 kRight = 1u << 4, kLeft = 1u << 5, kUp = 1u << 6, kDown = 1u << 7,
                                 kR = 1u << 8, kL = 1u << 9, kX = 1u << 10, kY = 1u << 11,
                                 kZl = 1u << 14, kZr = 1u << 15; // (ZL, ZR: New 3DS)
};
// `held` the keys down (the D-pad's own bits: the 3DS's KEY_UP also has the
// circle pad's), `circle` and `cStick` as the 3DS reads them (about -156
// to 156; a DS has neither, so its D-pad is its left stick too)
Gamepad fromNintendo(Kind kind, std::uint32_t held, int circleX, int circleY, int cStickX,
                     int cStickY, Touch touch);

// --- Apple's GameController ---

struct GameControllerState {
  std::string_view productCategory; // "DualShock 4", "Xbox One"...
  // By position (Apple's buttonA is the bottom one on every pad)
  bool bottom, right, left, top;
  bool leftShoulder, rightShoulder, menu, options, leftThumb, rightThumb;
  bool up, down, leftDpad, rightDpad;
  float leftX, leftY, rightX, rightY; // -1..1, y up
  float leftTrigger, rightTrigger;    // 0..1
  bool hasThumbButtons, hasOptions;
};
Kind kindOfProduct(std::string_view productCategory);
Gamepad fromGameController(const GameControllerState &state);

// --- XInput (Windows) ---

struct XInputState { // XINPUT_GAMEPAD's fields
  std::uint16_t buttons;
  std::uint8_t leftTrigger, rightTrigger;
  std::int16_t leftX, leftY, rightX, rightY;
};
Gamepad fromXInput(const XInputState &state);

// --- evdev (Linux) ---

// One pad's evdev device, its events applied as they're read
class EvdevPad {
public:
  struct Range {
    int minimum = -32768, maximum = 32767;
  };
  // `vendor` its USB vendor (Sony, Microsoft, Nintendo), for its kind
  explicit EvdevPad(std::uint16_t vendor);
  // An axis's range, as EVIOCGABS gives it
  void setRange(int code, Range range);
  // An event (struct input_event's type, code and value)
  void apply(int type, int code, int value);
  const Gamepad &pad() const { return pad_; }

private:
  float normalised(int code, int value) const;
  void setStick(Stick &stick, bool isY, float value);
  Gamepad pad_;
  Range ranges_[0x20];
};

} // namespace ScratchPadd::Input
