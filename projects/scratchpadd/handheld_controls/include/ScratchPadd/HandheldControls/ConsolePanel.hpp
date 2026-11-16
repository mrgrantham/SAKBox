// ConsolePanel.hpp - ControlTabs on a DS or 3DS: its lines printed on the
// current text console (a console chosen with consoleSelect, on either; its
// input comes from the pad, panelInputFrom in ControlTabs.hpp). The two
// consoles take the same escape codes, so this is the same on both. Also printLine, for an app's
// own text on either console, and GraphPixels, which draws ControlTabs'
// graphs in pixels under the bottom screen's text. Only for the handhelds.
#pragma once

#if defined(__NDS__)
#include <nds.h>
#elif defined(__3DS__)
#include <3ds.h>
#else
#error "ConsolePanel.hpp is for the Nintendo DS and 3DS"
#endif

#include <ScratchPadd/HandheldControls/ControlTabs.hpp>
#include <ScratchPadd/HandheldControls/GraphPlot.hpp>

#include <cstdint>
#include <cstdio>
#include <vector>

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

// Graphs drawn in pixels on the bottom screen, under its console's text,
// in the rows ControlTabs leaves for them (ControlTabs::setPixelGraphs).
// Start it once the bottom screen's console is made, then draw the panel
// with draw(tabs, graphs) instead of draw(tabs):
//
//   graphs.start(bottomConsole);
//   tabs.setPixelGraphs(true);
//   ...each frame: consoleSelect(&bottomConsole); draw(tabs, graphs);
//
// On the DS, the console is a text layer (BG 0) on the sub engine, so the
// graphs go on a 256-colour bitmap layer behind it (BG 3, mode 5), in VRAM
// bank C after the console's font and map (64 to 128 KB): text shows over
// it, its empty cells letting the graph through. On the 3DS the console
// draws its text into the bottom screen's framebuffer, so the graphs are
// drawn straight into the same framebuffer, into the rows the panel has
// just printed empty.
class GraphPixels {
public:
  // A cyan, apart from the panel's green settings and yellow information
#if defined(__NDS__)
  static constexpr int kColourIndex = 200; // (a palette entry the console's font doesn't use)
  static constexpr std::uint16_t kColour = RGB15(4, 24, 31);
#else
  static constexpr std::uint16_t kColour = 0x061F; // RGB565: red 0, green 48 of 63, blue 31
#endif

  void start(PrintConsole &bottom) {
#if defined(__NDS__)
    (void)bottom;
    // Mode 5 keeps BG 0 as text and makes BG 3 a bitmap. Only the mode
    // bits (the lowest three) change, so the console's layer stays on.
    REG_DISPCNT_SUB = (REG_DISPCNT_SUB & ~0x7u) | (MODE_5_2D & 0x7u);
    const int layer = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 4, 0); // (4 x 16 KB in)
    bgSetPriority(layer, 3);                                              // behind the text
    bitmap_ = bgGetGfxPtr(layer);
    BG_PALETTE_SUB[kColourIndex] = kColour;
    dmaFillHalfWords(0, bitmap_, kWidth * kHeight); // (one byte a pixel)
#else
    frameBuffer_ = bottom.frameBuffer;
#endif
  }

  // Draws `areas` (ControlTabs::graphAreas()), clearing what was drawn before
  void draw(const std::vector<GraphArea> &areas) {
    clear();
    for (const GraphArea &area : areas) {
      const int left = area.column * 8, top = area.row * 8;
      // (a pixel's gap above and below, between a graph and the rows of text)
      plotGraph(area.graph, area.columns * 8, area.rows * 8 - 2,
                [&](int x, int y) { set(left + x, top + 1 + y); });
    }
    drawn_ = areas;
  }

  // Clears what was drawn
  void clear() {
#if defined(__NDS__)
    if (!drawn_.empty())
      dmaFillHalfWords(0, bitmap_, kWidth * kHeight);
#else
    for (const GraphArea &area : drawn_)
      for (int x = area.column * 8; x < (area.column + area.columns) * 8; ++x)
        for (int y = area.row * 8; y < (area.row + area.rows) * 8; ++y)
          frameBuffer_[x * kHeight + (kHeight - 1 - y)] = 0;
#endif
    drawn_.clear();
  }

private:
#if defined(__NDS__)
  static constexpr int kWidth = 256, kHeight = 192;
  // A pixel, one byte each; VRAM takes only 16-bit writes, so two at a time
  void set(int x, int y) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight)
      return;
    std::uint16_t &pair = bitmap_[(y * kWidth + x) / 2];
    pair = (x & 1) ? static_cast<std::uint16_t>((pair & 0x00FF) | (kColourIndex << 8))
                   : static_cast<std::uint16_t>((pair & 0xFF00) | kColourIndex);
  }
  std::uint16_t *bitmap_ = nullptr;
#else
  static constexpr int kWidth = 320, kHeight = 240;
  // The framebuffer is the screen turned a quarter: a column of 240 pixels
  // for each x, from the bottom up
  void set(int x, int y) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight)
      return;
    frameBuffer_[x * kHeight + (kHeight - 1 - y)] = kColour;
  }
  std::uint16_t *frameBuffer_ = nullptr;
#endif
  std::vector<GraphArea> drawn_;
};

// Draws `tabs` as draw(tabs) does, and its graphs in pixels with `graphs`
// whenever it prints; clears the graphs when the panel's hidden. Returns
// whether it printed.
inline bool draw(ControlTabs &tabs, GraphPixels &graphs) {
  if (!tabs.visible()) {
    graphs.clear();
    return false;
  }
  const bool printed = draw(tabs);
  if (printed)
    graphs.draw(tabs.graphAreas());
  return printed;
}

} // namespace ScratchPadd::HandheldControls
