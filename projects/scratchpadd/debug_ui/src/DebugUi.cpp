#include <ScratchPadd/DebugUi/DebugUi.hpp>

#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace ScratchPadd::DebugUi {

Ui::Ui(Scene::Display &display, Theme::Colors colors) : display_(display) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  Theme::apply(colors);
  ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini into the working directory
  ImGui_ImplGlfw_InitForOpenGL(display.glfw(), true);
  ImGui_ImplOpenGL3_Init("#version 330");
  drawing_ = display.drawLast([this] {
    if (!frameStarted_)
      return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    frameStarted_ = false;
  });
}

Ui::~Ui() {
  display_.stopDrawingLast(drawing_);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void Ui::newFrame() {
  if (frameStarted_)
    ImGui::EndFrame(); // a frame that was never shown
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  frameStarted_ = true;
  const ImGuiIO &io = ImGui::GetIO();
  display_.setTaken(io.WantCaptureKeyboard, io.WantCaptureMouse);
}

} // namespace ScratchPadd::DebugUi
