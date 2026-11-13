// The Vulkan backend's device (Vulkan.hpp, Device.hpp): Vulkan loaded, one
// instance and device for the program, made when first wanted, with the
// helpers the backend makes everything with
#include "Device.hpp"

#include <ScratchPadd/Scene/MeshData.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>

#if defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>

#include <rules_cc/cc/runfiles/runfiles.h>
#endif

namespace ScratchPadd::Scene::Vulkan {

namespace {

#if defined(__APPLE__)
// This program's path, for finding its runfiles
std::string executablePath() {
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) != 0)
    return "";
  path.resize(std::strlen(path.c_str()));
  return path;
}

// MoltenVK, from the runfiles: Vulkan without a loader
std::string loadMoltenVk() {
  using rules_cc::cc::runfiles::Runfiles;
  std::string error;
  const std::unique_ptr<Runfiles> runfiles(
      Runfiles::Create(executablePath(), BAZEL_CURRENT_REPOSITORY, &error));
  if (!runfiles)
    return "no runfiles to find MoltenVK in: " + error;
  const std::string path =
      runfiles->Rlocation("moltenvk/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib");
  // Its warnings and errors only, unless asked for more (it lists every
  // extension it has at its info level)
  setenv("MVK_CONFIG_LOG_LEVEL", "2", /*overwrite=*/0);
  void *library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!library)
    return std::string("MoltenVK didn't load: ") + dlerror();
  auto *getProcAddress =
      reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
  if (!getProcAddress)
    return "MoltenVK has no vkGetInstanceProcAddr";
  volkInitializeCustom(getProcAddress);
  return "";
}
#endif

bool has(const std::vector<VkExtensionProperties> &extensions, const char *name) {
  return std::ranges::any_of(extensions, [name](const VkExtensionProperties &e) {
    return std::strcmp(e.extensionName, name) == 0;
  });
}

struct Shared {
  Context context{};
  std::string why;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  bool wireframes = false;
  PFN_vkCmdBeginRendering beginRendering = nullptr;
  PFN_vkCmdEndRendering endRendering = nullptr;
};

// The instance, with every window system's surface extension it has
// (GLFW picks one when it makes a window's surface)
VkInstance makeInstance(std::uint32_t apiVersion, std::string &why) {
  std::uint32_t count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> available(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());
  std::vector<const char *> enabled;
  for (const char *name : {"VK_KHR_surface", "VK_KHR_win32_surface", "VK_KHR_xlib_surface",
                           "VK_KHR_xcb_surface", "VK_KHR_wayland_surface", "VK_EXT_metal_surface",
                           "VK_MVK_macos_surface", "VK_KHR_portability_enumeration"})
    if (has(available, name))
      enabled.push_back(name);

  const VkApplicationInfo application{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                      .pApplicationName = "ScratchPadd",
                                      .pEngineName = "ScratchPadd scene layer",
                                      .apiVersion = apiVersion};
  // (MoltenVK and other drivers that aren't all of Vulkan are listed only
  // to programs that say they'll take them)
  const VkInstanceCreateInfo info{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                  .flags = has(available, "VK_KHR_portability_enumeration")
                                               ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
                                               : VkInstanceCreateFlags{0},
                                  .pApplicationInfo = &application,
                                  .enabledExtensionCount =
                                      static_cast<std::uint32_t>(enabled.size()),
                                  .ppEnabledExtensionNames = enabled.data()};
  VkInstance instance = VK_NULL_HANDLE;
  if (const VkResult result = vkCreateInstance(&info, nullptr, &instance); result != VK_SUCCESS)
    why = "vkCreateInstance failed (" + std::to_string(result) + ")";
  return instance;
}

// A device's graphics queue family, or -1
int graphicsFamily(VkPhysicalDevice device) {
  std::uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
  for (std::uint32_t i = 0; i < count; ++i)
    if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      return static_cast<int>(i);
  return -1;
}

