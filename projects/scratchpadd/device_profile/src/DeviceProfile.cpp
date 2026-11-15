// The built-in device profiles, and their adjustable values as text (see
// DeviceProfile.hpp). The numbers are the roadmap's profile table: the DS
// Lite's and old 3DS's limits, and desktops with none.
#include <ScratchPadd/DeviceProfile/DeviceProfile.hpp>

#include <algorithm>
#include <charconv>

namespace ScratchPadd {

void DeviceProfile::setName(std::string_view text) {
  name.fill('\0');
  std::copy_n(text.begin(), std::min(text.size(), name.size() - 1), name.begin());
}

DevicePlatform DeviceProfile::thisPlatform() {
#if defined(__NDS__)
  return DevicePlatform::Ds;
#elif defined(__3DS__)
  return DevicePlatform::ThreeDs;
#elif defined(__APPLE__)
  return DevicePlatform::Mac;
#elif defined(_WIN32)
  return DevicePlatform::Windows;
#else
  return DevicePlatform::Linux;
#endif
}

std::string_view platformName(DevicePlatform platform) {
  switch (platform) {
  case DevicePlatform::Ds:
    return "Nintendo DS";
  case DevicePlatform::ThreeDs:
    return "Nintendo 3DS";
  case DevicePlatform::Mac:
    return "macOS";
  case DevicePlatform::Windows:
    return "Windows";
  case DevicePlatform::Linux:
    return "Linux";
  }
  return "?";
}

DeviceProfile DeviceProfile::forPlatform(DevicePlatform platform) {
  DeviceProfile p;
  p.platform = platform;
  switch (platform) {
  case DevicePlatform::Ds:
    p.setName("DS Lite");
    p.device = {.cpuMHz = 67, .cores = 1, .fpu = false, .memoryMB = 4};
    p.screens = {.count = 2,
                 .width = 256,
                 .height = 192,
                 .secondWidth = 256,
                 .secondHeight = 192,
                 .touch = true};
    p.scene = {.maxPolygons = 2048,
               .maxVertices = 6144,
               .framesPerSecond = 30,
               .outlines = false,
               .fog = true};
    p.physics = {.stepsPerSecond = 15, .subSteps = 4};
    p.textures = {.budgetKB = 64, .colours = 16};
    p.audio = {.voices = 16, .hardwareChannels = true};
    p.controls.kinds = {Input::Kind::Ds, Input::Kind::None, Input::Kind::None, Input::Kind::None};
    break;
  case DevicePlatform::ThreeDs:
    // The old 3DS: a New 3DS has twice the speed and more memory, to be
    // told apart later
    p.setName("3DS");
    p.device = {.cpuMHz = 268, .cores = 2, .fpu = true, .memoryMB = 128};
    p.screens = {.count = 2,
                 .width = 400,
                 .height = 240,
                 .secondWidth = 320,
                 .secondHeight = 240,
                 .touch = true,
                 .stereo = true};
    p.scene = {
        .maxPolygons = 0, .maxVertices = 0, .framesPerSecond = 60, .outlines = true, .fog = true};
    p.physics = {.stepsPerSecond = 60, .subSteps = 4};
    p.textures = {.budgetKB = 1024, .colours = 0};
    p.audio = {.voices = 24, .hardwareChannels = false};
    p.controls.kinds = {Input::Kind::ThreeDs, Input::Kind::NewThreeDs, Input::Kind::None,
                        Input::Kind::None};
    break;
  case DevicePlatform::Mac:
  case DevicePlatform::Windows:
  case DevicePlatform::Linux:
    p.setName(platform == DevicePlatform::Mac       ? "Mac"
              : platform == DevicePlatform::Windows ? "Windows PC"
                                                    : "Linux PC");
    p.device = {.cpuMHz = 0, .cores = 0, .fpu = true, .memoryMB = 0}; // (0: plenty)
    p.screens = {.count = 1};
    p.scene = {
        .maxPolygons = 0, .maxVertices = 0, .framesPerSecond = 60, .outlines = true, .fog = true};
    p.physics = {.stepsPerSecond = 60, .subSteps = 4};
    p.textures = {.budgetKB = 0, .colours = 0};
    p.audio = {.voices = 32, .hardwareChannels = false};
    p.controls.kinds = {Input::Kind::Keyboard, Input::Kind::Xbox, Input::Kind::PlayStation,
                        Input::Kind::SwitchPro};
    break;
  }
  return p;
}

std::vector<std::pair<const char *, int *>> adjustableInts(DeviceProfile &p) {
  return {{"scene.maxPolygons", &p.scene.maxPolygons},
          {"scene.maxVertices", &p.scene.maxVertices},
          {"scene.framesPerSecond", &p.scene.framesPerSecond},
          {"physics.stepsPerSecond", &p.physics.stepsPerSecond},
          {"physics.subSteps", &p.physics.subSteps},
          {"textures.budgetKB", &p.textures.budgetKB},
          {"textures.colours", &p.textures.colours},
          {"audio.voices", &p.audio.voices}};
}

std::vector<std::pair<const char *, bool *>> adjustableFlags(DeviceProfile &p) {
  return {{"scene.outlines", &p.scene.outlines}, {"scene.fog", &p.scene.fog}};
}

std::string profileText(const DeviceProfile &profile) {
  DeviceProfile copy = profile; // (the lists point into a profile they may change)
  std::string out;
  for (const auto &[name, value] : adjustableInts(copy))
    ((out += name) += ' ') += std::to_string(*value) += '\n';
  for (const auto &[name, value] : adjustableFlags(copy))
    ((out += name) += ' ') += (*value ? "1\n" : "0\n");
  return out;
}

DeviceProfile profileFromText(std::string_view text, DeviceProfile start) {
  const auto ints = adjustableInts(start);
  const auto flags = adjustableFlags(start);
  while (!text.empty()) {
    const std::size_t end = text.find('\n');
    const std::string_view line = text.substr(0, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    const std::size_t space = line.find(' ');
    if (space == std::string_view::npos)
      continue;
    const std::string_view key = line.substr(0, space), valueText = line.substr(space + 1);
    int value = 0;
    const auto [at, error] =
        std::from_chars(valueText.data(), valueText.data() + valueText.size(), value);
    if (error != std::errc{} || at != valueText.data() + valueText.size())
      continue;
    for (const auto &[name, target] : ints) {
      if (key == name)
        *target = value;
    }
    for (const auto &[name, target] : flags) {
      if (key == name)
        *target = value != 0;
    }
  }
  return start;
}

} // namespace ScratchPadd
