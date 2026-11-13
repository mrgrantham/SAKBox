#include <ScratchPadd/Input/Devices.hpp>
#include <ScratchPadd/Input/Reader.hpp>

namespace ScratchPadd::Input {

Reader::Reader() : source_(readPlatformPad) {}
Reader::~Reader() = default;

Gamepad Reader::read(const Keyboard *keyboard) {
  Gamepad pad = source_();
  pad.left = withoutDeadZone(pad.left, deadZone_);
  pad.right = withoutDeadZone(pad.right, deadZone_);

  if (keyboard && keyboard->open) {
    float mouseX = 0, mouseY = 0;
    if (haveCursor_) {
      mouseX = keyboard->cursorX - lastCursorX_;
      mouseY = keyboard->cursorY - lastCursorY_;
    }
    lastCursorX_ = keyboard->cursorX;
    lastCursorY_ = keyboard->cursorY;
    haveCursor_ = true;
    if (pad.kind == Kind::None) {
      pad.kind = Kind::Keyboard;
      pad.features = profileOf(Kind::Keyboard).features;
    }
    addKeyboard(pad, *keyboard, keyMap_, mouseX, mouseY);
  }

  // Each button's presses and releases since reading began
  const std::uint32_t changed = pad.held ^ lastHeld_;
  for (std::size_t b = 0; b < kButtons; ++b) {
    if (!(changed >> b & 1))
      continue;
    if (pad.held >> b & 1)
      ++presses_[b];
    else
      ++releases_[b];
  }
  lastHeld_ = pad.held;
  // (counted by the platform instead, where it does: see readPlatformPresses)
  if (source_ == readPlatformPad) {
    readPlatformPresses(presses_, releases_);
  }
  pad.presses = presses_;
  pad.releases = releases_;
  return pad;
}

} // namespace ScratchPadd::Input
