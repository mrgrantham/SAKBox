// Display.hpp - the DS's top screen, where the scene layer draws: the 3D,
// with a 2D text layer over it for the overlay's text. The bottom screen is
// the app's (a console for text, as consoleDemoInit makes).
//
//   Scene::Display display;   // the top screen
//   consoleDemoInit();        // the bottom screen's console, for printf
//   Scene::Renderer renderer(display);
#pragma once

#include <nds.h>

namespace ScratchPadd::Scene {

class Display {
public:
  // Puts the top screen in 3D mode with the text layer over it (background
  // 1, its tiles and map in VRAM bank E, leaving banks A to D for
  // textures). printf keeps writing where it did (the app's console).
  Display();
  Display(const Display &) = delete;
  Display &operator=(const Display &) = delete;

  int width() const { return 256; }
  int height() const { return 192; }

  // The text layer's console (the overlay's text; the renderer writes it)
  PrintConsole &textLayer() { return textLayer_; }

private:
  PrintConsole textLayer_{};
};

} // namespace ScratchPadd::Scene
