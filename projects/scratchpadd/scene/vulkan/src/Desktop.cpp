// The desktop extras (Desktop.hpp) on Vulkan
#include "Backend.hpp"

#include <ScratchPadd/Scene/Desktop/Desktop.hpp>
#include <ScratchPadd/Scene/Spirv/GlslToSpirv.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

namespace ScratchPadd::Scene::Desktop {

namespace {
// Where a ShaderPanel's shaders' loose uniforms go: set 0, a binding each
constexpr std::uint32_t kVertexUniforms = 0, kFragmentUniforms = 1;

VkDevice device() { return Vulkan::context()->device; }
} // namespace

void setVertexSnap(Renderer &renderer, float worldGrid, float pixelGrid) {
  renderer.backend().worldSnap = worldGrid;
  renderer.backend().screenSnap = pixelGrid;
}

Colour readPixel(const Renderer &renderer, float x, float y) {
  const Renderer::Backend &b = renderer.backend();
  const Vulkan::Surface &surface = b.surface;
  if (!surface.readback.mapped)
    return {};
  const float scale = static_cast<float>(surface.width) / static_cast<float>(b.display.width());
  const int px = std::clamp(static_cast<int>(x * scale), 0, surface.width - 1);
  const int py = std::clamp(static_cast<int>(y * scale), 0, surface.height - 1);
  // BGRA, top row first
  const auto *bgra = static_cast<const unsigned char *>(surface.readback.mapped) +
                     (static_cast<std::size_t>(py) * surface.width + px) * 4;
  return {bgra[2], bgra[1], bgra[0], bgra[3]};
}

struct ShaderPanel::State {
  ~State() {
    if (!Vulkan::context())
      return;
    // ImGui may still be drawing it
    vkQueueWaitIdle(Vulkan::context()->queue);
    if (imageId && Vulkan::imageRegistry().remove)
      Vulkan::imageRegistry().remove(imageId);
    Vulkan::destroy(colour);
    Vulkan::destroy(depth);
    for (Vulkan::Buffer *buffer : {&vertices, &indices, &uniforms[0], &uniforms[1]})
      Vulkan::destroy(*buffer);
    if (pipeline)
      vkDestroyPipeline(device(), pipeline, nullptr);
    for (VkShaderModule module : {vertexModule, fragmentModule})
      if (module)
        vkDestroyShaderModule(device(), module, nullptr);
    if (layout)
      vkDestroyPipelineLayout(device(), layout, nullptr);
    if (setLayout)
      vkDestroyDescriptorSetLayout(device(), setLayout, nullptr);
    if (pool)
      vkDestroyDescriptorPool(device(), pool, nullptr);
    if (done)
      vkDestroyFence(device(), done, nullptr);
    Vulkan::freeCommandBuffer(commands);
  }

  Vulkan::Image colour, depth;
  Vulkan::Buffer vertices, indices;
  std::uint32_t indexCount = 0;
  VkShaderModule vertexModule = VK_NULL_HANDLE, fragmentModule = VK_NULL_HANDLE;
  VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  VkDescriptorSet set = VK_NULL_HANDLE;
  // Each stage's loose uniforms (vertex, fragment): where they are, and the
  // buffer they're written into
  Spirv::Shader::Uniforms uniformLayouts[2];
  bool hasUniforms[2] = {false, false};
  Vulkan::Buffer uniforms[2];
  VkCommandBuffer commands = VK_NULL_HANDLE;
  VkFence done = VK_NULL_HANDLE;
  bool submitted = false;
  std::uint64_t imageId = 0;
};

namespace {

// Records `record` and submits it, on the queue frames are drawn through,
// signalling `done`
template <typename Record> void submit(ShaderPanel::State &s, Record record) {
  if (s.submitted)
    vkWaitForFences(device(), 1, &s.done, VK_TRUE, UINT64_MAX);
  vkResetFences(device(), 1, &s.done);
  const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                       .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  vkResetCommandBuffer(s.commands, 0);
  vkBeginCommandBuffer(s.commands, &begin);
  record(s.commands);
  vkEndCommandBuffer(s.commands);
  const VkSubmitInfo info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .commandBufferCount = 1,
                          .pCommandBuffers = &s.commands};
  vkQueueSubmit(Vulkan::context()->queue, 1, &info, s.done);
  s.submitted = true;
}

// The texture as ImGui reads it, after it's drawn into
const Vulkan::Use kShown{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT};

} // namespace

