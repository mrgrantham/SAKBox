// Windows' pads, through XInput (Xbox pads, and the many that act as one).
// Asking an empty slot is slow (milliseconds), so empty slots are asked
// again only once a second.
#if defined(_WIN32)
#include <ScratchPadd/Input/Devices.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX // (the build may define it)
#define NOMINMAX
#endif
#include <windows.h>
#include <xinput.h>

#include <chrono>

namespace ScratchPadd::Input {

Gamepad readPlatformPad() {
  struct Slot {
    bool connected = false;
    DWORD packet = 0;
    std::chrono::steady_clock::time_point lastAsked{}, lastChanged{};
  };
  static Slot slots[XUSER_MAX_COUNT];
  const auto now = std::chrono::steady_clock::now();
  int newest = -1;
  XINPUT_STATE newestState{};
  for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
    Slot &slot = slots[i];
    if (!slot.connected && now - slot.lastAsked < std::chrono::seconds(1))
      continue;
    slot.lastAsked = now;
    XINPUT_STATE state{};
    slot.connected = XInputGetState(i, &state) == ERROR_SUCCESS;
    if (!slot.connected)
      continue;
    if (state.dwPacketNumber != slot.packet) {
      slot.packet = state.dwPacketNumber;
      slot.lastChanged = now;
    }
    // The one used most recently
    if (newest < 0 || slot.lastChanged > slots[newest].lastChanged) {
      newest = static_cast<int>(i);
      newestState = state;
    }
  }
  if (newest < 0)
    return {};
  const XINPUT_GAMEPAD &g = newestState.Gamepad;
  return fromXInput(XInputState{g.wButtons, g.bLeftTrigger, g.bRightTrigger, g.sThumbLX, g.sThumbLY,
                                g.sThumbRX, g.sThumbRY});
}

} // namespace ScratchPadd::Input
#endif
