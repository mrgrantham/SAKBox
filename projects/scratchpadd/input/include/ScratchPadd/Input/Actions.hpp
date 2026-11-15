// Actions.hpp - a game's own controls ("steer", "boost", "confirm"), bound to
// each kind of pad's buttons and sticks, so the game asks about its actions
// rather than buttons, players can remap them, and prompts name the button on
// the pad in hand.
//
// A game lists its actions as an enum (0, 1, 2, ... in order), with a name
// for each (shown and saved) and whether it's a button or an axis:
//
//   enum class Ship { Steer, Climb, Boost, Confirm };
//   inline constexpr Input::ActionSet<Ship, 4> kShipActions{{{
//       {Ship::Steer, "steer", Input::ActionKind::Axis},
//       {Ship::Climb, "climb", Input::ActionKind::Axis},
//       {Ship::Boost, "boost", Input::ActionKind::Button},
//       {Ship::Confirm, "confirm", Input::ActionKind::Button},
//   }}};
//
// and its default bindings for each kind of pad, up to two each:
//
//   Input::Bindings<4> shipDefaults(Input::Kind kind) {
//     Input::Bindings<4> b;
//     b.bind(Ship::Steer, Input::Binding::stick(Input::Side::Left, Input::Direction::X),
//            Input::Binding::pair(Input::Button::Left, Input::Button::Right));  // the DS's D-pad
//     ...
//   }
//
// Then, once a frame:
//
//   Input::ActionReader<Ship, 4> controls(kShipActions, shipDefaults);
//   controls.update(pad);                       // the Gamepad (GamePadd's shared state)
//   float steer = controls.value<float>(Ship::Steer);    // -1..1
//   if (controls.pressed(Ship::Boost)) boost();
//   text("{} to confirm", controls.prompt(Ship::Confirm));   // "A", "Cross", "K"
//
// Remapped bindings (per kind of pad) are written and read as text, one line
// an action ("steer stick:left:x pair:left:right"), for the app's settings.
#pragma once

#include <ScratchPadd/Input/Gamepad.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ScratchPadd::Input {

enum class ActionKind : std::uint8_t {
  Button, // pressed, held, released
  Axis,   // a value from -1 to 1
};
enum class Side : std::uint8_t { Left, Right };
enum class Direction : std::uint8_t { X, Y };

// One control on a pad that drives an action
struct Binding {
  enum class Source : std::uint8_t {
    None,    // (an empty slot)
    Button,  // a button: an axis reads 1 while it's held
    Pair,    // two buttons as an axis: -1 for `negative`, 1 for `positive`
    Stick,   // one direction of a stick (up and right are positive)
    Trigger, // 0 to 1; as a button, held from halfway
  };
  Source source = Source::None;
  Input::Button button = Input::Button::FaceBottom, negative = Input::Button::Left,
                positive = Input::Button::Right;
  Side side = Side::Left;
  Direction direction = Direction::X;
  bool inverted = false; // a stick's direction turned round

  static constexpr Binding of(Input::Button b) { return {.source = Source::Button, .button = b}; }
  static constexpr Binding pair(Input::Button negativeButton, Input::Button positiveButton) {
    return {.source = Source::Pair, .negative = negativeButton, .positive = positiveButton};
  }
  static constexpr Binding stick(Side s, Direction d, bool invert = false) {
    return {.source = Source::Stick, .side = s, .direction = d, .inverted = invert};
  }
  static constexpr Binding trigger(Side s) { return {.source = Source::Trigger, .side = s}; }

  bool empty() const { return source == Source::None; }
  bool operator==(const Binding &) const = default;
};
inline constexpr std::size_t kBindingsPerAction = 2;

// The game's list of actions, in the enum's order
template <typename Action> struct ActionInfo {
  Action action;
  std::string_view name; // shown and saved: lowercase, no spaces
  ActionKind kind;
};
template <typename Action, std::size_t N> struct ActionSet {
  std::array<ActionInfo<Action>, N> actions;

  static constexpr std::size_t indexOf(Action action) { return static_cast<std::size_t>(action); }
  constexpr std::string_view name(Action action) const { return actions[indexOf(action)].name; }
  constexpr ActionKind kind(Action action) const { return actions[indexOf(action)].kind; }
  constexpr std::optional<Action> named(std::string_view name) const {
    for (const ActionInfo<Action> &info : actions) {
      if (info.name == name)
        return info.action;
    }
    return std::nullopt;
  }
  // Whether the table is in the enum's order (check with a static_assert)
  constexpr bool inOrder() const {
    for (std::size_t i = 0; i < N; ++i) {
      if (indexOf(actions[i].action) != i)
        return false;
    }
    return true;
  }
};

// What each action is bound to, for one kind of pad
template <std::size_t N> struct Bindings {
  std::array<std::array<Binding, kBindingsPerAction>, N> slots{};

  template <typename Action> void bind(Action action, Binding first, Binding second = {}) {
    slots[static_cast<std::size_t>(action)] = {first, second};
  }
  template <typename Action>
  const std::array<Binding, kBindingsPerAction> &of(Action action) const {
    return slots[static_cast<std::size_t>(action)];
  }
  bool operator==(const Bindings &) const = default;
};

// ---------------------------------------------------------------------------
// Bindings as text: "stick:left:x", "pair:left:right", "button:face_bottom",
// "trigger:right", "stick:left:y:inverted"

