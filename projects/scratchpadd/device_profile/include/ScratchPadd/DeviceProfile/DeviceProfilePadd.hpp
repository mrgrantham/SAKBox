// DeviceProfilePadd.hpp - the device profile's tab and its sharing: the
// profile published as shared state when the system starts (and again on
// each change), and, in a debug build, a "Profile" tab that adjusts its
// values (the scene's budget, physics steps, textures, audio) and saves
// them to device_profile.txt (by StoragePadd when the app has one), so a
// device can be tuned on the device and keep it. A release build has no
// tab: the profile is the built-in one with whatever was saved.
//
//   // listed in the system; then any padd:
//   const DeviceProfile profile = currentState<DeviceProfile>().value;   // live, with changes
//   // or, anywhere, any time (before the padds start too):
//   const DeviceProfile profile = currentProfile();
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/DeviceProfile/DeviceProfile.hpp>
#include <ScratchPadd/Storage/Files.hpp>
#include <ScratchPadd/Storage/Folders.hpp>
#include <ScratchPadd/Storage/Messages.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ScratchPadd {

class DeviceProfilePadd : public Base {
public:
#if defined(SAKBOX_DEBUG) || !defined(NDEBUG)
  static constexpr bool kTab = true;
#else
  static constexpr bool kTab = false;
#endif

  explicit DeviceProfilePadd(System *system) : Base(system) {}
  std::string name() override { return "DeviceProfilePadd"; }

  ControlValueMap generateControls() override {
    if (!kTab)
      return {};
    DeviceProfile profile = currentProfile();
    ControlValueMap controls;
    controls["Device"] = ControlType::Info(describe(profile));
    for (const auto &[name, value] : adjustableInts(profile))
      controls[name] = ControlType::Integer(*value, std::pair{0, 100000});
    for (const auto &[name, value] : adjustableFlags(profile))
      controls[name] = ControlType::Boolean(*value);
    controls[kReset] = ControlType::Boolean(false);
    return controls;
  }

  void starting() override { publishState(currentProfile()); }

  void controlChanged(const std::string &name, const ControlTypeVariant &) override {
    DeviceProfile profile = currentProfile();
    if (name == kReset) {
      profile = DeviceProfile::forThisDevice();
      for (const auto &[key, value] : adjustableInts(profile))
        setControl(key, ControlType::Integer(*value, std::pair{0, 100000}));
      for (const auto &[key, value] : adjustableFlags(profile))
        setControl(key, ControlType::Boolean(*value));
      setControl(kReset, ControlType::Boolean(false));
    } else {
      for (const auto &[key, value] : adjustableInts(profile)) {
        if (name == key)
          *value = std::get<ControlType::Integer>(controlValue(name)).value;
      }
      for (const auto &[key, value] : adjustableFlags(profile)) {
        if (name == key)
          *value = std::get<ControlType::Boolean>(controlValue(name)).value;
      }
    }
    // Saved first (written here at once without StoragePadd; queued for it
    // with one, which writes when nothing else has work)
    save(profile);
    setCurrentProfile(profile);
    publishState(profile);
  }

protected:
  // Somewhere else for device_profile.txt than the app's files (tests); then
  // it's written here, not by StoragePadd
  virtual std::optional<Storage::Folders> foldersOverride() { return std::nullopt; }

private:
  static constexpr const char *kReset = "Reset to the built-in profile";

  static std::string describe(const DeviceProfile &p) {
    std::string text =
        std::string(p.nameView()) + " (" + std::string(platformName(p.platform)) + ")";
    if (p.device.cpuMHz > 0)
      text += ", " + std::to_string(p.device.cpuMHz) + " MHz";
    if (!p.device.fpu)
      text += ", no FPU";
    if (p.device.memoryMB > 0)
      text += ", " + std::to_string(p.device.memoryMB) + " MB";
    return text;
  }

  // Only what differs from the built-in profile is meaningful, but all of it
  // is written: a later build's different defaults then don't change a
  // device someone tuned
  void save(const DeviceProfile &profile) {
    const std::string text = profileText(profile);
    auto bytes = std::make_shared<const Storage::Bytes>(text.begin(), text.end());
    const auto folders = foldersOverride();
    const PaddAddress storage = folders ? PaddAddress() : addressOf("StoragePadd");
    if (storage) {
      sendTo(storage, Storage::SaveFile{.path = kProfileFile, .bytes = std::move(bytes)});
      return;
    }
    std::string error;
    if (!Storage::saveFile(folders.value_or(Storage::Folders::forApp()).files + kProfileFile,
                           *bytes, error))
      logger().warn("{}: couldn't save {}: {}", name(), kProfileFile, error);
  }
};

} // namespace ScratchPadd
