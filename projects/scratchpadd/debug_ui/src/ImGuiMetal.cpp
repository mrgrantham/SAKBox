#include "ImGuiBackend.hpp"

#include <ScratchPadd/Scene/Metal/Metal.hpp>

#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_metal.h>

#include <Metal/Metal.hpp>

namespace ScratchPadd::DebugUi::ImGuiBackend {

void start(Scene::Display &display) {
  ImGui_ImplGlfw_InitForOther(display.glfw(), true);
  ImGui_ImplMetal_Init(Scene::Metal::device());
}

void stop() {
  ImGui_ImplMetal_Shutdown();
  ImGui_ImplGlfw_Shutdown();
}

void newFrame(Scene::Display &display) {
  // ImGui's pipeline follows the pass it draws in: the one over the frame
  ImGui_ImplMetal_NewFrame(Scene::Metal::framePass(display));
  ImGui_ImplGlfw_NewFrame();
}

void draw(Scene::Display &display) {
  const Scene::Metal::Frame *frame = Scene::Metal::currentFrame(display);
  if (!frame)
    return;
  MTL::RenderCommandEncoder *encoder = frame->commands->renderCommandEncoder(frame->pass);
  ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), frame->commands, encoder);
  encoder->endEncoding();
}

} // namespace ScratchPadd::DebugUi::ImGuiBackend
