// The input library without a pad: profiles and labels, presses counted
// and compared, the dead zone, each platform's device state made into a
// Gamepad (made-up states), and the keyboard laid onto the pad.
#include <ScratchPadd/Input/Devices.hpp>
#include <ScratchPadd/Input/Reader.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string_view>

using namespace ScratchPadd::Input;

namespace {
constexpr int kEvKey = 1, kEvAbs = 3;

// What the test's pad source gives the Reader
Gamepad nextPad;
Gamepad testPad() { return nextPad; }

double lengthOf(Stick s) { return std::hypot(double(s.x), double(s.y)) / kStickFull; }
} // namespace

// The same button has the name on the pad in hand: the bottom face button
// is A on an Xbox pad, Cross on a PlayStation's, B on Nintendo's.
TEST(InputTest, LabelsFollowThePad) {
  EXPECT_STREQ(labelOf(Button::FaceBottom, Kind::Xbox), "A");
  EXPECT_STREQ(labelOf(Button::FaceBottom, Kind::PlayStation), "Cross");
  EXPECT_STREQ(labelOf(Button::FaceBottom, Kind::ThreeDs), "B");
  EXPECT_STREQ(labelOf(Button::FaceRight, Kind::Ds), "A");
  EXPECT_STREQ(labelOf(Button::FaceBottom, Kind::Keyboard), "K");
  EXPECT_STREQ(labelOf(Button::Start, Kind::SwitchPro), "+");
  EXPECT_STREQ(labelOf(Button::L, Kind::PlayStation), "L1");
  const Gamepad pad{.kind = Kind::PlayStation};
  EXPECT_STREQ(pad.labelOf(Button::FaceTop), "Triangle");
  // The keyboard's labels are the standard key map's keys
  const KeyMap standard = KeyMap::standard();
  for (std::size_t b = 0; b < kButtons; ++b)
    EXPECT_STREQ(labelOf(Button(b), Kind::Keyboard), standard.labelOf(Button(b))) << b;
  // What each has
  EXPECT_FALSE(profileOf(Kind::Ds).features & std::uint32_t(Feature::RightStick));
  EXPECT_TRUE(profileOf(Kind::NewThreeDs).features & std::uint32_t(Feature::RightStick));
  EXPECT_TRUE(profileOf(Kind::Xbox).features & std::uint32_t(Feature::AnalogTriggers));
}

// The Reader counts each press and release; a reader comparing its last
// copy with this one sees each once, whenever it reads, and two readers
// each see them all.
TEST(InputTest, PressesAreCountedForEveryReader) {
  Reader reader(testPad);
  nextPad = {.kind = Kind::Xbox};
  const Gamepad start = reader.read();
  Gamepad seenBySlowReader = start;

  nextPad.held = bitOf(Button::FaceBottom);
  const Gamepad first = reader.read();
  EXPECT_TRUE(Edges(start, first).pressed(Button::FaceBottom));
  nextPad.held = 0;
  const Gamepad second = reader.read();
  EXPECT_FALSE(Edges(first, second).pressed(Button::FaceBottom));
  EXPECT_TRUE(Edges(first, second).released(Button::FaceBottom));
  nextPad.held = bitOf(Button::FaceBottom) | bitOf(Button::Start);
  const Gamepad third = reader.read();

  // A reader that missed the reads in between still sees both presses
  const Edges slow(seenBySlowReader, third);
  EXPECT_EQ(slow.presses(Button::FaceBottom), 2u);
  EXPECT_EQ(slow.releases(Button::FaceBottom), 1u);
  EXPECT_TRUE(slow.pressed(Button::Start));
  EXPECT_EQ(slow.pressedBits(), bitOf(Button::FaceBottom) | bitOf(Button::Start));
  EXPECT_EQ(Edges(third, third).pressedBits(), 0u);

  // Across the counts wrapping round
  Gamepad before, after;
  before.presses[0] = 65535;
  after.presses[0] = 1;
  EXPECT_EQ(Edges(before, after).presses(Button(0)), 2u);
}

// Within the dead zone a stick is at rest; past it, it stretches to reach
// full again, keeping its direction.
TEST(InputTest, DeadZone) {
  EXPECT_EQ(lengthOf(withoutDeadZone(stickFrom(0.02f, -0.01f), 0.1f)), 0);
  EXPECT_NEAR(lengthOf(withoutDeadZone(stickFrom(1, 0), 0.1f)), 1.0, 1e-4);
  EXPECT_NEAR(lengthOf(withoutDeadZone(stickFrom(0.55f, 0), 0.1f)), 0.5, 1e-3);
  const Stick diagonal = withoutDeadZone(stickFrom(-0.4f, 0.4f), 0.1f);
  EXPECT_EQ(diagonal.x, -diagonal.y);
  EXPECT_NEAR(axis<double>(kStickFull), 1.0, 1e-9);
  EXPECT_NEAR(trigger<double>(65535), 1.0, 1e-9);
}

