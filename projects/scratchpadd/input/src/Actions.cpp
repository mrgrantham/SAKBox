// Input actions: bindings as text, and reading a binding's value, whether it's
// held, and its prompt (see Actions.hpp).
#include <ScratchPadd/Input/Actions.hpp>

#include <cstdlib>

namespace ScratchPadd::Input {

namespace {

constexpr std::array<std::string_view, kButtons> kButtonNames = {"face_left",
                                                                 "face_right",
                                                                 "face_top",
                                                                 "face_bottom",
                                                                 "l",
                                                                 "r",
                                                                 "zl",
                                                                 "zr",
                                                                 "start",
                                                                 "select",
                                                                 "up",
                                                                 "down",
                                                                 "left",
                                                                 "right",
                                                                 "left_stick_click",
                                                                 "right_stick_click"};

std::vector<std::string_view> parts(std::string_view text) {
  std::vector<std::string_view> out;
  while (true) {
    const std::size_t colon = text.find(':');
    out.push_back(text.substr(0, colon));
    if (colon == std::string_view::npos)
      return out;
    text.remove_prefix(colon + 1);
  }
}

std::optional<Side> sideNamed(std::string_view name) {
  if (name == "left")
    return Side::Left;
  if (name == "right")
    return Side::Right;
  return std::nullopt;
}

std::int16_t stickValue(const Gamepad &pad, Side side, Direction direction) {
  const Stick &stick = side == Side::Left ? pad.left : pad.right;
  return direction == Direction::X ? stick.x : stick.y;
}

} // namespace

std::string_view buttonName(Button button) {
  return kButtonNames[static_cast<std::size_t>(button)];
}

std::optional<Button> buttonNamed(std::string_view name) {
  for (std::size_t i = 0; i < kButtons; ++i) {
    if (kButtonNames[i] == name)
      return static_cast<Button>(i);
  }
  return std::nullopt;
}

std::string bindingText(const Binding &binding) {
  const auto side = [](Side s) { return std::string(s == Side::Left ? "left" : "right"); };
  switch (binding.source) {
  case Binding::Source::Button:
    return "button:" + std::string(buttonName(binding.button));
  case Binding::Source::Pair:
    return "pair:" + std::string(buttonName(binding.negative)) + ":" +
           std::string(buttonName(binding.positive));
  case Binding::Source::Stick:
    return "stick:" + side(binding.side) + ":" + (binding.direction == Direction::X ? "x" : "y") +
           (binding.inverted ? ":inverted" : "");
  case Binding::Source::Trigger:
    return "trigger:" + side(binding.side);
  case Binding::Source::None:
    break;
  }
  return "none";
}

std::optional<Binding> bindingFromText(std::string_view text) {
  const std::vector<std::string_view> p = parts(text);
  if (p[0] == "button" && p.size() == 2) {
    if (const auto button = buttonNamed(p[1]))
      return Binding::of(*button);
  } else if (p[0] == "pair" && p.size() == 3) {
    const auto negative = buttonNamed(p[1]), positive = buttonNamed(p[2]);
    if (negative && positive)
      return Binding::pair(*negative, *positive);
  } else if (p[0] == "stick" && (p.size() == 3 || (p.size() == 4 && p[3] == "inverted"))) {
    const auto side = sideNamed(p[1]);
    if (side && (p[2] == "x" || p[2] == "y"))
      return Binding::stick(*side, p[2] == "x" ? Direction::X : Direction::Y, p.size() == 4);
  } else if (p[0] == "trigger" && p.size() == 2) {
    if (const auto side = sideNamed(p[1]))
      return Binding::trigger(*side);
  }
  return std::nullopt;
}

float valueOf(const Binding &binding, const Gamepad &pad) {
  switch (binding.source) {
  case Binding::Source::Button:
    return pad.isHeld(binding.button) ? 1.0f : 0.0f;
  case Binding::Source::Pair:
    return (pad.isHeld(binding.positive) ? 1.0f : 0.0f) -
           (pad.isHeld(binding.negative) ? 1.0f : 0.0f);
  case Binding::Source::Stick: {
    const float value = axis<float>(stickValue(pad, binding.side, binding.direction));
    return binding.inverted ? -value : value;
  }
  case Binding::Source::Trigger:
    return trigger<float>(binding.side == Side::Left ? pad.leftTrigger : pad.rightTrigger);
  case Binding::Source::None:
    break;
  }
  return 0;
}

bool heldOf(const Binding &binding, const Gamepad &pad) {
  switch (binding.source) {
  case Binding::Source::Button:
    return pad.isHeld(binding.button);
  case Binding::Source::Pair:
    return pad.isHeld(binding.negative) || pad.isHeld(binding.positive);
  case Binding::Source::Stick:
  case Binding::Source::Trigger:
    return valueOf(binding, pad) >= 0.5f;
  case Binding::Source::None:
    break;
  }
  return false;
}

std::string promptOf(const Binding &binding, const Gamepad &pad) {
  switch (binding.source) {
  case Binding::Source::Button:
    return pad.labelOf(binding.button);
  case Binding::Source::Pair:
    return std::string(pad.labelOf(binding.negative)) + "/" + pad.labelOf(binding.positive);
  case Binding::Source::Stick:
    return binding.side == Side::Left ? "Left stick" : "Right stick";
  case Binding::Source::Trigger:
    return pad.labelOf(binding.side == Side::Left ? Button::ZL : Button::ZR);
  case Binding::Source::None:
    break;
  }
  return "-";
}

} // namespace ScratchPadd::Input
