#include <ScratchPadd/Scene/DS/Display.hpp>

namespace ScratchPadd::Scene {

Display::Display() {
  // Mode 5: background 0 the 3D, background 3 a bitmap
  videoSetMode(MODE_5_3D);
  vramSetBankE(VRAM_E_MAIN_BG);
  // 256 x 256 at a byte a pixel: bank E's 64 KB (the screen shows 192 rows)
  const int layer = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
  textPixels_ = bgGetGfxPtr(layer);
  dmaFillHalfWords(0, textPixels_, 256 * 256);
  // The text over the 3D: a layer of lower priority number is drawn in front
  // (background 0, the 3D, wasn't made by bgInit: its register directly)
  REG_BG0CNT = (REG_BG0CNT & ~3) | BG_PRIORITY(1);
  bgSetPriority(layer, 0);
}

} // namespace ScratchPadd::Scene
