#include <ScratchPadd/Input/Gamepad.hpp>

namespace ScratchPadd::Input {

namespace {

constexpr std::uint32_t operator|(Feature a, Feature b) {
  return static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b);
}
constexpr std::uint32_t operator|(std::uint32_t a, Feature b) {
  return a | static_cast<std::uint32_t>(b);
}

constexpr std::uint32_t kFullPad =
    Feature::RightStick | Feature::ZButtons | Feature::AnalogTriggers | Feature::StickClicks;

// Labels in Button's order: FaceLeft, FaceRight, FaceTop, FaceBottom, L, R,
// ZL, ZR, Start, Select, Up, Down, Left, Right, LeftStickClick,
// RightStickClick
constexpr Profile kXbox{Kind::Xbox,
                        "Xbox",
                        kFullPad,
                        {"X", "B", "Y", "A", "LB", "RB", "LT", "RT", "Menu", "View", "Up", "Down",
                         "Left", "Right", "LS", "RS"}};
constexpr Profile kPlayStation{Kind::PlayStation,
                               "PlayStation",
                               kFullPad,
                               {"Square", "Circle", "Triangle", "Cross", "L1", "R1", "L2", "R2",
                                "Options", "Share", "Up", "Down", "Left", "Right", "L3", "R3"}};
constexpr Profile kSwitchPro{Kind::SwitchPro,
                             "Switch Pro",
                             Feature::RightStick | Feature::ZButtons | Feature::StickClicks,
                             {"Y", "A", "X", "B", "L", "R", "ZL", "ZR", "+", "-", "Up", "Down",
                              "Left", "Right", "L Stick", "R Stick"}};
constexpr Profile kDs{Kind::Ds,
                      "DS",
                      static_cast<std::uint32_t>(Feature::Touch),
                      {"Y", "A", "X", "B", "L", "R", "", "", "START", "SELECT", "Up", "Down",
                       "Left", "Right", "", ""}};
constexpr Profile kThreeDs{Kind::ThreeDs,
                           "3DS",
                           static_cast<std::uint32_t>(Feature::Touch),
                           {"Y", "A", "X", "B", "L", "R", "", "", "START", "SELECT", "Up", "Down",
                            "Left", "Right", "", ""}};
constexpr Profile kNewThreeDs{Kind::NewThreeDs,
                              "New 3DS",
                              Feature::Touch | Feature::RightStick | Feature::ZButtons,
                              {"Y", "A", "X", "B", "L", "R", "ZL", "ZR", "START", "SELECT", "Up",
                               "Down", "Left", "Right", "", ""}};
// (the keyboard's labels are the standard key map's keys)
constexpr Profile kKeyboard{Kind::Keyboard,
                            "Keyboard",
                            Feature::RightStick | Feature::ZButtons | Feature::StickClicks,
                            {"J", "L", "I", "K", "Q", "O", "E", "U", "Enter", "Tab", "T", "G", "F",
                             "H", "Left Shift", "Right Shift"}};
constexpr Profile kOther{Kind::Other, "Controller", kFullPad, kXbox.labels};
constexpr Profile kNone{Kind::None, "None", 0, kXbox.labels};

} // namespace

const Profile &profileOf(Kind kind) {
  switch (kind) {
  case Kind::Keyboard:
    return kKeyboard;
  case Kind::Xbox:
    return kXbox;
  case Kind::PlayStation:
    return kPlayStation;
  case Kind::SwitchPro:
    return kSwitchPro;
  case Kind::Ds:
    return kDs;
  case Kind::ThreeDs:
    return kThreeDs;
  case Kind::NewThreeDs:
    return kNewThreeDs;
  case Kind::Other:
    return kOther;
  case Kind::None:
    break;
  }
  return kNone;
}

const char *labelOf(Button button, Kind kind) {
  return profileOf(kind).labels[static_cast<std::size_t>(button)];
}

const char *Gamepad::labelOf(Button button) const { return Input::labelOf(button, kind); }

Edges::Edges(const Gamepad &before, const Gamepad &now) {
  for (std::size_t b = 0; b < kButtons; ++b) {
    // (unsigned 16-bit subtraction: right across the counts wrapping)
    presses_[b] = static_cast<std::uint16_t>(now.presses[b] - before.presses[b]);
    releases_[b] = static_cast<std::uint16_t>(now.releases[b] - before.releases[b]);
  }
}

std::uint32_t Edges::pressedBits() const {
  std::uint32_t bits = 0;
  for (std::size_t b = 0; b < kButtons; ++b)
    if (presses_[b])
      bits |= 1u << b;
  return bits;
}

} // namespace ScratchPadd::Input