// The DS's and 3DS's keys, by position (Nintendo's A on the right); the
// DS's D-pad is its left stick too, the 3DS's circle pad its own.
TEST(InputTest, NintendoKeys) {
  using K = NintendoKeys;
  const Gamepad ds = fromNintendo(Kind::Ds, K::kA | K::kUp | K::kRight, 0, 0, 0, 0, {});
  EXPECT_TRUE(ds.isHeld(Button::FaceRight));
  EXPECT_FALSE(ds.isHeld(Button::FaceBottom));
  EXPECT_TRUE(ds.isHeld(Button::Up));
  EXPECT_NEAR(lengthOf(ds.left), 1.0, 1e-3);
  EXPECT_GT(ds.left.x, 0);
  EXPECT_GT(ds.left.y, 0);
  EXPECT_TRUE(ds.has(Feature::Touch));

  const Gamepad old = fromNintendo(Kind::ThreeDs, K::kB | K::kZl, 156, -78, 100, 0, {true, 10, 20});
  EXPECT_TRUE(old.isHeld(Button::FaceBottom));
  EXPECT_FALSE(old.isHeld(Button::ZL)); // (an old 3DS has no ZL)
  EXPECT_EQ(old.left.x, kStickFull);
  EXPECT_NEAR(axis<double>(old.left.y), -0.5, 1e-3);
  EXPECT_EQ(old.right.x, 0);
  EXPECT_TRUE(old.touch.down);
  EXPECT_EQ(old.touch.y, 20);

  const Gamepad n = fromNintendo(Kind::NewThreeDs, K::kZl, 0, 0, 0, 156, {});
  EXPECT_TRUE(n.isHeld(Button::ZL));
  EXPECT_EQ(n.leftTrigger, 65535);
  EXPECT_EQ(n.right.y, kStickFull);
}

// Apple's GameController: buttonA is the bottom one on every pad (Cross on
// a DualShock 4); the pad's kind from its product; a trigger counts as ZL
// or ZR once half pressed.
TEST(InputTest, GameController) {
  GameControllerState s{};
  s.productCategory = "DualShock 4";
  s.bottom = true;
  s.leftY = 1;
  s.rightX = -0.5f;
  s.leftTrigger = 0.64f;
  s.rightTrigger = 0.3f;
  s.hasOptions = true;
  s.options = true;
  const Gamepad pad = fromGameController(s);
  EXPECT_EQ(pad.kind, Kind::PlayStation);
  EXPECT_TRUE(pad.isHeld(Button::FaceBottom));
  EXPECT_STREQ(pad.labelOf(Button::FaceBottom), "Cross");
  EXPECT_TRUE(pad.isHeld(Button::Select));
  EXPECT_TRUE(pad.isHeld(Button::ZL));
  EXPECT_FALSE(pad.isHeld(Button::ZR));
  EXPECT_EQ(pad.left.y, kStickFull);
  EXPECT_NEAR(axis<double>(pad.right.x), -0.5, 1e-3);
  EXPECT_NEAR(trigger<double>(pad.leftTrigger), 0.64, 1e-3);
  EXPECT_EQ(kindOfProduct("Xbox Wireless Controller"), Kind::Xbox);
  EXPECT_EQ(kindOfProduct("DualSense"), Kind::PlayStation);
  EXPECT_EQ(kindOfProduct("Nintendo Switch Pro Controller"), Kind::SwitchPro);
  EXPECT_EQ(kindOfProduct("8BitDo"), Kind::Other);
}

// XInput's bits and numbers (an Xbox pad: A at the bottom)
TEST(InputTest, XInput) {
  const Gamepad pad = fromXInput({.buttons = 0x1000 | 0x0001 | 0x0100,
                                  .leftTrigger = 255,
                                  .rightTrigger = 10,
                                  .leftX = -32768,
                                  .leftY = 16384,
                                  .rightX = 0,
                                  .rightY = 32767});
  EXPECT_EQ(pad.kind, Kind::Xbox);
  EXPECT_TRUE(pad.isHeld(Button::FaceBottom));
  EXPECT_TRUE(pad.isHeld(Button::Up));
  EXPECT_TRUE(pad.isHeld(Button::L));
  EXPECT_TRUE(pad.isHeld(Button::ZL));
  EXPECT_FALSE(pad.isHeld(Button::ZR));
  EXPECT_EQ(pad.leftTrigger, 65535);
  EXPECT_EQ(pad.left.x, -kStickFull);
  EXPECT_EQ(pad.right.y, kStickFull);
}

