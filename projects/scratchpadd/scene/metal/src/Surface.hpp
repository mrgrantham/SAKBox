// Surface.hpp - what the Metal backend keeps with a window
// (Display::surface): its layer, and the textures a frame is drawn into
#pragma once

#include <ScratchPadd/Scene/Metal/Metal.hpp>

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace ScratchPadd::Scene::Metal {

constexpr MTL::PixelFormat kColourFormat = MTL::PixelFormatBGRA8Unorm;
constexpr MTL::PixelFormat kDepthFormat = MTL::PixelFormatDepth32Float;
// As the OpenGL backend's window has (GLFW_SAMPLES)
constexpr NS::UInteger kSamples = 4;

// A frame is drawn multisampled (`samples`, `depth`), resolved into
// `frame`, and copied from there into the layer's drawable to be shown, or
// in a hidden window into `readback` (Desktop::readPixel). Drawing into a
// texture of its own, not the drawable, lets ImGui draw over the finished
// frame and a hidden window keep it.
struct Surface {
  explicit Surface(CA::MetalLayer *layer) : layer(layer) {}
  ~Surface();
  Surface(const Surface &) = delete;
  Surface &operator=(const Surface &) = delete;

  // Makes the textures `width` x `height` pixels, if they aren't already
  void resize(int width, int height);

  CA::MetalLayer *layer;
  int width = 0, height = 0;
  MTL::Texture *samples = nullptr, *depth = nullptr, *frame = nullptr;
  MTL::Buffer *readback = nullptr; // hidden windows: the last frame, BGRA
  // The pass over the finished frame (Metal.hpp's Frame), and the frame
  // while Display::present runs its last drawings
  MTL::RenderPassDescriptor *overFrame = nullptr;
  Frame current{};
  bool inPresent = false;
};

} // namespace ScratchPadd::Scene::Metal
