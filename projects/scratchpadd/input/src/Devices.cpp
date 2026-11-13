#include <ScratchPadd/Input/Devices.hpp>

#include <algorithm>
#include <cmath>

namespace ScratchPadd::Input {

namespace {

std::int16_t toAxis(float value) {
  return static_cast<std::int16_t>(std::lround(std::clamp(value, -1.0f, 1.0f) * kStickFull));
}
std::uint16_t toTrigger(float value) {
  return static_cast<std::uint16_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 65535.0f));
}
void hold(Gamepad &pad, Button button, bool down) {
  if (down)
    pad.held |= bitOf(button);
}
std::uint32_t featuresOf(Kind kind) { return profileOf(kind).features; }

} // namespace

Stick stickFrom(float x, float y) { return {toAxis(x), toAxis(y)}; }

Stick withoutDeadZone(Stick stick, float deadZone) {
  if (stick.x == 0 && stick.y == 0)
    return stick;
  const float x = static_cast<float>(stick.x) / kStickFull,
              y = static_cast<float>(stick.y) / kStickFull;
  const float length = std::sqrt(x * x + y * y);
  if (length <= deadZone)
    return {};
  const float reach = std::min(1.0f, (length - deadZone) / (1.0f - deadZone));
  return stickFrom(x / length * reach, y / length * reach);
}

Gamepad fromNintendo(Kind kind, std::uint32_t held, int circleX, int circleY, int cStickX,
                     int cStickY, Touch touch) {
  using K = NintendoKeys;
  Gamepad pad{.kind = kind, .features = featuresOf(kind)};
  // By position: Nintendo's A is the right button, B the bottom
  hold(pad, Button::FaceRight, held & K::kA);
  hold(pad, Button::FaceBottom, held & K::kB);
  hold(pad, Button::FaceTop, held & K::kX);
  hold(pad, Button::FaceLeft, held & K::kY);
  hold(pad, Button::L, held & K::kL);
  hold(pad, Button::R, held & K::kR);
  hold(pad, Button::Start, held & K::kStart);
  hold(pad, Button::Select, held & K::kSelect);
  hold(pad, Button::Up, held & K::kUp);
  hold(pad, Button::Down, held & K::kDown);
  hold(pad, Button::Left, held & K::kLeft);
  hold(pad, Button::Right, held & K::kRight);
  if (kind == Kind::NewThreeDs) {
    hold(pad, Button::ZL, held & K::kZl);
    hold(pad, Button::ZR, held & K::kZr);
    pad.leftTrigger = pad.isHeld(Button::ZL) ? 65535 : 0;
    pad.rightTrigger = pad.isHeld(Button::ZR) ? 65535 : 0;
  }
  if (kind == Kind::Ds) {
    // No stick: the D-pad is the left one too
    const float x = pad.isHeld(Button::Right) - pad.isHeld(Button::Left);
    const float y = pad.isHeld(Button::Up) - pad.isHeld(Button::Down);
    const float diagonal = (x != 0 && y != 0) ? 0.70710678f : 1.0f;
    pad.left = stickFrom(x * diagonal, y * diagonal);
  } else {
    constexpr float kReach = 156.0f; // the circle pad's, about
    pad.left = stickFrom(circleX / kReach, circleY / kReach);
    if (kind == Kind::NewThreeDs)
      pad.right = stickFrom(cStickX / kReach, cStickY / kReach);
  }
  pad.touch = touch;
  return pad;
}

Kind kindOfProduct(std::string_view product) {
  const auto has = [&](std::string_view part) {
    return product.find(part) != std::string_view::npos;
  };
  if (has("DualShock") || has("DualSense") || has("PlayStation"))
    return Kind::PlayStation;
  if (has("Xbox"))
    return Kind::Xbox;
  if (has("Switch") || has("Joy-Con") || has("Nintendo"))
    return Kind::SwitchPro;
  return Kind::Other;
}

