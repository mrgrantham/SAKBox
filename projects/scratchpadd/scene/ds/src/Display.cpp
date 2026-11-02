#include <ScratchPadd/Scene/DS/Display.hpp>

namespace ScratchPadd::Scene {

Display::Display() {
  videoSetMode(MODE_0_3D);
  vramSetBankE(VRAM_E_MAIN_BG);
  // Background 1: 4-bit text, its map at 62 KB and the font's tiles at 0.
  // Initialising a console makes it the one printf writes to, and
  // consoleInit(NULL, ...) (as consoleDemoInit does) initialises whichever
  // is: the one before is put back, so the app's console stays its own.
  PrintConsole *previous = consoleSelect(&textLayer_);
  consoleInit(&textLayer_, 1, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
  consoleSelect(previous);
  // The text over the 3D (background 0): a layer of lower priority number
  // is drawn in front
  REG_BG0CNT = (REG_BG0CNT & ~3) | BG_PRIORITY(1);
  REG_BG1CNT = (REG_BG1CNT & ~3) | BG_PRIORITY(0);
}

} // namespace ScratchPadd::Scene
