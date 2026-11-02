// The Metal backend's part of a desktop Display (WindowBackend.hpp): a
// window without an OpenGL context, a Metal layer in its view, and the
// textures frames are drawn into (Surface.hpp). The renderer shows each
// frame itself, after the Display's last drawings.
#include "Layer.hpp"
#include "Surface.hpp"

#include <GLFW/glfw3.h>

#include <ScratchPadd/Scene/Desktop/WindowBackend.hpp>

#include <algorithm>
#include <memory>

namespace ScratchPadd::Scene {

namespace Metal {

MTL::Device *device() {
  static MTL::Device *device = MTL::CreateSystemDefaultDevice();
  return device;
}

MTL::CommandQueue *queue() {
  static MTL::CommandQueue *queue = device() ? device()->newCommandQueue() : nullptr;
  return queue;
}

Surface::~Surface() {
  for (NS::Object *object : {static_cast<NS::Object *>(samples), static_cast<NS::Object *>(depth),
                             static_cast<NS::Object *>(frame), static_cast<NS::Object *>(readback),
                             static_cast<NS::Object *>(overFrame)})
    if (object)
      object->release();
}

void Surface::resize(int newWidth, int newHeight) {
  newWidth = std::max(newWidth, 1);
  newHeight = std::max(newHeight, 1);
  if (frame && newWidth == width && newHeight == height)
    return;
  width = newWidth;
  height = newHeight;
  for (MTL::Resource *old :
       {static_cast<MTL::Resource *>(samples), static_cast<MTL::Resource *>(depth),
        static_cast<MTL::Resource *>(frame), static_cast<MTL::Resource *>(readback)})
    if (old)
      old->release();
  readback = nullptr;

  auto texture = [&](MTL::PixelFormat format, NS::UInteger sampleCount, MTL::TextureUsage usage) {
    MTL::TextureDescriptor *d = MTL::TextureDescriptor::alloc()->init();
    d->setTextureType(sampleCount > 1 ? MTL::TextureType2DMultisample : MTL::TextureType2D);
    d->setPixelFormat(format);
    d->setWidth(static_cast<NS::UInteger>(width));
    d->setHeight(static_cast<NS::UInteger>(height));
    d->setSampleCount(sampleCount);
    d->setStorageMode(MTL::StorageModePrivate);
    d->setUsage(usage);
    MTL::Texture *made = device()->newTexture(d);
    d->release();
    return made;
  };
  samples = texture(kColourFormat, kSamples, MTL::TextureUsageRenderTarget);
  depth = texture(kDepthFormat, kSamples, MTL::TextureUsageRenderTarget);
  frame = texture(kColourFormat, 1, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
  layer->setDrawableSize(CGSize{static_cast<CGFloat>(width), static_cast<CGFloat>(height)});

  if (!overFrame)
    overFrame = MTL::RenderPassDescriptor::alloc()->init();
  MTL::RenderPassColorAttachmentDescriptor *colour = overFrame->colorAttachments()->object(0);
  colour->setTexture(frame);
  colour->setLoadAction(MTL::LoadActionLoad);
  colour->setStoreAction(MTL::StoreActionStore);
}

const Frame *currentFrame(const Display &display) {
  const auto *surface = static_cast<const Surface *>(display.surface());
  return surface && surface->inPresent ? &surface->current : nullptr;
}

MTL::RenderPassDescriptor *framePass(const Display &display) {
  auto *surface = static_cast<Surface *>(display.surface());
  if (!surface)
    return nullptr;
  surface->resize(display.pixelWidth(), display.pixelHeight());
  return surface->overFrame;
}

} // namespace Metal

namespace WindowBackend {

const std::string kName = "Metal";

void hint() { glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); }

std::string attach(Display &display) {
  MTL::Device *device = Metal::device();
  if (!device || !Metal::queue())
    return "no Metal device";
  CA::MetalLayer *layer = Metal::attachLayer(display.glfw(), device, display.visible());
  auto surface = std::make_shared<Metal::Surface>(layer);
  surface->resize(display.pixelWidth(), display.pixelHeight());
  display.setSurface(std::move(surface));
  return "";
}

void show(Display &) {} // the renderer shows the frame (Renderer::endFrame)

} // namespace WindowBackend

} // namespace ScratchPadd::Scene
