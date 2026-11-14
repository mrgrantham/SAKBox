// ConsolePanel.hpp - ControlTabs on a DS or 3DS: its lines printed on the
// current text console (a console chosen with consoleSelect, on either; its
// input comes from the pad, panelInputFrom in ControlTabs.hpp). The two
// consoles take the same escape codes, so this is the same on both. Also printLine, for an app's
// own text on either console. Only for the handhelds.
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

// Prints a line of text at `row` (counted from 0 on both consoles) from the
// first column, cut to `columns` (usableColumns of the console's width, so
// it never wraps, or on the bottom row scrolls the screen), and clears the
// rest of the row. `format` and `args` are printf's.
template <typename... Args> void printLine(int row, int columns, const char *format, Args... args) {
  char text[128];
  if constexpr (sizeof...(Args) == 0) {
    std::snprintf(text, sizeof text, "%s", format);
  } else {
    std::snprintf(text, sizeof text, format, args...);
  }
  std::printf("\x1b[%d;%dH%.*s\x1b[K", row + kFirstRow, kFirstRow, columns, text);
}

// Prints `tabs` on the current console from its top row, if it's shown and
// changed since last printed. Everything is the console's own colour (white)
// but what's chosen: a control that can be changed bright green, one that
// can't (information, an Info control) bright yellow, as is the chosen tab.
// Returns whether it printed.
inline bool draw(ControlTabs &tabs) {
  if (!tabs.visible() || !tabs.dirty())
    return false;
  int row = kFirstRow;
  for (const Line &line : tabs.lines()) {
    std::printf("\x1b[%d;%dH", row++, kFirstRow);
    for (const Segment &segment : line) {
      // (39: back to the default colour; libnds' console has no 0, reset)
      const char *colour = !segment.highlighted              ? ""
                           : segment.shows == Shows::Setting ? "\x1b[32;1m"  // bright green
                                                             : "\x1b[33;1m"; // bright yellow
      std::printf(*colour ? "%s%s\x1b[39;22m" : "%s%s", colour, segment.text.c_str());
    }
    std::printf("\x1b[K"); // the rest of the row cleared
  }
  return true;
}

} // namespace ScratchPadd::HandheldControls
