// Device.hpp - the Vulkan backend's helpers over the shared device
// (Vulkan.hpp's Context): Vulkan loaded, buffers and images made and freed,
// layouts changed, pipelines built. Private to the backend.
#pragma once

#include <ScratchPadd/Scene/Vulkan/Vulkan.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ScratchPadd::Scene::Vulkan {

constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;

// Loads Vulkan's functions (volk), once: from MoltenVK in the program's
// runfiles on a Mac, the system's loader elsewhere. "" or why it couldn't.
const std::string &loadVulkan();

// How many samples frames are drawn with: 4, as the other backends, where
// the device can
VkSampleCountFlagBits frameSamples();
// Whether materials can be drawn as wireframes (fillModeNonSolid)
bool wireframes();

// A buffer the CPU writes straight into (host-visible, coherent, mapped)
struct Buffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  void *mapped = nullptr;
  VkDeviceSize size = 0;
};
Buffer makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void *contents = nullptr);
void destroy(Buffer &buffer);

// An image in the GPU's memory, with a view of all of it
struct Image {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
};
Image makeImage(std::uint32_t width, std::uint32_t height, VkFormat format,
                VkSampleCountFlagBits samples, VkImageUsageFlags usage);
void destroy(Image &image);

// A barrier taking `image` from one layout to another, after the work in
// `from`'s stages and access, before the work in `to`'s
struct Use {
  VkImageLayout layout;
  VkPipelineStageFlags stages;
  VkAccessFlags access;
};
void transition(VkCommandBuffer commands, VkImage image, VkFormat format, const Use &from,
                const Use &to);

VkCommandBuffer makeCommandBuffer();
void freeCommandBuffer(VkCommandBuffer commands);
VkFence makeFence(bool signalled);

// SPIR-V as a shader module (null if Vulkan won't take it)
VkShaderModule makeShaderModule(std::span<const std::uint32_t> words);

// A pipeline drawing into a frame's attachments (kColourFormat, and
// kDepthFormat unless `depthFormat` is undefined), with its viewport and
// scissor set when it's drawn
struct PipelineDescription {
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
  std::span<const VkVertexInputAttributeDescription> attributes;
  std::uint32_t stride = 0; // of binding 0's vertices
  VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
  bool blend = false; // as OpenGL's glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
  bool depthTest = true, depthWrite = true;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  VkColorComponentFlags colourMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkFormat depthFormat = kDepthFormat;
};
VkPipeline makePipeline(const PipelineDescription &description);

// The layout of Scene::Vertex: location 0 position, 1 normal (float3), 2
// colour (four bytes, 0-1)
std::span<const VkVertexInputAttributeDescription> vertexAttributes();

} // namespace ScratchPadd::Scene::Vulkan
