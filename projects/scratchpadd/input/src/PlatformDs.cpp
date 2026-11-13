// The DS's buttons and touch screen
#if defined(__NDS__)
#include <ScratchPadd/Input/Devices.hpp>

#include <nds.h>

namespace ScratchPadd::Input {

static_assert(NintendoKeys::A == KEY_A && NintendoKeys::B == KEY_B && NintendoKeys::X == KEY_X &&
              NintendoKeys::Y == KEY_Y && NintendoKeys::L == KEY_L && NintendoKeys::R == KEY_R &&
              NintendoKeys::Start == KEY_START && NintendoKeys::Select == KEY_SELECT &&
              NintendoKeys::Up == KEY_UP && NintendoKeys::Down == KEY_DOWN &&
              NintendoKeys::Left == KEY_LEFT && NintendoKeys::Right == KEY_RIGHT);

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
