#include "ImGuiBackend.hpp"

#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace ScratchPadd::DebugUi::ImGuiBackend {

void start(Scene::Display &display) {
  ImGui_ImplGlfw_InitForOpenGL(display.glfw(), true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void stop() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
}

void newFrame(Scene::Display &) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
}

void draw(Scene::Display &) { ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }

} // namespace ScratchPadd::DebugUi::ImGuiBackend
