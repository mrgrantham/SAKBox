// DebugUi.hpp - Dear ImGui on a desktop Scene::Display, with ScratchPadd's
// look (Theme.hpp): panels, sliders, stats, drawn over the scene. Desktop
// only; never compiled for the DS (see the scene layer's DESIGN.md).
//
//   Scene::Display display(1280, 800, "App");
//   Scene::Renderer renderer(display);
//   DebugUi::Ui ui(display);
//   // each frame:
//   display.pollEvents();
//   ui.newFrame();        // then ImGui windows, anywhere in the frame
//   renderer.beginFrame(...); ... renderer.endFrame();   // ImGui drawn last
#pragma once

#include <ScratchPadd/DebugUi/Theme.hpp>
#include <ScratchPadd/Scene/GL/Display.hpp>

namespace ScratchPadd::DebugUi {

class Ui {
public:
  // Starts ImGui on `display` (one at a time), with the theme's `colors`.
  // ImGui keeps no imgui.ini.
  explicit Ui(Scene::Display &display, Theme::Colors colors = Theme::Colors::Dark);
  ~Ui();
  Ui(const Ui &) = delete;
  Ui &operator=(const Ui &) = delete;

  // Starts ImGui's frame: call after the display's pollEvents and before
  // any ImGui window. While ImGui wants the keyboard or mouse (typing in a
  // field, dragging a slider), the display tells the app they're taken.
  // ImGui's frame is drawn when the renderer ends its frame.
  void newFrame();

private:
  Scene::Display &display_;
  int drawing_ = 0; // its Display::drawLast
  bool frameStarted_ = false;
};

} // namespace ScratchPadd::DebugUi
