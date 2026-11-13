// Linux's pads, through evdev: the devices in /dev/input whose kernel
// driver gives them the gamepad buttons (BTN_SOUTH...), looked for once a
// second, read without waiting. Reading them needs read access to the
// device (the `input` group).
#if defined(__linux__)
#include <ScratchPadd/Input/Devices.hpp>

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace ScratchPadd::Input {

namespace {

struct Device {
  std::string path;
  int fd = -1;
  EvdevPad pad;
  std::chrono::steady_clock::time_point lastEvent{};
  Device(std::string p, int f, std::uint16_t vendor) : path(std::move(p)), fd(f), pad(vendor) {}
  ~Device() {
    if (fd >= 0)
      close(fd);
  }
};

bool hasBit(const unsigned long *bits, int bit) {
  constexpr int kBitsPerLong = sizeof(unsigned long) * 8;
  return (bits[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1;
}

// A gamepad's device opened, or nullptr
std::unique_ptr<Device> openPad(const std::string &path) {
  const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    return nullptr;
  unsigned long keys[(KEY_MAX + 1) / (sizeof(unsigned long) * 8) + 1] = {};
  input_id id{};
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys) < 0 || !hasBit(keys, BTN_GAMEPAD) ||
      ioctl(fd, EVIOCGID, &id) < 0) {
    close(fd);
    return nullptr;
  }
  auto device = std::make_unique<Device>(path, fd, id.vendor);
  for (int code : {ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ}) {
    input_absinfo info{};
    if (ioctl(fd, EVIOCGABS(code), &info) == 0)
      device->pad.setRange(code, {info.minimum, info.maximum});
  }
  return device;
}

} // namespace

Gamepad readPlatformPad() {
  static std::vector<std::unique_ptr<Device>> devices;
  static std::chrono::steady_clock::time_point lastLooked{};
  const auto now = std::chrono::steady_clock::now();
  if (now - lastLooked >= std::chrono::seconds(1)) {
    lastLooked = now;
    if (DIR *dir = opendir("/dev/input")) {
      while (const dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.rfind("event", 0) != 0)
          continue;
        const std::string path = "/dev/input/" + name;
        bool known = false;
        for (const auto &device : devices)
          known = known || device->path == path;
        if (!known)
          if (auto device = openPad(path))
            devices.push_back(std::move(device));
      }
      closedir(dir);
    }
  }
  Device *newest = nullptr;
  for (auto it = devices.begin(); it != devices.end();) {
    Device &device = **it;
    input_event event{};
    bool gone = false;
    for (;;) {
      const ssize_t got = ::read(device.fd, &event, sizeof event);
      if (got == sizeof event) {
        device.pad.apply(event.type, event.code, event.value);
        device.lastEvent = now;
        continue;
      }
      gone = got < 0 && errno == ENODEV; // unplugged
      break;
    }
    if (gone) {
      it = devices.erase(it);
      continue;
    }
    if (!newest || device.lastEvent > newest->lastEvent)
      newest = &device;
    ++it;
  }
  return newest ? newest->pad.pad() : Gamepad{};
}

} // namespace ScratchPadd::Input
#endif
