// Keyboard.hpp - a desktop's keyboard (and mouse) as a controller. The
// window's padd publishes which keys are down (Scene::Display::keyboard());
// the reader (Reader.hpp) lays them onto the pad through a KeyMap, so a game
// plays the same with keys or a pad.
#pragma once

#include <ScratchPadd/Input/Gamepad.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace ScratchPadd::Input {

enum class Key : std::uint8_t {
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Digit0,
  Digit1,
  Digit2,
  Digit3,
  Digit4,
  Digit5,
  Digit6,
  Digit7,
  Digit8,
  Digit9,
  Space,
  Enter,
  Tab,
  Escape,
  Backspace,
  Left,
  Right,
  Up,
  Down, // the arrow keys
  LeftShift,
  RightShift,
  LeftAlt,
  RightAlt,
  LeftControl,
  RightControl,
  Count
};
inline constexpr std::size_t kKeys = static_cast<std::size_t>(Key::Count);
const char *nameOf(Key key); // "K", "Enter", "Left Shift"...

// The keys down, and where the cursor is (in points, for the mouse as the
// right stick), as a window's padd publishes them
struct Keyboard {
  std::array<std::uint64_t, (kKeys + 63) / 64> down{};
  float cursorX = 0, cursorY = 0;
  bool open = false; // a window publishes it (false: no keyboard to read)

  bool isDown(Key key) const {
    const auto k = static_cast<std::size_t>(key);
    return (down[k / 64] >> (k % 64) & 1) != 0;
  }
  void set(Key key, bool isDown) {
    const auto k = static_cast<std::size_t>(key);
    if (isDown)
      down[k / 64] |= std::uint64_t{1} << (k % 64);
    else
      down[k / 64] &= ~(std::uint64_t{1} << (k % 64));
  }
};

// Which keys are which parts of the pad
struct KeyMap {
  struct StickKeys {
    Key up, down, left, right;
  };
  std::vector<std::pair<Key, Button>> buttons;
  StickKeys leftStick, rightStick;
  // Held: the keys' sticks at half strength (walking, fine steering)
  std::vector<Key> halfStrength;
  // The mouse as the right stick: how far it moves the stick for each
  // point the cursor moves in a read (0: not used)
  float mouseRightStick = 0;

  // The keys of a pad's own places: WASD the left stick, IJKL the face
  // buttons (where they are on a pad), the arrows the right stick, TFGH the
  // D-pad, Q/E L and ZL, O/U R and ZR, the Shifts the stick clicks, Enter
  // Start and Tab Select; Alt for half strength
  static KeyMap standard();
  // The key for `button`, or nullptr
  const char *labelOf(Button button) const;
};

// `keyboard` laid onto `pad` through `map`: its buttons held as well, a
// stick pushed by keys or the mouse when the pad's is at rest (a key all the
// way, diagonals as far as straight), a trigger full while its button's key
// is down. `mouse` is how far the cursor moved since the last read.
void addKeyboard(Gamepad &pad, const Keyboard &keyboard, const KeyMap &map, float mouseX,
                 float mouseY);

} // namespace ScratchPadd::Input