std::vector<VkExtensionProperties> deviceExtensions(VkPhysicalDevice device) {
  std::uint32_t count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> extensions(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
  return extensions;
}

Shared makeShared() {
  Shared s;
  if (s.why = loadVulkan(); !s.why.empty())
    return s;
  const std::uint32_t loaderVersion = volkGetInstanceVersion();
  if (loaderVersion < VK_API_VERSION_1_1) {
    s.why = "Vulkan 1.1 or later is needed";
    return s;
  }
  const std::uint32_t apiVersion = std::min(loaderVersion, VK_API_VERSION_1_3);
  Context &c = s.context;
  c.instance = makeInstance(apiVersion, s.why);
  if (!c.instance)
    return s;
  volkLoadInstance(c.instance);

  // The best device that draws and has dynamic rendering: a GPU of its own,
  // then one shared with the CPU, then anything (a software renderer)
  std::uint32_t count = 0;
  vkEnumeratePhysicalDevices(c.instance, &count, nullptr);
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(c.instance, &count, devices.data());
  int bestScore = -1;
  for (VkPhysicalDevice device : devices) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    const bool dynamicRendering =
        properties.apiVersion >= VK_API_VERSION_1_3 ||
        has(deviceExtensions(device), VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    if (properties.apiVersion < VK_API_VERSION_1_1 || !dynamicRendering ||
        graphicsFamily(device) < 0)
      continue;
    const int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 3
                      : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2
                      : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    ? 1
                                                                                        : 0;
    if (score > bestScore) {
      bestScore = score;
      c.physicalDevice = device;
    }
  }
  if (!c.physicalDevice) {
    s.why = "no Vulkan device that can draw (Vulkan 1.1 with dynamic rendering)";
    return s;
  }
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(c.physicalDevice, &properties);
  c.apiVersion = std::min(apiVersion, properties.apiVersion);
  c.queueFamily = static_cast<std::uint32_t>(graphicsFamily(c.physicalDevice));

  const std::vector<VkExtensionProperties> available = deviceExtensions(c.physicalDevice);
  std::vector<const char *> enabled;
  // Dynamic rendering's extension even on 1.3, where it's core, as ImGui's
  // Vulkan backend asks; and what it needs before 1.2
  for (const char *name :
       {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset",
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME})
    if (has(available, name))
      enabled.push_back(name);

  VkPhysicalDeviceFeatures supported;
  vkGetPhysicalDeviceFeatures(c.physicalDevice, &supported);
  s.wireframes = supported.fillModeNonSolid == VK_TRUE;
  VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
      .dynamicRendering = VK_TRUE};
  VkPhysicalDeviceFeatures2 features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                     .pNext = &dynamicRendering};
  features.features.fillModeNonSolid = supported.fillModeNonSolid;
  const float priority = 1.0f;
  const VkDeviceQueueCreateInfo queue{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                      .queueFamilyIndex = c.queueFamily,
                                      .queueCount = 1,
                                      .pQueuePriorities = &priority};
  const VkDeviceCreateInfo info{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                .pNext = &features,
                                .queueCreateInfoCount = 1,
                                .pQueueCreateInfos = &queue,
                                .enabledExtensionCount = static_cast<std::uint32_t>(enabled.size()),
                                .ppEnabledExtensionNames = enabled.data()};
  if (const VkResult result = vkCreateDevice(c.physicalDevice, &info, nullptr, &c.device);
      result != VK_SUCCESS) {
    s.why = "vkCreateDevice failed (" + std::to_string(result) + ")";
    c.device = VK_NULL_HANDLE;
    return s;
  }
  volkLoadDevice(c.device);
  vkGetDeviceQueue(c.device, c.queueFamily, 0, &c.queue);
  s.beginRendering = vkCmdBeginRendering ? vkCmdBeginRendering : vkCmdBeginRenderingKHR;
  s.endRendering = vkCmdEndRendering ? vkCmdEndRendering : vkCmdEndRenderingKHR;

  const VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts &
                                    properties.limits.framebufferDepthSampleCounts;
  s.samples = (counts & VK_SAMPLE_COUNT_4_BIT) ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
  const VkCommandPoolCreateInfo pool{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                     .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                     .queueFamilyIndex = c.queueFamily};
  vkCreateCommandPool(c.device, &pool, nullptr, &s.pool);
  return s;
}

Shared &shared() {
  static Shared s = makeShared();
  return s;
}

const Context &device() { return shared().context; }

