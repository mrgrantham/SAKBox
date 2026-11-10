// The Vulkan backend's part of a desktop Display (WindowBackend.hpp): a
// window without an OpenGL context, a Vulkan surface and swapchain for it
// if it's shown, and the images frames are drawn into (Surface.hpp). The
// renderer shows each frame itself, after the Display's last drawings.
#include "Surface.hpp"

#include <GLFW/glfw3.h>

#include <ScratchPadd/Scene/Desktop/WindowBackend.hpp>

#include <algorithm>
#include <memory>

namespace ScratchPadd::Scene {

namespace Vulkan {

namespace {
const Context &c() { return *context(); }
} // namespace

Surface::Surface(Display &display) : display(display) {
  commands = makeCommandBuffer();
  done = makeFence(false);
  const VkSemaphoreCreateInfo semaphore{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  vkCreateSemaphore(c().device, &semaphore, nullptr, &acquired);
}

Surface::~Surface() {
  vkDeviceWaitIdle(c().device);
  for (Image *image : {&samples, &depth, &frame})
    destroy(*image);
  destroy(readback);
  for (VkSemaphore semaphore : rendered)
    vkDestroySemaphore(c().device, semaphore, nullptr);
  vkDestroySemaphore(c().device, acquired, nullptr);
  if (swapchain)
    vkDestroySwapchainKHR(c().device, swapchain, nullptr);
  if (surface)
    vkDestroySurfaceKHR(c().instance, surface, nullptr);
  vkDestroyFence(c().device, done, nullptr);
  freeCommandBuffer(commands);
}

void Surface::waitForLastFrame() {
  if (submitted) {
    vkWaitForFences(c().device, 1, &done, VK_TRUE, UINT64_MAX);
    vkResetFences(c().device, 1, &done);
    submitted = false;
  }
}

void Surface::resize(int newWidth, int newHeight) {
  newWidth = std::max(newWidth, 1);
  newHeight = std::max(newHeight, 1);
  if (frame.image && newWidth == width && newHeight == height)
    return;
  waitForLastFrame();
  width = newWidth;
  height = newHeight;
  for (Image *image : {&samples, &depth, &frame})
    destroy(*image);
  destroy(readback);
  const auto w = static_cast<std::uint32_t>(width), h = static_cast<std::uint32_t>(height);
  const VkSampleCountFlagBits count = frameSamples();
  if (count != VK_SAMPLE_COUNT_1_BIT)
    samples =
        makeImage(w, h, kColourFormat, count,
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
  depth = makeImage(w, h, kDepthFormat, count, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  frame = makeImage(w, h, kColourFormat, VK_SAMPLE_COUNT_1_BIT,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  if (!display.visible())
    readback = makeBuffer(VkDeviceSize{w} * h * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  swapchainStale = true;
}

bool Surface::readySwapchain() {
  if (!surface)
    return false;
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(c().physicalDevice, surface, &capabilities);
  VkExtent2D extent = capabilities.currentExtent;
  if (extent.width == UINT32_MAX) // (the surface takes the swapchain's size)
    extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
  if (extent.width == 0 || extent.height == 0)
    return false;
  if (swapchain && !swapchainStale && extent.width == swapchainExtent.width &&
      extent.height == swapchainExtent.height)
    return true;

  vkQueueWaitIdle(c().queue); // (its images may still be being shown)
  std::uint32_t count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(c().physicalDevice, surface, &count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(c().physicalDevice, surface, &count, formats.data());
  // The frame's format, shown as plain sRGB as the other backends show
  // theirs (MoltenVK also offers it in linear and wide colour spaces, which
  // would lighten or tint it)
  VkSurfaceFormatKHR format = formats.empty() ? VkSurfaceFormatKHR{} : formats[0];
  for (const VkSurfaceFormatKHR &f : formats) {
    if (f.format == kColourFormat && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      format = f;
      break;
    }
  }
  VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  for (VkCompositeAlphaFlagBitsKHR a :
       {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR}) {
    if (capabilities.supportedCompositeAlpha & a) {
      alpha = a;
      break;
    }
  }
  std::uint32_t images = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0)
    images = std::min(images, capabilities.maxImageCount);
  // FIFO: each frame waits for the screen's refresh, as the other backends'
  const VkSwapchainKHR old = swapchain;
  const VkSwapchainCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                      .surface = surface,
                                      .minImageCount = images,
                                      .imageFormat = format.format,
                                      .imageColorSpace = format.colorSpace,
                                      .imageExtent = extent,
                                      .imageArrayLayers = 1,
                                      .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                      .preTransform = capabilities.currentTransform,
                                      .compositeAlpha = alpha,
                                      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                      .clipped = VK_TRUE,
                                      .oldSwapchain = old};
  swapchain = VK_NULL_HANDLE;
  const VkResult result = vkCreateSwapchainKHR(c().device, &info, nullptr, &swapchain);
  if (old)
    vkDestroySwapchainKHR(c().device, old, nullptr);
  if (result != VK_SUCCESS) {
    swapchain = VK_NULL_HANDLE;
    return false;
  }
  swapchainExtent = extent;
  swapchainStale = false;
  vkGetSwapchainImagesKHR(c().device, swapchain, &count, nullptr);
  swapchainImages.resize(count);
  vkGetSwapchainImagesKHR(c().device, swapchain, &count, swapchainImages.data());
  const VkSemaphoreCreateInfo semaphore{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  while (rendered.size() < count) {
    rendered.push_back(VK_NULL_HANDLE);
    vkCreateSemaphore(c().device, &semaphore, nullptr, &rendered.back());
  }
  return true;
}

const Frame *currentFrame(const Display &display) {
  const auto *surface = static_cast<const Surface *>(display.surface());
  return surface && surface->inPresent ? &surface->current : nullptr;
}

} // namespace Vulkan

namespace WindowBackend {

const std::string kName = "Vulkan";

void beforeGlfwStarts() {
  // GLFW makes windows' Vulkan surfaces with the functions volk loaded
  if (Vulkan::loadVulkan().empty())
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
}

void hint() { glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); }

std::string attach(Display &display) {
  const Vulkan::Context *context = Vulkan::context();
  if (!context)
    return "no Vulkan: " + Vulkan::whyNoVulkan();
  auto surface = std::make_shared<Vulkan::Surface>(display);
  if (display.visible()) {
    if (const VkResult result =
            glfwCreateWindowSurface(context->instance, display.glfw(), nullptr, &surface->surface);
        result != VK_SUCCESS)
      return "no Vulkan surface for the window (" + std::to_string(result) + ")";
    VkBool32 presents = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(context->physicalDevice, context->queueFamily,
                                         surface->surface, &presents);
    if (!presents)
      return "the Vulkan device can't show frames in this window";
  }
  surface->resize(display.pixelWidth(), display.pixelHeight());
  display.setSurface(std::move(surface));
  return "";
}

void show(Display &) {} // the renderer shows the frame (Renderer::endFrame)

} // namespace WindowBackend

} // namespace ScratchPadd::Scene