// evdev: the kernel's buttons by position, sticks scaled from the device's
// ranges with y turned up, triggers, a hat D-pad, the kind by vendor
TEST(InputTest, Evdev) {
  EvdevPad sony(0x054c);
  EXPECT_EQ(sony.pad().kind, Kind::PlayStation);
  sony.setRange(0x00, {0, 255}); // ABS_X
  sony.setRange(0x01, {0, 255}); // ABS_Y
  sony.setRange(0x02, {0, 255}); // ABS_Z: the left trigger
  sony.apply(kEvKey, 0x130, 1);  // BTN_SOUTH
  sony.apply(kEvAbs, 0x00, 255);
  sony.apply(kEvAbs, 0x01, 0); // (pushed up: evdev's y grows downward)
  sony.apply(kEvAbs, 0x02, 255);
  sony.apply(kEvAbs, 0x11, -1); // ABS_HAT0Y: up
  EXPECT_TRUE(sony.pad().isHeld(Button::FaceBottom));
  EXPECT_EQ(sony.pad().left.x, kStickFull);
  EXPECT_EQ(sony.pad().left.y, kStickFull);
  EXPECT_EQ(sony.pad().leftTrigger, 65535);
  EXPECT_TRUE(sony.pad().isHeld(Button::Up));
  sony.apply(kEvKey, 0x130, 0);
  sony.apply(kEvAbs, 0x11, 0);
  EXPECT_FALSE(sony.pad().isHeld(Button::FaceBottom));
  EXPECT_FALSE(sony.pad().isHeld(Button::Up));
  EXPECT_EQ(EvdevPad(0x045e).pad().kind, Kind::Xbox);
  EXPECT_EQ(EvdevPad(0x1234).pad().kind, Kind::Other);
}

// The keyboard through the standard map: keys held as buttons, WASD the
// left stick (a diagonal as long as straight), Alt half strength, the mouse
// as the right stick, a pad's own stick first
TEST(InputTest, Keyboard) {
  Reader reader(testPad);
  nextPad = {};
  Keyboard keys;
  keys.open = true;
  keys.set(Key::K, true);
  keys.set(Key::W, true);
  keys.set(Key::D, true);
  keys.set(Key::E, true);
  Gamepad pad = reader.read(&keys);
  EXPECT_EQ(pad.kind, Kind::Keyboard);
  EXPECT_TRUE(pad.isHeld(Button::FaceBottom));
  EXPECT_TRUE(pad.isHeld(Button::ZL));
  EXPECT_EQ(pad.leftTrigger, 65535);
  EXPECT_NEAR(lengthOf(pad.left), 1.0, 1e-3);
  EXPECT_EQ(pad.left.x, pad.left.y);

  keys.set(Key::LeftAlt, true);
  keys.set(Key::W, false);
  pad = reader.read(&keys);
  EXPECT_NEAR(axis<double>(pad.left.x), 0.5, 1e-3);
  EXPECT_EQ(pad.left.y, 0);

  // A pad's stick, when pushed, comes first; its kind is the pad's
  nextPad = {.kind = Kind::Xbox, .left = {-20000, 0}}; // (past the dead zone)
  pad = reader.read(&keys);
  EXPECT_EQ(pad.kind, Kind::Xbox);
  EXPECT_LT(pad.left.x, 0);
  EXPECT_TRUE(pad.isHeld(Button::FaceBottom)); // the keys still count

  // The mouse as the right stick, by how far the cursor moved
  KeyMap map = KeyMap::standard();
  map.mouseRightStick = 0.01f;
  reader.setKeyMap(map);
  nextPad = {};
  keys = {};
  keys.open = true;
  keys.cursorX = 100;
  reader.read(&keys);
  keys.cursorX = 150; // 50 points right, 0.5 of the stick
  keys.cursorY = -20; // and up
  pad = reader.read(&keys);
  EXPECT_NEAR(axis<double>(pad.right.x), 0.5, 1e-3);
  EXPECT_NEAR(axis<double>(pad.right.y), 0.2, 1e-3);

  // No pad and no keyboard: nothing
  EXPECT_EQ(reader.read().kind, Kind::None);
  EXPECT_STREQ(nameOf(Key::LeftShift), "Left Shift");
}