std::uint32_t memoryType(std::uint32_t allowed, VkMemoryPropertyFlags wanted) {
  VkPhysicalDeviceMemoryProperties memory;
  vkGetPhysicalDeviceMemoryProperties(device().physicalDevice, &memory);
  for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i)
    if ((allowed & (1u << i)) && (memory.memoryTypes[i].propertyFlags & wanted) == wanted)
      return i;
  // (any it may use, if none has all that's wanted: device-local is only
  // wanted for speed)
  for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i)
    if (allowed & (1u << i))
      return i;
  return 0;
}

VkDeviceMemory allocate(const VkMemoryRequirements &needs, VkMemoryPropertyFlags wanted) {
  const VkMemoryAllocateInfo info{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                  .allocationSize = needs.size,
                                  .memoryTypeIndex = memoryType(needs.memoryTypeBits, wanted)};
  VkDeviceMemory memory = VK_NULL_HANDLE;
  vkAllocateMemory(device().device, &info, nullptr, &memory);
  return memory;
}

VkImageAspectFlags aspectOf(VkFormat format) {
  return format == kDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
}

} // namespace

const std::string &loadVulkan() {
  static const std::string why = []() -> std::string {
#if defined(__APPLE__)
    return loadMoltenVk();
#else
    return volkInitialize() == VK_SUCCESS
               ? ""
               : "no Vulkan loader (libvulkan.so.1 on Linux, vulkan-1.dll on Windows)";
#endif
  }();
  return why;
}

const Context *context() { return shared().context.device ? &shared().context : nullptr; }

const std::string &whyNoVulkan() { return shared().why; }

void beginRendering(VkCommandBuffer commands, const VkRenderingInfo &info) {
  shared().beginRendering(commands, &info);
}

void endRendering(VkCommandBuffer commands) { shared().endRendering(commands); }

ImageRegistry &imageRegistry() {
  static ImageRegistry registry;
  return registry;
}

VkSampleCountFlagBits frameSamples() { return shared().samples; }

bool wireframes() { return shared().wireframes; }

Buffer makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void *contents) {
  Buffer b;
  b.size = std::max<VkDeviceSize>(size, 4);
  const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                .size = b.size,
                                .usage = usage,
                                .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
  vkCreateBuffer(device().device, &info, nullptr, &b.buffer);
  VkMemoryRequirements needs;
  vkGetBufferMemoryRequirements(device().device, b.buffer, &needs);
  b.memory =
      allocate(needs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vkBindBufferMemory(device().device, b.buffer, b.memory, 0);
  vkMapMemory(device().device, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped);
  if (contents && b.mapped)
    std::memcpy(b.mapped, contents, static_cast<std::size_t>(size));
  return b;
}

void destroy(Buffer &b) {
  if (b.buffer) {
    vkDestroyBuffer(device().device, b.buffer, nullptr);
    vkFreeMemory(device().device, b.memory, nullptr); // (unmapped with it)
  }
  b = {};
}

Image makeImage(std::uint32_t width, std::uint32_t height, VkFormat format,
                VkSampleCountFlagBits samples, VkImageUsageFlags usage) {
  Image i;
  const VkImageCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                               .imageType = VK_IMAGE_TYPE_2D,
                               .format = format,
                               .extent = {width, height, 1},
                               .mipLevels = 1,
                               .arrayLayers = 1,
                               .samples = samples,
                               .tiling = VK_IMAGE_TILING_OPTIMAL,
                               .usage = usage,
                               .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                               .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
  vkCreateImage(device().device, &info, nullptr, &i.image);
  VkMemoryRequirements needs;
  vkGetImageMemoryRequirements(device().device, i.image, &needs);
  i.memory = allocate(needs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vkBindImageMemory(device().device, i.image, i.memory, 0);
  const VkImageViewCreateInfo view{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                   .image = i.image,
                                   .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                   .format = format,
                                   .subresourceRange = {aspectOf(format), 0, 1, 0, 1}};
  vkCreateImageView(device().device, &view, nullptr, &i.view);
  return i;
}

void destroy(Image &i) {
  if (i.image) {
    vkDestroyImageView(device().device, i.view, nullptr);
    vkDestroyImage(device().device, i.image, nullptr);
    vkFreeMemory(device().device, i.memory, nullptr);
  }
  i = {};
}

void transition(VkCommandBuffer commands, VkImage image, VkFormat format, const Use &from,
                const Use &to) {
  const VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                     .srcAccessMask = from.access,
                                     .dstAccessMask = to.access,
                                     .oldLayout = from.layout,
                                     .newLayout = to.layout,
                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .image = image,
                                     .subresourceRange = {aspectOf(format), 0, 1, 0, 1}};
  vkCmdPipelineBarrier(commands, from.stages, to.stages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkCommandBuffer makeCommandBuffer() {
  const VkCommandBufferAllocateInfo info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                         .commandPool = shared().pool,
                                         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                         .commandBufferCount = 1};
  VkCommandBuffer commands = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(device().device, &info, &commands);
  return commands;
}

void freeCommandBuffer(VkCommandBuffer commands) {
  if (commands)
    vkFreeCommandBuffers(device().device, shared().pool, 1, &commands);
}

VkFence makeFence(bool signalled) {
  const VkFenceCreateInfo info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                               .flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT
                                                  : VkFenceCreateFlags{0}};
  VkFence fence = VK_NULL_HANDLE;
  vkCreateFence(device().device, &info, nullptr, &fence);
  return fence;
}

VkShaderModule makeShaderModule(std::span<const std::uint32_t> words) {
  const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                      .codeSize = words.size_bytes(),
                                      .pCode = words.data()};
  VkShaderModule module = VK_NULL_HANDLE;
  vkCreateShaderModule(device().device, &info, nullptr, &module);
  return module;
}

