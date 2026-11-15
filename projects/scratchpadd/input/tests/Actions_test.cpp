// Input actions: a game's actions bound per kind of pad; buttons pressed,
// held and released; axes from sticks, button pairs and triggers, added
// together; prompts naming the pad's own buttons; remapping, at once; and
// bindings saved and read back as text.
#include <ScratchPadd/Input/Actions.hpp>

#include <gtest/gtest.h>

using namespace ScratchPadd::Input;

namespace {

enum class Ship { Steer, Climb, Boost, Confirm };
constexpr ActionSet<Ship, 4> kShip{{{
    {Ship::Steer, "steer", ActionKind::Axis},
    {Ship::Climb, "climb", ActionKind::Axis},
    {Ship::Boost, "boost", ActionKind::Button},
    {Ship::Confirm, "confirm", ActionKind::Button},
}}};
static_assert(kShip.inOrder());

// A pad with sticks steers with the left stick (and the D-pad); a DS steers
// with its D-pad alone. Boost is the right trigger, or R on a DS.
Bindings<4> shipDefaults(Kind kind) {
  Bindings<4> b;
  if (kind == Kind::Ds) {
    b.bind(Ship::Steer, Binding::pair(Button::Left, Button::Right));
    b.bind(Ship::Climb, Binding::pair(Button::Down, Button::Up));
    b.bind(Ship::Boost, Binding::of(Button::R));
  } else {
    b.bind(Ship::Steer, Binding::stick(Side::Left, Direction::X),
           Binding::pair(Button::Left, Button::Right));
    b.bind(Ship::Climb, Binding::stick(Side::Left, Direction::Y));
    b.bind(Ship::Boost, Binding::trigger(Side::Right), Binding::of(Button::R));
  }
  b.bind(Ship::Confirm, Binding::of(Button::FaceBottom));
  return b;
}

Gamepad padOf(Kind kind) {
  Gamepad pad;
  pad.kind = kind;
  return pad;
}

// A button going down since the last read
void press(Gamepad &pad, Button button) {
  pad.held |= bitOf(button);
  ++pad.presses[static_cast<std::size_t>(button)];
}
void lift(Gamepad &pad, Button button) {
  pad.held &= ~bitOf(button);
  ++pad.releases[static_cast<std::size_t>(button)];
}

} // namespace

TEST(ActionsTest, TheSetNamesItsActions) {
  EXPECT_EQ(kShip.name(Ship::Boost), "boost");
  EXPECT_EQ(kShip.kind(Ship::Steer), ActionKind::Axis);
  EXPECT_EQ(kShip.named("confirm"), Ship::Confirm);
  EXPECT_FALSE(kShip.named("jump").has_value());
}

TEST(ActionsTest, ButtonsArePressedHeldAndReleased) {
  ActionReader<Ship, 4> controls(kShip, shipDefaults);
  Gamepad pad = padOf(Kind::Xbox);
  controls.update(pad);
  EXPECT_FALSE(controls.held(Ship::Confirm));
  press(pad, Button::FaceBottom);
  controls.update(pad);
  EXPECT_TRUE(controls.pressed(Ship::Confirm));
  EXPECT_TRUE(controls.held(Ship::Confirm));
  controls.update(pad); // still held, no new press
  EXPECT_FALSE(controls.pressed(Ship::Confirm));
  EXPECT_TRUE(controls.held(Ship::Confirm));
  lift(pad, Button::FaceBottom);
  controls.update(pad);
  EXPECT_TRUE(controls.released(Ship::Confirm));
  EXPECT_FALSE(controls.held(Ship::Confirm));
}

// A press that came and went between two reads still counts
TEST(ActionsTest, AQuickTapIsntMissed) {
  ActionReader<Ship, 4> controls(kShip, shipDefaults);
  Gamepad pad = padOf(Kind::Xbox);
  controls.update(pad);
  press(pad, Button::FaceBottom);
  lift(pad, Button::FaceBottom);
  controls.update(pad);
  EXPECT_TRUE(controls.pressed(Ship::Confirm));
  EXPECT_FALSE(controls.held(Ship::Confirm));
}

// An axis from the stick, the D-pad, or both added together, within -1..1
TEST(ActionsTest, AxesFromSticksAndButtonPairs) {
  ActionReader<Ship, 4> controls(kShip, shipDefaults);
  Gamepad pad = padOf(Kind::Xbox);
  pad.left.x = kStickFull / 2;
  controls.update(pad);
  EXPECT_NEAR(controls.value<float>(Ship::Steer), 0.5f, 0.01f);
  pad.held |= bitOf(Button::Right); // and the D-pad too: full right, no further
  controls.update(pad);
  EXPECT_EQ(controls.value<float>(Ship::Steer), 1.0f);
  pad.left.x = 0;
  pad.held = bitOf(Button::Left);
  controls.update(pad);
  EXPECT_EQ(controls.value<float>(Ship::Steer), -1.0f);
  pad.held = 0;
  pad.left.y = -kStickFull; // down
  controls.update(pad);
  EXPECT_EQ(controls.value<float>(Ship::Climb), -1.0f);
}