ShaderPanel::ShaderPanel(int width, int height, const std::string &vertexSource,
                         const std::string &fragmentSource, const MeshData &mesh)
    : width_(width), height_(height), state_(std::make_unique<State>()) {
  if (!Vulkan::context()) {
    error_ = "no Vulkan: " + Vulkan::whyNoVulkan();
    return;
  }
  State &s = *state_;
  const auto w = static_cast<std::uint32_t>(width), h = static_cast<std::uint32_t>(height);
  // Its texture: RGB as far as anyone sees (alpha is kept at 1), so a panel
  // is never see-through
  s.colour = Vulkan::makeImage(w, h, Vulkan::kColourFormat, VK_SAMPLE_COUNT_1_BIT,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  s.depth = Vulkan::makeImage(w, h, Vulkan::kDepthFormat, VK_SAMPLE_COUNT_1_BIT,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  const std::vector<std::uint32_t> indices = triangleList(mesh);
  if (!mesh.vertices.empty() && !indices.empty()) {
    s.vertices = Vulkan::makeBuffer(mesh.vertices.size_bytes(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    mesh.vertices.data());
    s.indices = Vulkan::makeBuffer(indices.size() * sizeof(std::uint32_t),
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data());
    s.indexCount = static_cast<std::uint32_t>(indices.size());
  }
  s.commands = Vulkan::makeCommandBuffer();
  s.done = Vulkan::makeFence(false);
  // Black until it's first drawn
  submit(s, [&](VkCommandBuffer commands) {
    const Vulkan::Use cleared{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_ACCESS_TRANSFER_WRITE_BIT};
    Vulkan::transition(commands, s.colour.image, Vulkan::kColourFormat,
                       {VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0}, cleared);
    const VkClearColorValue black{{0, 0, 0, 1}};
    const VkImageSubresourceRange all{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(commands, s.colour.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1,
                         &all);
    Vulkan::transition(commands, s.colour.image, Vulkan::kColourFormat, cleared, kShown);
  });

  // The GLSL as SPIR-V, each stage's loose uniforms at a binding of its own
  Spirv::Shader vertex, fragment;
  std::string why;
  if (!Spirv::compile(Spirv::Stage::vertex, Spirv::Dialect::opengl, vertexSource, vertex, why,
                      kVertexUniforms)) {
    error_ = "vertex shader: " + why;
    return;
  }
  if (!Spirv::compile(Spirv::Stage::fragment, Spirv::Dialect::opengl, fragmentSource, fragment, why,
                      kFragmentUniforms)) {
    error_ = "fragment shader: " + why;
    return;
  }
  s.vertexModule = Vulkan::makeShaderModule(vertex.words);
  s.fragmentModule = Vulkan::makeShaderModule(fragment.words);
  if (!s.vertexModule || !s.fragmentModule) {
    error_ = "Vulkan didn't take the shaders";
    return;
  }
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  for (int stage = 0; stage < 2; ++stage) {
    const std::optional<Spirv::Shader::Uniforms> &found =
        stage == 0 ? vertex.uniforms : fragment.uniforms;
    if (!found || found->size == 0)
      continue;
    s.hasUniforms[stage] = true;
    s.uniformLayouts[stage] = *found;
    s.uniforms[stage] = Vulkan::makeBuffer(found->size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    bindings.push_back({.binding = found->binding,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = 1,
                        .stageFlags = stage == 0
                                          ? VkShaderStageFlags{VK_SHADER_STAGE_VERTEX_BIT}
                                          : VkShaderStageFlags{VK_SHADER_STAGE_FRAGMENT_BIT}});
  }
  const VkDescriptorSetLayoutCreateInfo setLayout{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<std::uint32_t>(bindings.size()),
      .pBindings = bindings.data()};
  vkCreateDescriptorSetLayout(device(), &setLayout, nullptr, &s.setLayout);
  const VkPipelineLayoutCreateInfo layout{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                          .setLayoutCount = 1,
                                          .pSetLayouts = &s.setLayout};
  vkCreatePipelineLayout(device(), &layout, nullptr, &s.layout);
  if (!bindings.empty()) {
    const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    static_cast<std::uint32_t>(bindings.size())};
    const VkDescriptorPoolCreateInfo pool{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .maxSets = 1,
                                          .poolSizeCount = 1,
                                          .pPoolSizes = &size};
    vkCreateDescriptorPool(device(), &pool, nullptr, &s.pool);
    const VkDescriptorSetAllocateInfo allocate{.sType =
                                                   VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                               .descriptorPool = s.pool,
                                               .descriptorSetCount = 1,
                                               .pSetLayouts = &s.setLayout};
    vkAllocateDescriptorSets(device(), &allocate, &s.set);
    for (int stage = 0; stage < 2; ++stage) {
      if (!s.hasUniforms[stage])
        continue;
      const VkDescriptorBufferInfo buffer{s.uniforms[stage].buffer, 0,
                                          s.uniformLayouts[stage].size};
      const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                       .dstSet = s.set,
                                       .dstBinding = s.uniformLayouts[stage].binding,
                                       .descriptorCount = 1,
                                       .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                       .pBufferInfo = &buffer};
      vkUpdateDescriptorSets(device(), 1, &write, 0, nullptr);
    }
  }
  s.pipeline =
      Vulkan::makePipeline({.layout = s.layout,
                            .vertex = s.vertexModule,
                            .fragment = s.fragmentModule,
                            .attributes = Vulkan::vertexAttributes(),
                            .stride = sizeof(Vertex),
                            .colourMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT});
  if (!s.pipeline)
    error_ = "Vulkan couldn't make the shaders' pipeline";
}

ShaderPanel::~ShaderPanel() = default;

std::uint64_t ShaderPanel::imageId() const {
  // Made for ImGui the first time it's asked for (debug_ui is running by
  // then)
  State &s = *state_;
  if (!s.imageId && s.colour.view && Vulkan::imageRegistry().add)
    s.imageId = Vulkan::imageRegistry().add(s.colour.view);
  return s.imageId;
}

void ShaderPanel::render(float seconds, float mouseX, float mouseY, bool mouseDown, Colour colour) {
  if (!Vulkan::context())
    return;
  State &s = *state_;
  // The uniforms the shaders have, by name, where their blocks put them
  // (the last frame drawn with them is done: submit waits for it first)
  if (s.submitted)
    vkWaitForFences(device(), 1, &s.done, VK_TRUE, UINT64_MAX);
  const float values[][4] = {
      {seconds},
      {static_cast<float>(width_), static_cast<float>(height_)},
      {mouseX, mouseY, mouseDown ? 1.0f : 0.0f, 0.0f},
      {colour.r / 255.0f, colour.g / 255.0f, colour.b / 255.0f, colour.a / 255.0f}};
  const char *names[] = {"iTime", "iResolution", "iMouse", "uniform_color"};
  const std::size_t sizes[] = {1, 2, 4, 4};
  for (int stage = 0; stage < 2; ++stage) {
    if (!s.hasUniforms[stage])
      continue;
    auto *bytes = static_cast<unsigned char *>(s.uniforms[stage].mapped);
    for (int u = 0; u < 4; ++u) {
      const auto &offsets = s.uniformLayouts[stage].offsets;
      const auto found = offsets.find(names[u]);
      if (found != offsets.end() &&
          found->second + sizes[u] * sizeof(float) <= s.uniformLayouts[stage].size)
        std::memcpy(bytes + found->second, values[u], sizes[u] * sizeof(float));
    }
  }

  submit(s, [&](VkCommandBuffer commands) {
    const Vulkan::Use target{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
    Vulkan::transition(commands, s.colour.image, Vulkan::kColourFormat, kShown, target);
    Vulkan::transition(
        commands, s.depth.image, Vulkan::kDepthFormat,
        {VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0},
        {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
    VkRenderingAttachmentInfo colourAttachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                               .imageView = s.colour.view,
                                               .imageLayout =
                                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                               .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                               .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
    colourAttachment.clearValue.color = {{0, 0, 0, 1}};
    VkRenderingAttachmentInfo depthAttachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                              .imageView = s.depth.view,
                                              .imageLayout =
                                                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE};
    depthAttachment.clearValue.depthStencil = {1.0f, 0};
    const VkExtent2D extent{static_cast<std::uint32_t>(width_),
                            static_cast<std::uint32_t>(height_)};
    const VkRenderingInfo rendering{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                    .renderArea = {{0, 0}, extent},
                                    .layerCount = 1,
                                    .colorAttachmentCount = 1,
                                    .pColorAttachments = &colourAttachment,
                                    .pDepthAttachment = &depthAttachment};
    Vulkan::beginRendering(commands, rendering);
    if (ok() && s.vertices.buffer) {
      // Not flipped: row 0 is clip space's y = -1, the bottom of an OpenGL
      // texture, so the texture is laid out as OpenGL's is and gl_FragCoord
      // counts from the same row
      const VkViewport viewport{0,    0,   static_cast<float>(width_), static_cast<float>(height_),
                                0.0f, 1.0f};
      const VkRect2D scissor{{0, 0}, extent};
      vkCmdSetViewport(commands, 0, 1, &viewport);
      vkCmdSetScissor(commands, 0, 1, &scissor);
      vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, s.pipeline);
      if (s.set)
        vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, s.layout, 0, 1, &s.set,
                                0, nullptr);
      const VkDeviceSize start = 0;
      vkCmdBindVertexBuffers(commands, 0, 1, &s.vertices.buffer, &start);
      vkCmdBindIndexBuffer(commands, s.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(commands, s.indexCount, 1, 0, 0, 0);
    }
    Vulkan::endRendering(commands);
    // Read by ImGui in a frame submitted after it, on the same queue
    Vulkan::transition(commands, s.colour.image, Vulkan::kColourFormat, target, kShown);
  });
}

} // namespace ScratchPadd::Scene::Desktop
