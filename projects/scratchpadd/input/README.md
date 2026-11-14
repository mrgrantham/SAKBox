# Input: the controller on every platform

One controller struct, `Input::Gamepad`, the same on a Mac, Windows, Linux, a 3DS and a DS; each platform's own API fills it in. `GamePadd` publishes it as [shared state](../README.md#shared-state); an app without padds reads it with an `Input::Reader`.

```cpp
#include <ScratchPadd/Input/GamePadd.hpp>   // //projects/scratchpadd/input:game_padd

// The system includes GamePadd; any padd reads the pad, once a frame
const auto [pad, version, published] = currentState<Input::Gamepad>();
const Input::Edges pressed(lastPad_, pad);   // what was pressed since the last copy
lastPad_ = pad;

if (pressed.pressed(Input::Button::FaceBottom)) jump();
const float steer = Input::axis<float>(pad.left.x);           // -1..1 (or a fixed-point type on the DS)
overlay.text(x, y, std::format("{} to jump", pad.labelOf(Input::Button::FaceBottom)));
//   "A" on an Xbox pad, "Cross" on a PlayStation pad, "B" on a 3DS, "K" on the keyboard
```

## The Gamepad

| Part | What it holds |
| --- | --- |
| `held` | A bit for each button down now (`isHeld(Button)`) |
| `presses`, `releases` | How many times each button has gone down and come up since reading began. A reader keeps its last copy and compares (`Edges`), so a press that came and went between its reads is still seen, and each reader sees every press once, however often it reads |
| `left`, `right` | The sticks, -32767 to 32767 each way (y up), the dead zone taken out; `axis<T>()` turns them into -1..1 |
| `leftTrigger`, `rightTrigger` | 0 to 65535 (`trigger<T>()` 0..1). A pad without analog triggers gives 0 or full for ZL and ZR; an analog one counts as ZL or ZR from halfway |
| `touch` | The touch screen while it's touched, in pixels (DS, 3DS) |
| `kind`, `features` | Which pad (`Kind`: Keyboard, Xbox, PlayStation, SwitchPro, Ds, ThreeDs, NewThreeDs, Other, or None), and what it has (`has(Feature::RightStick)`) |

- **Face buttons are named by where they are**: `FaceLeft`, `FaceRight`, `FaceTop`, `FaceBottom`. Nintendo's A is the right one and B the bottom one, Xbox's A the bottom one, PlayStation's Cross the bottom one; a game naming them by place gets the same physical button on every pad. `labelOf` gives the name to show, from the pad's `Profile` (`profileOf(kind)`: its name, features and labels), so a prompt follows the pad in hand.
- **A part a pad doesn't have reads as at rest**: a stick centred, a button up. `features` says what it has, for choosing controls once (the DS has no right stick: a game decides what it does without one), rather than every value being optional.
- **Numbers are integers**, as the DS has no floating-point hardware; `axis<T>()` and `trigger<T>()` make floats or the DS's fixed-point `Scalar`.
- **Several pads**: the one used most recently. **None**: the keyboard, if there's a window, or `Kind::None`.

## Each platform

| Platform | Reads | Notes |
| --- | --- | --- |
| Mac | Apple's GameController (`src/PlatformMac.mm`) | Pads are **found** only while the main thread's Cocoa run loop runs; once found, their values update by themselves, read from any thread (measured with a DualShock 4 in a command-line program: no run loop, no pad). So each read on the main thread runs the run loop for no time, which a window's GLFW does anyway. A DualShock 4 reads in full: every button, both sticks and the analog triggers. Its Share button (Select) never read as held at the same moment as Options, so don't build combinations on it on a Mac |
| Windows | XInput (`xinput1_4.dll`, part of Windows) | Xbox pads and the many that act as one. PlayStation and Switch pads aren't XInput: on Windows they need Steam or DS4Windows to appear as an Xbox pad. Asking an empty slot takes milliseconds, so empty slots are asked again only once a second |
| Linux | evdev (`/dev/input/event*`) | The kernel's drivers (xpad, hid-playstation, hid-nintendo) give pads the gamepad buttons by position (`BTN_SOUTH`...), which it reads; new devices are looked for once a second. Reading needs read access to the device: the `input` group (`sudo usermod -aG input $USER`, then log in again) |
| 3DS | libctru's `hid` | The buttons, circle pad, C-stick and ZL/ZR (a New 3DS's), the touch screen |
| DS | libnds' keys | The buttons and touch screen; the D-pad is the left stick too. Presses are counted by a thread of their own at every screen refresh (below) |

