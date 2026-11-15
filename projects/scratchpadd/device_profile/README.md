# Device profile

What this device is, what it can do, and how an app should run and be played on it, in one place instead of an `#if` in each app.

```cpp
#include <ScratchPadd/DeviceProfile/DeviceProfile.hpp>       // //projects/scratchpadd/device_profile

const ScratchPadd::DeviceProfile profile = ScratchPadd::currentProfile();   // anywhere, any time
world.setSubSteps(profile.physics.subSteps);
if (!profile.device.fpu) useFixedPoint();
```

## The profile

`DeviceProfile` (`DeviceProfile.hpp`) holds plain values, so it can be [shared state](../README.md#shared-state):

| Section | What |
| --- | --- |
| `name`, `platform` | "DS Lite", `DevicePlatform::Ds` |
| `device` | Processor speed (MHz), cores the app's padds can use, whether it has floating point in hardware, memory (MB); 0 means plenty (desktops) |
| `screens` | How many, their sizes, whether the lower one is touch, whether the main one can show 3D |
| `scene` | The most polygons and vertices a frame (0: no limit), frames a second, toon outlines, fog |
| `physics` | Steps a second and sub-steps |
| `textures` | The texture budget (KB) and colours (16 or 256 for paletted, 0 for full colour) |
| `audio` | Voices at once, and whether the hardware mixes them (the DS's channels) |
| `controls` | The kinds of controller it has or takes, its own first; each kind's [action map](../input/README.md#actions) comes from the game's bindings |

Each platform has a built-in profile (`DeviceProfile::forPlatform`), from the roadmap's profile table:

| | DS | 3DS | Mac, Windows, Linux |
| --- | --- | --- | --- |
| Device | 67 MHz, no FPU, 4 MB | 268 MHz, 2 cores, FPU, 128 MB | plenty |
| Screens | 2 × 256×192, touch below | 400×240 stereo, 320×240 touch | a window |
| Scene | 2,048 polygons and 6,144 vertices a frame, 30 fps, no outlines | no fixed limit, 60 fps, outlines | no limit, 60 fps |
| Physics | 15 steps a second × 4 | 60 × 4 | 60 × 4 |
| Textures | 64 KB, 16 colours | 1 MB, full colour | no limit |
| Audio | 16 hardware channels | 24 voices | 32 voices |
| Controllers | the DS's own | 3DS, New 3DS | keyboard, Xbox, PlayStation, Switch Pro |

A 3DS profile is the old 3DS's, which a New 3DS runs comfortably. Telling a New 3DS from an old one, and a DSi from a DS, is to come: a tier within the platform.

## For this run

`currentProfile()` is this device's profile for the run, for any code at any time, before the padds start too: the built-in one with the adjustments saved in the app's files (`device_profile.txt`), read once. It's thread-safe. `setCurrentProfile()` changes it for the rest of the run.

## `DeviceProfilePadd`

Listed in an app's system (`:device_profile_padd`), it publishes the profile as shared state when the system starts and again on each change, for padds that adjust while running:

```cpp
const DeviceProfile profile = currentState<DeviceProfile>().value;
```

In a debug build it has a **Profile** tab:
- the device described ("DS Lite (Nintendo DS), 67 MHz, no FPU, 4 MB");
- the adjustable values: the scene's polygons, vertices and frames a second, outlines and fog, physics steps and sub-steps, the texture budget and colours, audio voices;
- "Reset to the built-in profile".

A change is saved to `device_profile.txt` (by StoragePadd when the app has one, when nothing else has work; otherwise at once), made current, and published. So a device can be tuned on the device and keep it. The whole profile is written, so a later build's different defaults don't change a device someone tuned. Reading it back skips lines it doesn't know, so the file survives the profile gaining or losing a value. A release build has no tab; its profile is the built-in one with whatever was saved.

## Still to do

- **Apps reading it** in place of their own `#if`s: the DS ragdoll's 15 steps × 4 sub-steps, GamePadd's 16 ms reads on a DS, the log's batch size.
- **Tiers within a platform:** New 3DS or old, DSi or DS.
- **A sweep that measures a device** and saves the result as its profile, as the Cyclone ragdoll's sweep does for physics.

## Tests

| Test | What it checks |
| --- | --- |
| `DeviceProfile_test` | Plain values (trivially copyable); the DS's, 3DS's and desktops' built-in profiles; the adjustable values as text and back; saved text surviving unknown lines and bad values; the current profile |
| `DeviceProfilePadd_test` | In a running system: the profile published when it starts; a value changed on the tab (as a panel sends it) published, made current and saved; a reset back to the built-in profile. Also ran 5 times under ThreadSanitizer |
