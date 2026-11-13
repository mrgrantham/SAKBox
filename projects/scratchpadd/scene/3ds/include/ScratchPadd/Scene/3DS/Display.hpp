// Display.hpp - the 3DS's top screen, where the scene layer draws (citro3d),
// in stereo while the 3D slider is up. The bottom screen is the app's (a
// console for text, as consoleInit(GFX_BOTTOM, ...) makes).
//
//   gfxInitDefault();
//   Scene::Display display;            // the top screen
//   consoleInit(GFX_BOTTOM, nullptr);  // the bottom screen's console
//   Scene::Renderer renderer(display);
#pragma once

#include <citro3d.h>

namespace ScratchPadd::Scene {

class Display {
public:
  // Starts citro3d, and makes the top screen's render targets, one for each
  // eye (after gfxInitDefault)
  Display();
  ~Display();
  Display(const Display &) = delete;
  Display &operator=(const Display &) = delete;

  // In points, as the overlay has them: the top screen's pixels
  int width() const { return 400; }
  int height() const { return 240; }

  // Each eye's render target (the left's is the only one shown with the
  // slider down)
  C3D_RenderTarget *left() const { return left_; }
  C3D_RenderTarget *right() const { return right_; }

private:
  C3D_RenderTarget *left_ = nullptr, *right_ = nullptr;
};

} // namespace ScratchPadd::Scene