The sticks' dead zone is 10% by default (`Reader::setDeadZone`): a DualShock 4's sticks rest up to 2% off centre. Each platform's device state is made into a `Gamepad` by a plain function (`Devices.hpp`), tested with made-up states on any desktop.

## The keyboard

On desktops the keys belong to the window, which the drawing padd owns. It publishes them as shared state, `publishState(display.keyboard())` (`DisplayPadd` does, every frame), and `GamePadd` lays them onto the pad, so a game plays the same with keys or a pad. The standard `KeyMap` puts each key where its button is on a pad:

| Pad | Keys |
| --- | --- |
| Left stick | W A S D |
| Face buttons | I (top), J (left), L (right), K (bottom) |
| Right stick | Arrow keys |
| D-pad | T F G H |
| L / ZL | Q / E |
| R / ZR | O / U |
| Stick clicks | Left Shift / Right Shift |
| Start / Select | Enter / Tab |

A key pushes its stick all the way, and a diagonal as far as straight; Alt held makes them half strength (walking, fine steering). With `KeyMap::mouseRightStick` set, the mouse moves the right stick (for cameras). A pad's own stick, when pushed, comes first; the keys' buttons count with the pad's. An app with its own map gives it to the reader (`setKeyMap`), and shows its keys with `KeyMap::labelOf` (the `Keyboard` profile's labels are the standard map's).

## GamePadd

A padd on the main thread (where a Mac finds its pads) that reads the pad every 4 ms and publishes it, with the keyboard laid on if a window publishes one. The main thread is busy drawing until the screen's refresh, so in practice it reads once a frame, just before the next one starts. On a DS it reads every 16 ms, about once a frame: presses are counted there at every refresh anyway (below), and each read is the main thread's time, which on a DS comes out of the other padds' (the main thread outranks them). Readers on any thread call `currentState<Input::Gamepad>()`.

When reads come more than 50 ms apart (three frames: other work kept the main thread from reading; 100 ms on a DS, three frames at 30 a second, as an app drawing every other refresh reads), GamePadd logs a warning, the first at once and then at most once every 10 seconds: `Reads were up to 120 ms apart (asked for every 4 ms)`. Only its regular reads are timed: between its first read and its second, the main thread's other padds start, and a DS app's screens took a second, which it used to report as a late read. A button pressed and let go in such a gap would be missed where presses are counted from the reads. On a DS they aren't: a thread of its own, ahead of everything else in the app, wakes at every screen refresh (calico's `threadIrqWait`, as an interrupt handler mustn't lock or call the C library), samples the buttons and counts each press and release, and the Reader takes those counts (`readPlatformPresses`). A tap is seen even when the reads are late, as they were on a DS Lite while a ragdoll's physics kept the main thread waiting (before padds had priorities: see [Threads](../README.md#threads)).

## Handhelds

The 3DS and DS apps take `:sources` into their Makefile builds (`"//projects/scratchpadd/input:sources": "scratchpadd/input"`, with `scratchpadd/input/src` in `SOURCES` and `scratchpadd/input/include` in `INCLUDES`); each platform's file compiles to nothing on the others. The handheld control panels take the pad (`HandheldControls::panelInputFrom(before, now)`), as does the 3DS backend's debug panel (`Debug::Panel::handle(before, now)`). The ScratchPadd demos and the Cyclone ragdoll read it through `GamePadd`, the Blender example's apps through a `Reader`. Programs that stand alone and show the raw API (`examples/read-controls`, the 3DS benchmark, the audio demo and self-test) read the hardware themselves.

## Tools and tests

`bazel run //projects/scratchpadd/input:show_pad -- 60` prints the pad as it's read, each time it changes (its kind, the buttons by its own labels, the sticks and triggers, each press), for checking a platform's reader with a real pad; L, R and Start together stop it.

| Test | Covers |
| --- | --- |
| `Input_test` | Labels following the pad, presses counted for every reader (between reads, across the counts wrapping), the dead zone, and each platform's device state as a Gamepad: the DS's and 3DS's keys, GameController, XInput and evdev; the keyboard through the standard map (diagonals, half strength, the mouse, a pad's own stick first) |
| `GamePadd_test` | GamePadd in a running system publishing the pad, with a window padd's keyboard laid on |
