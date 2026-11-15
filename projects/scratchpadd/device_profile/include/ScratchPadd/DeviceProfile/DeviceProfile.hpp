// DeviceProfile.hpp - what this device is, what it can do, and how an app
// should run and be played on it, in one place instead of an #if in each
// app: its processor and memory, its screens, how much the scene may draw,
// how often physics steps, the texture and audio budgets, and the
// controllers it supports.
//
// Each platform has a built-in profile (forPlatform()); DeviceProfilePadd
// publishes the one for this device as shared state, with the adjustments
// saved from its Profile tab (a debug build's), and every padd reads its
// section:
//
//   const DeviceProfile profile = currentState<DeviceProfile>().value;
//   world.setSubSteps(profile.physics.subSteps);
//
// Telling a New 3DS from an old one, or a DSi from a DS, comes later (a tier
// within the platform); for now each platform has one profile, the older,
// slower device's, which the newer one runs comfortably.
//
// Plain values only, so it can be shared state (trivially copyable).
#pragma once

#include <ScratchPadd/Input/Gamepad.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ScratchPadd {

enum class DevicePlatform : std::uint8_t { Ds, ThreeDs, Mac, Windows, Linux };

struct DeviceProfile {
  // A name to show ("DS Lite", "Mac")
  std::array<char, 32> name{};
  DevicePlatform platform = DevicePlatform::Mac;

  struct Device {
    int cpuMHz = 0;   // 0: fast enough not to matter (desktops)
    int cores = 1;    // that the app's padds can use
    bool fpu = true;  // floating point in hardware (the DS has none)
    int memoryMB = 0; // 0: plenty (desktops)
    bool operator==(const Device &) const = default;
  } device;

  struct Screens {
    int count = 1;
    int width = 0, height = 0;             // the main (top) screen; 0: a window, any size
    int secondWidth = 0, secondHeight = 0; // the lower screen, if any
    bool touch = false;                    // the lower screen is touch
    bool stereo = false;                   // the main screen can show 3D
    bool operator==(const Screens &) const = default;
  } screens;

  struct Scene {
    int maxPolygons = 0; // a frame (0: no limit)
    int maxVertices = 0; // a frame (0: no limit)
    int framesPerSecond = 60;
    bool outlines = true; // toon outlines on key objects
    bool fog = true;
    bool operator==(const Scene &) const = default;
  } scene;

  struct Physics {
    int stepsPerSecond = 60;
    int subSteps = 4;
    bool operator==(const Physics &) const = default;
  } physics;

  struct Textures {
    int budgetKB = 0; // 0: no limit
    int colours = 0;  // 0: full colour; 16 or 256 for paletted
    bool operator==(const Textures &) const = default;
  } textures;

  struct Audio {
    int voices = 32;               // at once
    bool hardwareChannels = false; // mixed by the hardware (DS) rather than in software
    bool operator==(const Audio &) const = default;
  } audio;

  struct Controls {
    // The kinds of controller this device has or takes; the first is its own
    std::array<Input::Kind, 4> kinds{Input::Kind::Keyboard, Input::Kind::Xbox,
                                     Input::Kind::PlayStation, Input::Kind::SwitchPro};
    Input::Kind defaultKind() const { return kinds[0]; }
    bool operator==(const Controls &) const = default;
  } controls;

  std::string_view nameView() const { return std::string_view(name.data()); }
  void setName(std::string_view text);
  bool operator==(const DeviceProfile &) const = default;

  // The built-in profile for a platform, and for the one this was built for
  static DeviceProfile forPlatform(DevicePlatform platform);
  static DevicePlatform thisPlatform();
  static DeviceProfile forThisDevice() { return forPlatform(thisPlatform()); }
};

// A profile's adjustable values as text, one "section.key value" a line
// ("physics.subSteps 4"), for the saved adjustments and the log
std::string profileText(const DeviceProfile &profile);
// `start` with the values the text gives (unknown lines skipped, so saved
// adjustments survive the profile gaining or losing a value)
DeviceProfile profileFromText(std::string_view text, DeviceProfile start);

// The adjustable values, for the Profile tab and the text: each one's name
// ("physics.subSteps") and where it is in `profile`
std::vector<std::pair<const char *, int *>> adjustableInts(DeviceProfile &profile);
std::vector<std::pair<const char *, bool *>> adjustableFlags(DeviceProfile &profile);

std::string_view platformName(DevicePlatform platform);

// This device's profile, for any code at any time (padds or not, before the
// padds start too): the built-in profile with the adjustments saved in the
// app's files (device_profile.txt), read once. Thread-safe.
DeviceProfile currentProfile();
// Changes it for the rest of the run (DeviceProfilePadd's tab does, and
// saves it); tests use it too
void setCurrentProfile(const DeviceProfile &profile);
// The file the adjustments are saved in, under the app's files folder
inline constexpr const char *kProfileFile = "device_profile.txt";

} // namespace ScratchPadd