std::string_view buttonName(Button button);
std::optional<Button> buttonNamed(std::string_view name);
std::string bindingText(const Binding &binding);
std::optional<Binding> bindingFromText(std::string_view text);

// One line an action: its name, then its bindings
template <typename Action, std::size_t N>
std::string bindingsText(const ActionSet<Action, N> &set, const Bindings<N> &bindings) {
  std::string out;
  for (const ActionInfo<Action> &info : set.actions) {
    out += info.name;
    for (const Binding &binding : bindings.of(info.action)) {
      if (!binding.empty())
        (out += ' ') += bindingText(binding);
    }
    out += '\n';
  }
  return out;
}

// Bindings read from text, over `start` (the defaults): an action named in
// the text gets the text's bindings; an action not named keeps its default,
// and a line naming an action the game no longer has is skipped, so saved
// settings survive the game gaining or renaming actions.
template <typename Action, std::size_t N>
Bindings<N> bindingsFromText(const ActionSet<Action, N> &set, std::string_view text,
                             Bindings<N> start) {
  while (!text.empty()) {
    const std::size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    const std::size_t space = line.find(' ');
    const auto action = set.named(line.substr(0, space));
    if (!action)
      continue;
    std::array<Binding, kBindingsPerAction> slots{};
    std::size_t next = 0;
    std::string_view rest =
        space == std::string_view::npos ? std::string_view{} : line.substr(space + 1);
    while (!rest.empty() && next < kBindingsPerAction) {
      const std::size_t gap = rest.find(' ');
      if (const auto binding = bindingFromText(rest.substr(0, gap)))
        slots[next++] = *binding;
      rest.remove_prefix(gap == std::string_view::npos ? rest.size() : gap + 1);
    }
    start.slots[static_cast<std::size_t>(*action)] = slots;
  }
  return start;
}

// ---------------------------------------------------------------------------
// Reading the actions

// A binding's value from -1 to 1 (0 to 1 for a button or trigger)
float valueOf(const Binding &binding, const Gamepad &pad);
// Whether a binding counts as held: a button down, a stick or trigger past
// halfway (a pair: either button)
bool heldOf(const Binding &binding, const Gamepad &pad);
// What to show for a binding on this pad: "A", "Left/Right", "Left stick"
std::string promptOf(const Binding &binding, const Gamepad &pad);

template <typename Action, std::size_t N> class ActionReader {
public:
  using Defaults = std::function<Bindings<N>(Kind)>;

  ActionReader(const ActionSet<Action, N> &set, Defaults defaults)
      : set_(set), defaults_(std::move(defaults)) {}

  // A remapping for one kind of pad (the player's), instead of its defaults
  // (takes effect at once for the pad in hand)
  void setBindings(Kind kind, const Bindings<N> &bindings) {
    overrides_[kind] = bindings;
    if (started_ && current_.second == kind)
      current_.first = bindings;
  }
  void resetBindings(Kind kind) {
    overrides_.erase(kind);
    if (started_ && current_.second == kind)
      current_.first = defaults_(kind);
  }
  // What each action is bound to on a kind of pad: the player's, or the defaults
  Bindings<N> bindingsFor(Kind kind) const {
    const auto found = overrides_.find(kind);
    return found != overrides_.end() ? found->second : defaults_(kind);
  }
  const std::map<Kind, Bindings<N>> &remapped() const { return overrides_; }

  // Once a frame, with the pad as it is now
  void update(const Gamepad &pad) {
    last_ = now_;
    now_ = pad;
    if (pad.kind != current_.second || !started_) {
      current_ = {bindingsFor(pad.kind), pad.kind};
      started_ = true;
    }
  }

  bool held(Action action) const {
    for (const Binding &binding : current_.first.of(action)) {
      if (!binding.empty() && heldOf(binding, now_))
        return true;
    }
    return false;
  }
  // Pressed since the last update: a button's press (even one that came and
  // went between updates), or a stick or trigger crossing halfway
  bool pressed(Action action) const { return changed(action, true); }
  bool released(Action action) const { return changed(action, false); }

  // An axis action's value, -1 to 1: its bindings added together (the stick
  // and the D-pad both work), kept within -1..1
  template <typename Number> Number value(Action action) const {
    float sum = 0;
    for (const Binding &binding : current_.first.of(action)) {
      if (!binding.empty())
        sum += valueOf(binding, now_);
    }
    return Number(sum < -1 ? -1.0f : (sum > 1 ? 1.0f : sum));
  }

  // What to press, on the pad in hand: its first binding's name ("A", "K",
  // "Left stick")
  std::string prompt(Action action) const {
    const Binding &first = current_.first.of(action)[0];
    return first.empty() ? std::string("-") : promptOf(first, now_);
  }

  const ActionSet<Action, N> &actions() const { return set_; }

private:
  bool changed(Action action, bool down) const {
    const Edges edges(last_, now_);
    for (const Binding &binding : current_.first.of(action)) {
      if (binding.empty())
        continue;
      if (binding.source == Binding::Source::Button) {
        if (down ? edges.pressed(binding.button) : edges.released(binding.button))
          return true;
      } else if (heldOf(binding, last_) != heldOf(binding, now_) && heldOf(binding, now_) == down) {
        return true;
      }
    }
    return false;
  }

  const ActionSet<Action, N> &set_;
  Defaults defaults_;
  std::map<Kind, Bindings<N>> overrides_;
  Gamepad last_, now_;
  std::pair<Bindings<N>, Kind> current_{};
  bool started_ = false;
};

} // namespace ScratchPadd::Input
