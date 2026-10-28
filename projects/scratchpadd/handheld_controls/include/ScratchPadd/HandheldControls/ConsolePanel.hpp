// ConsolePanel.hpp - ControlTabs on a DS or 3DS: its input from the buttons
// and touch screen, and its lines printed on the current text console (the
// DS's libnds console, or a 3DS console chosen with consoleSelect). The two
// consoles take the same escape codes and the keys have the same names, so
// this is the same on both. Only for the handhelds.
#pragma once

#if defined(__NDS__)
#include <nds.h>
#elif defined(__3DS__)
#include <3ds.h>
#else
#error "ConsolePanel.hpp is for the Nintendo DS and 3DS"
#endif

#include <ScratchPadd/HandheldControls/ControlTabs.hpp>

#include <cstdint>
#include <cstdio>

namespace ScratchPadd::HandheldControls {

// How wide to make a ControlTabs on a console `consoleColumns` wide: one
// less, as writing the last column moves the console's cursor to the next
// line (and on the bottom row scrolls the screen)
constexpr int usableColumns(int consoleColumns) { return consoleColumns - 1; }

// The first row's number in a cursor position (ESC [ row ; column H): libnds'
// console counts from 0, libctru's from 1, as ANSI does
#if defined(__NDS__)
constexpr int kFirstRow = 0;
#else
constexpr int kFirstRow = 1;
#endif

// This frame's input: `down` is the buttons that went down this frame (as
// libnds' keysDown() or libctru's hidKeysDown() give them), and a touch is
// read from the touch screen, in 8-pixel text cells
inline Input inputFrom(std::uint32_t down) {
  Input input{.up = (down & KEY_UP) != 0,
              .down = (down & KEY_DOWN) != 0,
              .left = (down & KEY_LEFT) != 0,
              .right = (down & KEY_RIGHT) != 0,
              .a = (down & KEY_A) != 0,
              .l = (down & KEY_L) != 0,
              .r = (down & KEY_R) != 0,
              .select = (down & KEY_SELECT) != 0};
  if (down & KEY_TOUCH) {
    touchPosition touch;
#if defined(__NDS__)
    touchRead(&touch);
#else
    hidTouchRead(&touch);
#endif
    input.touched = {{touch.px / 8, touch.py / 8}};
  }
  return input;
}

// Prints `tabs` on the current console from its top row, if it's shown and
// changed since last printed: the chosen tab and control in bright yellow.
// Returns whether it printed.
inline bool draw(ControlTabs &tabs) {
  if (!tabs.visible() || !tabs.dirty())
    return false;
  int row = kFirstRow;
  for (const Line &line : tabs.lines()) {
    std::printf("\x1b[%d;%dH", row++, kFirstRow);
    for (const Segment &segment : line) {
      // Bright yellow, then the default colour (39: libnds' console has no
      // 0, reset)
      std::printf(segment.highlighted ? "\x1b[33;1m%s\x1b[39m" : "%s", segment.text.c_str());
    }
    std::printf("\x1b[K"); // the rest of the row cleared
  }
  return true;
}

} // namespace ScratchPadd::HandheldControls
