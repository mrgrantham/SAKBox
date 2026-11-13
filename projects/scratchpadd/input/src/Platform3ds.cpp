// The 3DS's buttons, circle pad, C-stick (a New 3DS's) and touch screen
#if defined(__3DS__)
#include <ScratchPadd/Input/Devices.hpp>

#include <3ds.h>

namespace ScratchPadd::Input {

static_assert(NintendoKeys::kA == KEY_A && NintendoKeys::kB == KEY_B && NintendoKeys::kX == KEY_X &&
              NintendoKeys::kY == KEY_Y && NintendoKeys::kL == KEY_L && NintendoKeys::kR == KEY_R &&
              NintendoKeys::kZl == KEY_ZL && NintendoKeys::kZr == KEY_ZR &&
              NintendoKeys::kStart == KEY_START && NintendoKeys::kSelect == KEY_SELECT &&
              NintendoKeys::kUp == KEY_DUP && NintendoKeys::kDown == KEY_DDOWN &&
              NintendoKeys::kLeft == KEY_DLEFT && NintendoKeys::kRight == KEY_DRIGHT);

Gamepad readPlatformPad() {
  static const bool isNew = [] {
    bool n = false;
    APT_CheckNew3DS(&n);
    return n;
  }();
  hidScanInput();
  const u32 held = hidKeysHeld();
  circlePosition circle{}, cStick{};
  hidCircleRead(&circle);
  if (isNew)
    hidCstickRead(&cStick);
  Touch touch;
  if (held & KEY_TOUCH) {
    touchPosition at{};
    hidTouchRead(&at);
    touch = {true, static_cast<std::int16_t>(at.px), static_cast<std::int16_t>(at.py)};
  }
  return fromNintendo(isNew ? Kind::NewThreeDs : Kind::ThreeDs, held, circle.dx, circle.dy,
                      cStick.dx, cStick.dy, touch);
}

} // namespace ScratchPadd::Input
#endif