Gamepad fromGameController(const GameControllerState &s) {
  const Kind kind = kindOfProduct(s.productCategory);
  Gamepad pad{.kind = kind, .features = featuresOf(kind)};
  hold(pad, Button::FaceBottom, s.bottom);
  hold(pad, Button::FaceRight, s.right);
  hold(pad, Button::FaceLeft, s.left);
  hold(pad, Button::FaceTop, s.top);
  hold(pad, Button::L, s.leftShoulder);
  hold(pad, Button::R, s.rightShoulder);
  hold(pad, Button::Start, s.menu);
  hold(pad, Button::Select, s.hasOptions && s.options);
  hold(pad, Button::LeftStickClick, s.hasThumbButtons && s.leftThumb);
  hold(pad, Button::RightStickClick, s.hasThumbButtons && s.rightThumb);
  hold(pad, Button::Up, s.up);
  hold(pad, Button::Down, s.down);
  hold(pad, Button::Left, s.leftDpad);
  hold(pad, Button::Right, s.rightDpad);
  // A trigger is ZL or ZR once half pressed
  pad.leftTrigger = toTrigger(s.leftTrigger);
  pad.rightTrigger = toTrigger(s.rightTrigger);
  hold(pad, Button::ZL, s.leftTrigger >= 0.5f);
  hold(pad, Button::ZR, s.rightTrigger >= 0.5f);
  pad.left = stickFrom(s.leftX, s.leftY);
  pad.right = stickFrom(s.rightX, s.rightY);
  return pad;
}

Gamepad fromXInput(const XInputState &s) {
  // XINPUT_GAMEPAD's button bits
  constexpr std::uint16_t kUp = 0x1, kDown = 0x2, kLeft = 0x4, kRight = 0x8, kStart = 0x10,
                          kBack = 0x20, kLeftThumb = 0x40, kRightThumb = 0x80,
                          kLeftShoulder = 0x100, kRightShoulder = 0x200, kA = 0x1000, kB = 0x2000,
                          kX = 0x4000, kY = 0x8000;
  Gamepad pad{.kind = Kind::Xbox, .features = featuresOf(Kind::Xbox)};
  const auto bit = [&](std::uint16_t b) { return (s.buttons & b) != 0; };
  hold(pad, Button::FaceBottom, bit(kA));
  hold(pad, Button::FaceRight, bit(kB));
  hold(pad, Button::FaceLeft, bit(kX));
  hold(pad, Button::FaceTop, bit(kY));
  hold(pad, Button::L, bit(kLeftShoulder));
  hold(pad, Button::R, bit(kRightShoulder));
  hold(pad, Button::Start, bit(kStart));
  hold(pad, Button::Select, bit(kBack));
  hold(pad, Button::LeftStickClick, bit(kLeftThumb));
  hold(pad, Button::RightStickClick, bit(kRightThumb));
  hold(pad, Button::Up, bit(kUp));
  hold(pad, Button::Down, bit(kDown));
  hold(pad, Button::Left, bit(kLeft));
  hold(pad, Button::Right, bit(kRight));
  pad.leftTrigger = static_cast<std::uint16_t>(s.leftTrigger * 257); // 255 → 65535
  pad.rightTrigger = static_cast<std::uint16_t>(s.rightTrigger * 257);
  hold(pad, Button::ZL, s.leftTrigger >= 128);
  hold(pad, Button::ZR, s.rightTrigger >= 128);
  const auto axis = [](std::int16_t v) {
    return static_cast<std::int16_t>(std::max<int>(v, -kStickFull));
  };
  pad.left = {axis(s.leftX), axis(s.leftY)};
  pad.right = {axis(s.rightX), axis(s.rightY)};
  return pad;
}

// --- evdev ---