VkPipeline makePipeline(const PipelineDescription &d) {
  const VkPipelineShaderStageCreateInfo stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = d.vertex,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = d.fragment,
       .pName = "main"}};
  const VkVertexInputBindingDescription binding{
      .binding = 0, .stride = d.stride, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
  const VkPipelineVertexInputStateCreateInfo input{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding,
      .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(d.attributes.size()),
      .pVertexAttributeDescriptions = d.attributes.data()};
  const VkPipelineInputAssemblyStateCreateInfo assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = d.topology};
  const VkPipelineViewportStateCreateInfo viewport{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};
  // Both sides drawn, or the back culled
  const VkPipelineRasterizationStateCreateInfo rasterization{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = d.polygonMode,
      .cullMode =
          static_cast<VkCullModeFlags>(d.cullBack ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE),
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};
  const VkPipelineMultisampleStateCreateInfo multisample{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = d.samples};
  const VkPipelineDepthStencilStateCreateInfo depth{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = d.depthTest ? VK_TRUE : VK_FALSE,
      .depthWriteEnable = d.depthWrite ? VK_TRUE : VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS};
  const VkPipelineColorBlendAttachmentState blend{
      .blendEnable = d.blend ? VK_TRUE : VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = d.colourMask};
  const VkPipelineColorBlendStateCreateInfo blending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blend};
  const std::array<VkDynamicState, 2> dynamic{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<std::uint32_t>(dynamic.size()),
      .pDynamicStates = dynamic.data()};
  const VkFormat colourFormat = kColourFormat;
  const VkPipelineRenderingCreateInfo rendering{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &colourFormat,
      .depthAttachmentFormat = d.depthFormat};
  const VkGraphicsPipelineCreateInfo info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                          .pNext = &rendering,
                                          .stageCount = 2,
                                          .pStages = stages,
                                          .pVertexInputState = &input,
                                          .pInputAssemblyState = &assembly,
                                          .pViewportState = &viewport,
                                          .pRasterizationState = &rasterization,
                                          .pMultisampleState = &multisample,
                                          .pDepthStencilState = &depth,
                                          .pColorBlendState = &blending,
                                          .pDynamicState = &dynamicState,
                                          .layout = d.layout};
  VkPipeline pipeline = VK_NULL_HANDLE;
  vkCreateGraphicsPipelines(device().device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
  return pipeline;
}

std::span<const VkVertexInputAttributeDescription> vertexAttributes() {
  static const VkVertexInputAttributeDescription attributes[] = {
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex, position))},
      {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex, normal))},
      {2, 0, VK_FORMAT_R8G8B8A8_UNORM, static_cast<std::uint32_t>(offsetof(Vertex, colour))}};
  return attributes;
}

} // namespace ScratchPadd::Scene::Vulkan
