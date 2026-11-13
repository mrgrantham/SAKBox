// The DS's buttons and touch screen
#if defined(__NDS__)
#include <ScratchPadd/Input/Devices.hpp>

#include <nds.h>

namespace ScratchPadd::Input {

static_assert(NintendoKeys::kA == KEY_A && NintendoKeys::kB == KEY_B && NintendoKeys::kX == KEY_X &&
              NintendoKeys::kY == KEY_Y && NintendoKeys::kL == KEY_L && NintendoKeys::kR == KEY_R &&
              NintendoKeys::kStart == KEY_START && NintendoKeys::kSelect == KEY_SELECT &&
              NintendoKeys::kUp == KEY_UP && NintendoKeys::kDown == KEY_DOWN &&
              NintendoKeys::kLeft == KEY_LEFT && NintendoKeys::kRight == KEY_RIGHT);

Gamepad readPlatformPad() {
  scanKeys();
  const std::uint32_t held = keysHeld();
  Touch touch;
  if (held & KEY_TOUCH) {
    touchPosition at{};
    touchRead(&at);
    touch = {true, static_cast<std::int16_t>(at.px), static_cast<std::int16_t>(at.py)};
  }
  return fromNintendo(Kind::Ds, held, 0, 0, 0, 0, touch);
}

} // namespace ScratchPadd::Input
#endif
