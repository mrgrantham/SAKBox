// ImGuiBackend.hpp - ImGui's platform and renderer backends for the desktop
// backend the program is built with: GLFW with OpenGL 3 (ImGuiOpenGl.cpp),
// Metal (ImGuiMetal.cpp) or Vulkan (ImGuiVulkan.cpp)
#pragma once

#include <ScratchPadd/Scene/Desktop/Display.hpp>

namespace ScratchPadd::DebugUi::ImGuiBackend {

void start(Scene::Display &display);
void stop();
// Before ImGui::NewFrame
void newFrame(Scene::Display &display);
// ImGui's draw data, rendered over the frame (in Display::present's last
// drawings)
void draw(Scene::Display &display);

} // namespace ScratchPadd::DebugUi::ImGuiBackend
