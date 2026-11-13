#include <ScratchPadd/Input/Keyboard.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ScratchPadd::Input {

namespace {

constexpr const char *kKeyNames[kKeys] = {"A",
                                          "B",
                                          "C",
                                          "D",
                                          "E",
                                          "F",
                                          "G",
                                          "H",
                                          "I",
                                          "J",
                                          "K",
                                          "L",
                                          "M",
                                          "N",
                                          "O",
                                          "P",
                                          "Q",
                                          "R",
                                          "S",
                                          "T",
                                          "U",
                                          "V",
                                          "W",
                                          "X",
                                          "Y",
                                          "Z",
                                          "0",
                                          "1",
                                          "2",
                                          "3",
                                          "4",
                                          "5",
                                          "6",
                                          "7",
                                          "8",
                                          "9",
                                          "Space",
                                          "Enter",
                                          "Tab",
                                          "Escape",
                                          "Backspace",
                                          "Left",
                                          "Right",
                                          "Up",
                                          "Down",
                                          "Left Shift",
                                          "Right Shift",
                                          "Left Alt",
                                          "Right Alt",
                                          "Left Control",
                                          "Right Control"};

// A stick from its four keys: -1, 0 or 1 each way
void stickFromKeys(const Keyboard &keyboard, const KeyMap::StickKeys &keys, float &x, float &y) {
  x = static_cast<float>(keyboard.isDown(keys.right)) -
      static_cast<float>(keyboard.isDown(keys.left));
  y = static_cast<float>(keyboard.isDown(keys.up)) - static_cast<float>(keyboard.isDown(keys.down));
  // A diagonal as far as straight
  if (x != 0 && y != 0) {
    x *= 0.70710678f;
    y *= 0.70710678f;
  }
}

std::int16_t toStick(float value) {
  return static_cast<std::int16_t>(std::lround(std::clamp(value, -1.0f, 1.0f) * kStickFull));
}

// The pad's stick unless it's at rest, then the keys' (or mouse's)
void addStick(Stick &stick, float x, float y) {
  if ((stick.x == 0 && stick.y == 0) && (x != 0 || y != 0))
    stick = {toStick(x), toStick(y)};
}

} // namespace

const char *nameOf(Key key) {
  const auto k = static_cast<std::size_t>(key);
  return k < kKeys ? kKeyNames[k] : "";
}

KeyMap KeyMap::standard() {
  return KeyMap{
      .buttons = {{Key::J, Button::FaceLeft},
                  {Key::L, Button::FaceRight},
                  {Key::I, Button::FaceTop},
                  {Key::K, Button::FaceBottom},
                  {Key::Q, Button::L},
                  {Key::O, Button::R},
                  {Key::E, Button::ZL},
                  {Key::U, Button::ZR},
                  {Key::Enter, Button::Start},
                  {Key::Tab, Button::Select},
                  {Key::T, Button::Up},
                  {Key::G, Button::Down},
                  {Key::F, Button::Left},
                  {Key::H, Button::Right},
                  {Key::LeftShift, Button::LeftStickClick},
                  {Key::RightShift, Button::RightStickClick}},
      .leftStick = {Key::W, Key::S, Key::A, Key::D},
      .rightStick = {Key::Up, Key::Down, Key::Left, Key::Right},
      .halfStrength = {Key::LeftAlt, Key::RightAlt},
  };
}

const char *KeyMap::labelOf(Button button) const {
  for (const auto &[key, mapped] : buttons)
    if (mapped == button)
      return nameOf(key);
  return nullptr;
}

void addKeyboard(Gamepad &pad, const Keyboard &keyboard, const KeyMap &map, float mouseX,
                 float mouseY) {
  for (const auto &[key, button] : map.buttons)
    if (keyboard.isDown(key))
      pad.held |= bitOf(button);
  if (pad.isHeld(Button::ZL))
    pad.leftTrigger = std::max<std::uint16_t>(pad.leftTrigger, 65535);
  if (pad.isHeld(Button::ZR))
    pad.rightTrigger = std::max<std::uint16_t>(pad.rightTrigger, 65535);

  const bool half =
      std::ranges::any_of(map.halfStrength, [&](Key key) { return keyboard.isDown(key); });
  const float strength = half ? 0.5f : 1.0f;
  float x = 0, y = 0;
  stickFromKeys(keyboard, map.leftStick, x, y);
  addStick(pad.left, x * strength, y * strength);
  stickFromKeys(keyboard, map.rightStick, x, y);
  if (x == 0 && y == 0 && map.mouseRightStick != 0) {
    // (the cursor's y grows downward; a stick's up)
    x = mouseX * map.mouseRightStick;
    y = -mouseY * map.mouseRightStick;
  } else {
    x *= strength;
    y *= strength;
  }
  addStick(pad.right, x, y);
}

} // namespace ScratchPadd::Input
