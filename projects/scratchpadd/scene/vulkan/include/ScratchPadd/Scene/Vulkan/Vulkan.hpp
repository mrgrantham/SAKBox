// Vulkan.hpp - the Vulkan backend's device and frames, for desktop code that
// draws with Vulkan itself: debug_ui's ImGui. Apps don't need it.
#pragma once

#include <ScratchPadd/Scene/Desktop/Display.hpp>

#include <volk.h>

#include <cstdint>
#include <functional>
#include <string>

namespace ScratchPadd::Scene::Vulkan {

// The format frames are drawn in (BGRA, as the Metal backend's)
constexpr VkFormat kColourFormat = VK_FORMAT_B8G8R8A8_UNORM;

// The instance, the device, and the one queue everything is drawn through
// (the renderer, ShaderPanels, ImGui), so their work runs in the order it's
// submitted
struct Context {
  VkInstance instance;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  std::uint32_t queueFamily;
  VkQueue queue;
  std::uint32_t apiVersion;
};
// Null if there's no Vulkan here (whyNoVulkan() says why)
const Context *context();
const std::string &whyNoVulkan();

// While Display::present runs its last drawings: the frame's command
// buffer, and the finished frame to draw over (one sample, kColourFormat,
// in VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL), in a rendering of one's own
// that loads it. Null at any other time.
struct Frame {
  VkCommandBuffer commands;
  VkImageView image;
  VkExtent2D extent;
};
const Frame *currentFrame(const Display &display);

// Dynamic rendering: core in Vulkan 1.3, VK_KHR_dynamic_rendering before
void beginRendering(VkCommandBuffer commands, const VkRenderingInfo &info);
void endRendering(VkCommandBuffer commands);

// How an image becomes what ImGui::Image takes: set by debug_ui (ImGui's
// Vulkan backend makes a descriptor set for it), used by
// ShaderPanel::imageId. `add` takes an image in
// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
struct ImageRegistry {
  std::function<std::uint64_t(VkImageView view)> add;
  std::function<void(std::uint64_t id)> remove;
};
ImageRegistry &imageRegistry();

} // namespace ScratchPadd::Scene::Vulkan