namespace {
// Linux's input-event-codes.h
constexpr int kEvKey = 1, kEvAbs = 3;
constexpr int kAbsX = 0x00, kAbsY = 0x01, kAbsZ = 0x02, kAbsRx = 0x03, kAbsRy = 0x04, kAbsRz = 0x05,
              kAbsHat0X = 0x10, kAbsHat0Y = 0x11;
// The kernel's gamepad buttons, by position (BTN_SOUTH...)
Button buttonOfCode(int code, bool &known) {
  known = true;
  switch (code) {
  case 0x130:
    return Button::FaceBottom; // BTN_SOUTH
  case 0x131:
    return Button::FaceRight; // BTN_EAST
  case 0x133:
    return Button::FaceTop; // BTN_NORTH
  case 0x134:
    return Button::FaceLeft; // BTN_WEST
  case 0x136:
    return Button::L; // BTN_TL
  case 0x137:
    return Button::R; // BTN_TR
  case 0x138:
    return Button::ZL; // BTN_TL2
  case 0x139:
    return Button::ZR; // BTN_TR2
  case 0x13a:
    return Button::Select; // BTN_SELECT
  case 0x13b:
    return Button::Start; // BTN_START
  case 0x13d:
    return Button::LeftStickClick; // BTN_THUMBL
  case 0x13e:
    return Button::RightStickClick; // BTN_THUMBR
  case 0x220:
    return Button::Up; // BTN_DPAD_UP
  case 0x221:
    return Button::Down;
  case 0x222:
    return Button::Left;
  case 0x223:
    return Button::Right;
  default:
    known = false;
    return Button::Start;
  }
}
void setHeld(Gamepad &pad, Button button, bool down) {
  if (down)
    pad.held |= bitOf(button);
  else
    pad.held &= ~bitOf(button);
}
} // namespace

EvdevPad::EvdevPad(std::uint16_t vendor) {
  const Kind kind = vendor == 0x054c   ? Kind::PlayStation
                    : vendor == 0x045e ? Kind::Xbox
                    : vendor == 0x057e ? Kind::SwitchPro
                                       : Kind::Other;
  pad_.kind = kind;
  pad_.features = featuresOf(kind);
}

void EvdevPad::setRange(int code, Range range) {
  if (code >= 0 && code < 0x20)
    ranges_[code] = range;
}

float EvdevPad::normalised(int code, int value) const {
  const Range r = ranges_[code];
  if (r.maximum <= r.minimum)
    return 0;
  return (2.0f * (value - r.minimum) / (r.maximum - r.minimum)) - 1.0f;
}

void EvdevPad::setStick(Stick &stick, bool isY, float value) {
  (isY ? stick.y : stick.x) = toAxis(isY ? -value : value); // (evdev's y grows downward)
}

void EvdevPad::apply(int type, int code, int value) {
  if (type == kEvKey) {
    bool known = false;
    const Button button = buttonOfCode(code, known);
    if (known)
      setHeld(pad_, button, value != 0);
    return;
  }
  if (type != kEvAbs || code < 0 || code >= 0x20)
    return;
  switch (code) {
  case kAbsX:
    setStick(pad_.left, false, normalised(code, value));
    break;
  case kAbsY:
    setStick(pad_.left, true, normalised(code, value));
    break;
  case kAbsRx:
    setStick(pad_.right, false, normalised(code, value));
    break;
  case kAbsRy:
    setStick(pad_.right, true, normalised(code, value));
    break;
  case kAbsZ:
  case kAbsRz: {
    const Range r = ranges_[code];
    const float amount =
        r.maximum > r.minimum ? static_cast<float>(value - r.minimum) / (r.maximum - r.minimum) : 0;
    (code == kAbsZ ? pad_.leftTrigger : pad_.rightTrigger) = toTrigger(amount);
    break;
  }
  case kAbsHat0X: // a D-pad as a hat: -1, 0, 1
    setHeld(pad_, Button::Left, value < 0);
    setHeld(pad_, Button::Right, value > 0);
    break;
  case kAbsHat0Y:
    setHeld(pad_, Button::Up, value < 0);
    setHeld(pad_, Button::Down, value > 0);
    break;
  default:
    break;
  }
}

} // namespace ScratchPadd::Input
