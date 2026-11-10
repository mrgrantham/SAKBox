// Surface.hpp - what the Vulkan backend keeps with a window
// (Display::surface): the images a frame is drawn into, the swapchain it's
// shown with, and the command buffer it's recorded in
#pragma once

#include "Device.hpp"

#include <ScratchPadd/Scene/Desktop/Display.hpp>

#include <vector>

namespace ScratchPadd::Scene::Vulkan {

// A frame is drawn multisampled (`samples`, `depth`), resolved into
// `frame`, and copied from there into the swapchain's next image to be
// shown, or in a hidden window into `readback` (Desktop::readPixel), as the
// Metal backend does. Drawing into an image of its own, not the
// swapchain's, lets ImGui draw over the finished frame and a hidden window
// keep it. One frame is drawn at a time: the next waits for `done`.
struct Surface {
  explicit Surface(Display &display);
  ~Surface();
  Surface(const Surface &) = delete;
  Surface &operator=(const Surface &) = delete;

  // Makes the images `width` x `height` pixels, if they aren't already
  void resize(int width, int height);
  // Waits for the last frame submitted to be drawn
  void waitForLastFrame();
  // The swapchain, made again if the window has changed (shown windows);
  // false if there's none to show a frame in now (minimised)
  bool readySwapchain();

  Display &display;
  int width = 0, height = 0;
  Image samples, depth, frame; // (`samples` unused with one sample)
  Buffer readback;             // hidden windows: the last frame, BGRA
  VkCommandBuffer commands = VK_NULL_HANDLE;
  VkFence done = VK_NULL_HANDLE;
  bool submitted = false;

  // Shown windows
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkExtent2D swapchainExtent{};
  std::vector<VkImage> swapchainImages;
  std::vector<VkSemaphore> rendered; // one per swapchain image
  VkSemaphore acquired = VK_NULL_HANDLE;
  bool swapchainStale = false;

  // The frame while Display::present runs its last drawings
  Frame current{};
  bool inPresent = false;
};

} // namespace ScratchPadd::Scene::Vulkan
