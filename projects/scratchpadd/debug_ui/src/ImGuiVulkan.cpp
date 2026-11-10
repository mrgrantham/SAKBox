#include "ImGuiBackend.hpp"

#include <ScratchPadd/Scene/Vulkan/Vulkan.hpp>

#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace ScratchPadd::DebugUi::ImGuiBackend {

namespace {
// The format ImGui draws into: the finished frame's (Vulkan.hpp's Frame)
const VkFormat kFrameFormat = Scene::Vulkan::kColourFormat;
} // namespace

void start(Scene::Display &display) {
  const Scene::Vulkan::Context &c = *Scene::Vulkan::context();
  ImGui_ImplGlfw_InitForVulkan(display.glfw(), true);
  ImGui_ImplVulkan_InitInfo info{};
  info.ApiVersion = c.apiVersion;
  info.Instance = c.instance;
  info.PhysicalDevice = c.physicalDevice;
  info.Device = c.device;
  info.QueueFamily = c.queueFamily;
  info.Queue = c.queue;
  info.DescriptorPoolSize = 64; // its own pool: the font and ShaderPanels' images
  info.MinImageCount = 2;
  info.ImageCount = 2;
  // Into the frame, after it's resolved (one sample), in a rendering of its
  // own
  info.UseDynamicRendering = true;
  info.PipelineInfoMain.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &kFrameFormat};
  ImGui_ImplVulkan_Init(&info);
  // ShaderPanels' images, as ImGui::Image takes them
  Scene::Vulkan::imageRegistry() = {
      .add =
          [](VkImageView view) {
            return reinterpret_cast<std::uint64_t>(
                ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
          },
      .remove =
          [](std::uint64_t id) {
            ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(id));
          }};
}

void stop() {
  vkQueueWaitIdle(Scene::Vulkan::context()->queue);
  Scene::Vulkan::imageRegistry() = {};
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
}

void newFrame(Scene::Display &) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
}

void draw(Scene::Display &display) {
  const Scene::Vulkan::Frame *frame = Scene::Vulkan::currentFrame(display);
  if (!frame)
    return;
  const VkRenderingAttachmentInfo colour{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                         .imageView = frame->image,
                                         .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                         .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                         .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  const VkRenderingInfo rendering{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea = {{0, 0}, frame->extent},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &colour};
  Scene::Vulkan::beginRendering(frame->commands, rendering);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame->commands);
  Scene::Vulkan::endRendering(frame->commands);
}

} // namespace ScratchPadd::DebugUi::ImGuiBackend
