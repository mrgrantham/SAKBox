#pragma once

// AppEnded.hpp - what a handheld app does once it has ended: at the end of
// main, after its padds have stopped,
//
//   ScratchPadd::AppEnded::show("Cyclone ragdoll");          // "... has ended."
//   ScratchPadd::AppEnded::show("Cyclone ragdoll", "Saved."); // its own words
//
// - DS: both screens cleared, the title and the message on the top one, and
//   what a button does on the bottom; it waits for a button (START, which
//   quit, let go first), then switches the DS off, or goes back to the
//   menu that started the app if that menu left a way back (calico's jump
//   target). Without this, a DS app that returned from main left its last
//   frame on the screens.
// - 3DS: nothing; the app returns to the HOME Menu.
// A debug build's log (DebugLog::Session) says which way a DS goes.

#include <ScratchPadd/AppEnded/EndedText.hpp>

#include <string_view>

#if defined(__NDS__)
#include <ScratchPadd/Logger.hpp>
#include <nds.h>

#include <cstdio>
#endif

namespace ScratchPadd::AppEnded {

#if defined(__NDS__)
inline void show(std::string_view title, std::string_view message = {}) {
  constexpr int kColumns = 32, kRows = 24;
  const Leaving leaving = pmHasResetJumpTarget() ? Leaving::BackToMenu : Leaving::SwitchOff;
  PADDLOG_INFO("Ended: a button {}", leaving == Leaving::BackToMenu
                                         ? "goes back to the menu (calico has a jump target)"
                                         : "switches the DS off");

  // Text on both screens and nothing else (the app's 3D and backgrounds
  // gone), the main engine on top
  videoSetMode(MODE_0_2D);
  videoSetModeSub(MODE_0_2D);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankC(VRAM_C_SUB_BG);
  lcdMainOnTop();
  setBackdropColor(0);
  setBackdropColorSub(0);
  static PrintConsole top, bottom;
  consoleInit(&top, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
  consoleInit(&bottom, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

  // Each screen's lines in its middle (libnds counts rows and columns from 0)
  const EndedText text = endedText(title, message, leaving, kColumns - 1);
  const auto printCentred = [](PrintConsole &console, const std::vector<std::string> &lines) {
    consoleSelect(&console);
    consoleClear();
    const int firstRow = (kRows - static_cast<int>(lines.size())) / 2;
    for (std::size_t i = 0; i < lines.size(); ++i)
      std::printf("\x1b[%d;%dH%s", firstRow + static_cast<int>(i),
                  (kColumns - static_cast<int>(lines[i].size())) / 2, lines[i].c_str());
  };
  printCentred(top, text.top);
  printCentred(bottom, text.bottom);

  // A button pressed once START (or whatever quit) is let go; pmMainLoop
  // stops it early when the console's asked to reset (L+R+START+SELECT)
  bool letGo = false;
  while (pmMainLoop()) {
    swiWaitForVBlank();
    scanKeys();
    const u32 held = keysHeld() & ~KEY_LID;
    if (letGo && held)
      break;
    letGo = letGo || !held;
  }
  // (Returning from main then goes back to the menu, or with nowhere to
  // go, calico switches the DS off; switching off here makes sure)
  if (leaving == Leaving::SwitchOff)
    systemShutDown();
}
#else
inline void show(std::string_view, std::string_view = {}) {}
#endif

} // namespace ScratchPadd::AppEnded
