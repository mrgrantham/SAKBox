// The device's profile for this run: built in, with the saved adjustments,
// read once and changed by DeviceProfilePadd's tab (see DeviceProfile.hpp)
#include <ScratchPadd/DeviceProfile/DeviceProfile.hpp>
#include <ScratchPadd/Storage/Files.hpp>
#include <ScratchPadd/Storage/Folders.hpp>

#include <mutex>
#include <optional>

namespace ScratchPadd {

namespace {
std::mutex gMutex;
std::optional<DeviceProfile> gCurrent;
} // namespace

DeviceProfile currentProfile() {
  std::lock_guard lock(gMutex);
  if (!gCurrent) {
    DeviceProfile profile = DeviceProfile::forThisDevice();
    if (const auto bytes = Storage::loadFile(Storage::Folders::forApp().files + kProfileFile))
      profile = profileFromText(
          std::string_view(reinterpret_cast<const char *>(bytes->data()), bytes->size()), profile);
    gCurrent = profile;
  }
  return *gCurrent;
}

void setCurrentProfile(const DeviceProfile &profile) {
  std::lock_guard lock(gMutex);
  gCurrent = profile;
}

} // namespace ScratchPadd
