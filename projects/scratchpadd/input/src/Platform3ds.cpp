// The 3DS's buttons, circle pad, C-stick (a New 3DS's) and touch screen
#if defined(__3DS__)
#include <ScratchPadd/Input/Devices.hpp>

#include <3ds.h>

namespace ScratchPadd::Input {

static_assert(NintendoKeys::A == KEY_A && NintendoKeys::B == KEY_B && NintendoKeys::X == KEY_X &&
              NintendoKeys::Y == KEY_Y && NintendoKeys::L == KEY_L && NintendoKeys::R == KEY_R &&
              NintendoKeys::ZL == KEY_ZL && NintendoKeys::ZR == KEY_ZR &&
              NintendoKeys::Start == KEY_START && NintendoKeys::Select == KEY_SELECT &&
              NintendoKeys::Up == KEY_DUP && NintendoKeys::Down == KEY_DDOWN &&
              NintendoKeys::Left == KEY_DLEFT && NintendoKeys::Right == KEY_DRIGHT);

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