// A DS steers with its D-pad: the same actions, its own bindings
TEST(ActionsTest, EachKindOfPadHasItsOwnBindings) {
  ActionReader<Ship, 4> controls(kShip, shipDefaults);
  Gamepad ds = padOf(Kind::Ds);
  ds.held = bitOf(Button::Right) | bitOf(Button::Up);
  controls.update(ds);
  EXPECT_EQ(controls.value<float>(Ship::Steer), 1.0f);
  EXPECT_EQ(controls.value<float>(Ship::Climb), 1.0f);
  press(ds, Button::R);
  controls.update(ds);
  EXPECT_TRUE(controls.pressed(Ship::Boost));
}

// A trigger or stick counts as a button from halfway
TEST(ActionsTest, TriggersAndSticksAsButtons) {
  ActionReader<Ship, 4> controls(kShip, shipDefaults);
  Gamepad pad = padOf(Kind::PlayStation);
  pad.rightTrigger = 20000;
  controls.update(pad);
  EXPECT_FALSE(controls.held(Ship::Boost));
  pad.rightTrigger = 50000;
  controls.update(pad);
  EXPECT_TRUE(controls.pressed(Ship::Boost));
  EXPECT_TRUE(controls.held(Ship::Boost));
  pad.rightTrigger = 0;
  controls.update(pad);
  EXPECT_TRUE(controls.released(Ship::Boost));
}

// Prompts name the button on the pad in hand
TEST(ActionsTest, PromptsFollowThePad) {
  ActionReader<Ship, 4> controls(kShip, shipDefaults);
  for (Kind kind : {Kind::Xbox, Kind::PlayStation, Kind::ThreeDs}) {
    const Gamepad pad = padOf(kind);
    controls.update(pad);
    EXPECT_EQ(controls.prompt(Ship::Confirm), pad.labelOf(Button::FaceBottom));
  }
  controls.update(padOf(Kind::Xbox));
  EXPECT_EQ(controls.prompt(Ship::Steer), "Left stick");
  const Gamepad ds = padOf(Kind::Ds);
  controls.update(ds);
  EXPECT_EQ(controls.prompt(Ship::Steer),
            std::string(ds.labelOf(Button::Left)) + "/" + ds.labelOf(Button::Right));
}

// A remap takes effect at once for that kind of pad, and only that kind
TEST(ActionsTest, Remapping) {
  ActionReader<Ship, 4> controls(kShip, shipDefaults);
  Gamepad pad = padOf(Kind::Xbox);
  controls.update(pad);
  Bindings<4> mine = controls.bindingsFor(Kind::Xbox);
  mine.bind(Ship::Confirm, Binding::of(Button::FaceRight));
  controls.setBindings(Kind::Xbox, mine);
  press(pad, Button::FaceRight);
  controls.update(pad);
  EXPECT_TRUE(controls.pressed(Ship::Confirm));
  EXPECT_EQ(controls.bindingsFor(Kind::PlayStation), shipDefaults(Kind::PlayStation));
  controls.resetBindings(Kind::Xbox);
  EXPECT_EQ(controls.bindingsFor(Kind::Xbox), shipDefaults(Kind::Xbox));
}

TEST(ActionsTest, BindingsAsText) {
  for (const Binding &binding :
       {Binding::of(Button::FaceBottom), Binding::pair(Button::Left, Button::Right),
        Binding::stick(Side::Right, Direction::Y, true), Binding::trigger(Side::Left)}) {
    EXPECT_EQ(bindingFromText(bindingText(binding)), binding) << bindingText(binding);
  }
  EXPECT_EQ(bindingText(Binding::stick(Side::Left, Direction::X)), "stick:left:x");
  EXPECT_FALSE(bindingFromText("stick:middle:x").has_value());
  EXPECT_FALSE(bindingFromText("button:nothing").has_value());

  const Bindings<4> defaults = shipDefaults(Kind::Xbox);
  const std::string text = bindingsText(kShip, defaults);
  EXPECT_EQ(text.substr(0, text.find('\n')), "steer stick:left:x pair:left:right");
  EXPECT_EQ(bindingsFromText(kShip, text, Bindings<4>{}), defaults);
}

// Saved settings survive the game changing: an action not in the text keeps
// its default, a line for an action that's gone is skipped
TEST(ActionsTest, SavedBindingsSurviveChanges) {
  const Bindings<4> defaults = shipDefaults(Kind::Xbox);
  const Bindings<4> read =
      bindingsFromText(kShip, "confirm button:face_right\nwarp button:l\n", defaults);
  EXPECT_EQ(read.of(Ship::Confirm)[0], Binding::of(Button::FaceRight));
  EXPECT_EQ(read.of(Ship::Steer), defaults.of(Ship::Steer));
}
