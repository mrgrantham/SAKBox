// Layer.hpp - attaching a Metal layer to a GLFW window: the one thing the
// backend needs Objective-C for (Layer.mm)
#pragma once

struct GLFWwindow;
namespace CA {
class MetalLayer;
}
namespace MTL {
class Device;
}

namespace ScratchPadd::Scene::Metal {

// Puts a Metal layer for `device` in `window`'s view (BGRA8, its contents
// copied in by the renderer, so not framebuffer-only; sRGB), at the window's
// backing scale, waiting for the screen's refresh if `displaySync`. The
// view keeps it.
CA::MetalLayer *attachLayer(GLFWwindow *window, MTL::Device *device, bool displaySync);

} // namespace ScratchPadd::Scene::Metal
