// Display.hpp - the DS's top screen, where the scene layer draws: the 3D,
// with a 2D bitmap layer over it for the overlay's text. The bottom screen
// is the app's (a console for text, as consoleDemoInit makes).
//
//   Scene::Display display;   // the top screen
//   consoleDemoInit();        // the bottom screen's console, for printf
//   Scene::Renderer renderer(display);
#pragma once

#include <nds.h>

#include <cstdint>

namespace ScratchPadd::Scene {

class Display {
public:
  // Puts the top screen in 3D mode with the text layer over it: background
  // 3, a 256-colour bitmap in VRAM bank E (leaving banks A to D for
  // textures), colour 0 see-through
  Display();
  Display(const Display &) = delete;
  Display &operator=(const Display &) = delete;

  int width() const { return 256; }
  int height() const { return 192; }

  // The text layer's pixels, a byte each (a colour in textPalette), 256 a
  // row; VRAM, so written 16 bits at a time (the renderer copies a frame's
  // text in whole)
  std::uint16_t *textPixels() const { return textPixels_; }
  // Its 256 colours, RGB15 (0 is see-through)
  std::uint16_t *textPalette() const { return BG_PALETTE; }

private:
  std::uint16_t *textPixels_ = nullptr;
};

} // namespace ScratchPadd::Scene
