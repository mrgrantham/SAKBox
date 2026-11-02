#include "Layer.hpp"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace ScratchPadd::Scene::Metal {

CA::MetalLayer *attachLayer(GLFWwindow *window, MTL::Device *device, bool displaySync) {
  NSWindow *nsWindow = glfwGetCocoaWindow(window);
  CAMetalLayer *layer = [CAMetalLayer layer];
  layer.device = (__bridge id<MTLDevice>)device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly = NO;
  layer.displaySyncEnabled = displaySync;
  layer.contentsScale = nsWindow.backingScaleFactor;
  // Its colours are sRGB, as an OpenGL window's are taken to be, so the
  // screen shows the two backends' colours alike
  CGColorSpaceRef sRGB = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  layer.colorspace = sRGB;
  CGColorSpaceRelease(sRGB);
  nsWindow.contentView.layer = layer;
  nsWindow.contentView.wantsLayer = YES;
  return (__bridge CA::MetalLayer *)layer;
}

} // namespace ScratchPadd::Scene::Metal
