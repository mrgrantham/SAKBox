// Reader.hpp - reads the controller: the platform's own (GameController on
// a Mac, XInput on Windows, evdev on Linux, hid on a 3DS, the keys on a DS),
// with a desktop's keyboard laid on, the sticks' dead zone taken out and
// each button's presses counted. GamePadd reads one and publishes it; an
// app without padds can read one itself, once a frame:
//
//   Input::Reader reader;
//   Input::Gamepad before;
//   for (;;) {
//     const Input::Gamepad pad = reader.read();
//     const Input::Edges edges(before, pad);
//     if (edges.pressed(Input::Button::Start)) break;
//     before = pad;
//   }
//
// Read it on the main thread: on a Mac, pads are found only while the main
// thread's run loop runs, which each read runs for no time (values then
// update by themselves; see the input README).
#pragma once

#include <ScratchPadd/Input/Gamepad.hpp>
#include <ScratchPadd/Input/Keyboard.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace ScratchPadd::Input {

class Reader {
public:
  Reader();
  // Reading pads from `source` instead of the platform's (for tests)
  explicit Reader(Gamepad (*source)()) : source_(source) {}
  ~Reader();
  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;

  // The pad now. With several connected, the one used most recently; with
  // none, the keyboard alone (if given), or Kind::None.
  Gamepad read(const Keyboard *keyboard = nullptr);

  const KeyMap &keyMap() const { return keyMap_; }
  void setKeyMap(KeyMap map) { keyMap_ = std::move(map); }
  // How far a stick moves before it counts, as a fraction of its reach
  // (0.1 by default: a DualShock 4's sticks rest up to 0.02 off centre)
  void setDeadZone(float fraction) { deadZone_ = fraction; }

private:
  Gamepad (*source_)();
  KeyMap keyMap_ = KeyMap::standard();
  float deadZone_ = 0.1f;
  std::uint32_t lastHeld_ = 0;
  std::array<std::uint16_t, kButtons> presses_{}, releases_{};
  float lastCursorX_ = 0, lastCursorY_ = 0;
  bool haveCursor_ = false;
};

} // namespace ScratchPadd::Input
