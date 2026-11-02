#include <ScratchPadd/DebugUi/DebugUi.hpp>

#include "ImGuiBackend.hpp"

#include "imgui.h"

namespace ScratchPadd::DebugUi {

Ui::Ui(Scene::Display &display, Theme::Colors colors) : display_(display) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  Theme::apply(colors);
  ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini into the working directory
  ImGuiBackend::start(display);
  drawing_ = display.drawLast([this] {
    if (!frameStarted_)
      return;
    ImGui::Render();
    ImGuiBackend::draw(display_);
    frameStarted_ = false;
  });
}

Ui::~Ui() {
  display_.stopDrawingLast(drawing_);
  ImGuiBackend::stop();
  ImGui::DestroyContext();
}

void Ui::newFrame() {
  if (frameStarted_)
    ImGui::EndFrame(); // a frame that was never shown
  ImGuiBackend::newFrame(display_);
  ImGui::NewFrame();
  frameStarted_ = true;
  const ImGuiIO &io = ImGui::GetIO();
  display_.setTaken(io.WantCaptureKeyboard, io.WantCaptureMouse);
}

} // namespace ScratchPadd::DebugUi
