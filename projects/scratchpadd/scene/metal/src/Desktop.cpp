// The desktop extras (Desktop.hpp) on Metal
#include "Backend.hpp"
#include "GlslToMsl.hpp"

#include <ScratchPadd/Scene/Desktop/Desktop.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

namespace ScratchPadd::Scene::Desktop {

namespace {
// The vertices' buffer index in a ShaderPanel's shaders: past where
// SPIRV-Cross puts their uniforms (from 0)
constexpr NS::UInteger kPanelVertices = 30;
} // namespace

void setVertexSnap(Renderer &renderer, float worldGrid, float pixelGrid) {
  renderer.backend().worldSnap = worldGrid;
  renderer.backend().screenSnap = pixelGrid;
}

Colour readPixel(const Renderer &renderer, float x, float y) {
  const Renderer::Backend &b = renderer.backend();
  const Metal::Surface &surface = b.surface;
  if (!surface.readback)
    return {};
  const float scale = static_cast<float>(surface.width) / static_cast<float>(b.display.width());
  const int px = std::clamp(static_cast<int>(x * scale), 0, surface.width - 1);
  const int py = std::clamp(static_cast<int>(y * scale), 0, surface.height - 1);
  // BGRA, top row first
  const auto *bgra = static_cast<const unsigned char *>(surface.readback->contents()) +
                     (static_cast<std::size_t>(py) * surface.width + px) * 4;
  return {bgra[2], bgra[1], bgra[0], bgra[3]};
}

struct ShaderPanel::State {
  ~State() {
    for (NS::Object *object :
         {static_cast<NS::Object *>(texture), static_cast<NS::Object *>(depth),
          static_cast<NS::Object *>(pipeline), static_cast<NS::Object *>(depthState),
          static_cast<NS::Object *>(vertices), static_cast<NS::Object *>(indices)})
      if (object)
        object->release();
  }
  MTL::Texture *texture = nullptr, *depth = nullptr;
  MTL::RenderPipelineState *pipeline = nullptr;
  MTL::DepthStencilState *depthState = nullptr;
  MTL::Buffer *vertices = nullptr, *indices = nullptr;
  NS::UInteger indexCount = 0;
  Metal::TranslatedShader vertex, fragment;
};

ShaderPanel::ShaderPanel(int width, int height, const std::string &vertexSource,
                         const std::string &fragmentSource, const MeshData &mesh)
    : width_(width), height_(height), state_(std::make_unique<State>()) {
  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  State &s = *state_;
  MTL::Device *device = Metal::device();

  // Its texture: RGB as far as anyone sees (alpha is kept at 1), so a panel
  // is never see-through
  auto texture = [&](MTL::PixelFormat format, MTL::TextureUsage usage) {
    MTL::TextureDescriptor *d = MTL::TextureDescriptor::texture2DDescriptor(
        format, static_cast<NS::UInteger>(width), static_cast<NS::UInteger>(height), false);
    d->setStorageMode(MTL::StorageModePrivate);
    d->setUsage(usage);
    return device->newTexture(d);
  };
  s.texture =
      texture(Metal::kColourFormat, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
  s.depth = texture(Metal::kDepthFormat, MTL::TextureUsageRenderTarget);
  if (!mesh.vertices.empty() && !mesh.indices.empty()) {
    s.vertices = device->newBuffer(mesh.vertices.data(), mesh.vertices.size_bytes(),
                                   MTL::ResourceStorageModeShared);
    s.indices = device->newBuffer(mesh.indices.data(), mesh.indices.size_bytes(),
                                  MTL::ResourceStorageModeShared);
    s.indexCount = mesh.indices.size();
  }

  // The GLSL as Metal, compiled
  std::string why;
  if (!Metal::translate(Metal::Stage::vertex, vertexSource, s.vertex, why)) {
    error_ = "vertex shader: " + why;
  } else if (!Metal::translate(Metal::Stage::fragment, fragmentSource, s.fragment, why)) {
    error_ = "fragment shader: " + why;
  } else {
    MTL::Library *vertexLibrary = Metal::compileLibrary(s.vertex.source, why);
    MTL::Library *fragmentLibrary =
        vertexLibrary ? Metal::compileLibrary(s.fragment.source, why) : nullptr;
    if (!vertexLibrary || !fragmentLibrary) {
      error_ = "Metal: " + why;
    } else {
      MTL::RenderPipelineDescriptor *d = MTL::RenderPipelineDescriptor::alloc()->init();
      MTL::Function *vertexFunction = vertexLibrary->newFunction(
          NS::String::string(s.vertex.entryPoint.c_str(), NS::UTF8StringEncoding));
      MTL::Function *fragmentFunction = fragmentLibrary->newFunction(
          NS::String::string(s.fragment.entryPoint.c_str(), NS::UTF8StringEncoding));
      d->setVertexFunction(vertexFunction);
      d->setFragmentFunction(fragmentFunction);
      d->setVertexDescriptor(Metal::vertexLayout(kPanelVertices));
      d->setDepthAttachmentPixelFormat(Metal::kDepthFormat);
      MTL::RenderPipelineColorAttachmentDescriptor *colour = d->colorAttachments()->object(0);
      colour->setPixelFormat(Metal::kColourFormat);
      colour->setWriteMask(MTL::ColorWriteMaskRed | MTL::ColorWriteMaskGreen |
                           MTL::ColorWriteMaskBlue);
      NS::Error *nsError = nullptr;
      s.pipeline = device->newRenderPipelineState(d, &nsError);
      if (!s.pipeline)
        error_ = std::string("Metal pipeline: ") +
                 (nsError ? nsError->localizedDescription()->utf8String() : "unknown");
      if (vertexFunction)
        vertexFunction->release();
      if (fragmentFunction)
        fragmentFunction->release();
      d->release();
    }
    if (vertexLibrary)
      vertexLibrary->release();
    if (fragmentLibrary)
      fragmentLibrary->release();
  }
  MTL::DepthStencilDescriptor *depth = MTL::DepthStencilDescriptor::alloc()->init();
  depth->setDepthCompareFunction(MTL::CompareFunctionLess);
  depth->setDepthWriteEnabled(true);
  s.depthState = device->newDepthStencilState(depth);
  depth->release();
  pool->release();
}

ShaderPanel::~ShaderPanel() = default;

std::uint64_t ShaderPanel::imageId() const {
  return reinterpret_cast<std::uint64_t>(state_->texture);
}

void ShaderPanel::render(float seconds, float mouseX, float mouseY, bool mouseDown, Colour colour) {
  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  const State &s = *state_;
  // On the queue the frame is drawn through, so it's drawn before the frame
  // that shows it
  MTL::CommandBuffer *commands = Metal::queue()->commandBuffer();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *target = pass->colorAttachments()->object(0);
  target->setTexture(s.texture);
  target->setLoadAction(MTL::LoadActionClear);
  target->setStoreAction(MTL::StoreActionStore);
  target->setClearColor(MTL::ClearColor::Make(0, 0, 0, 1));
  MTL::RenderPassDepthAttachmentDescriptor *depth = pass->depthAttachment();
  depth->setTexture(s.depth);
  depth->setLoadAction(MTL::LoadActionClear);
  depth->setStoreAction(MTL::StoreActionDontCare);
  depth->setClearDepth(1.0);
  MTL::RenderCommandEncoder *encoder = commands->renderCommandEncoder(pass);
  if (ok() && s.vertices) {
    encoder->setRenderPipelineState(s.pipeline);
    encoder->setDepthStencilState(s.depthState);
    encoder->setVertexBuffer(s.vertices, 0, kPanelVertices);
    // The uniforms the shaders have, by name, where SPIRV-Cross put them
    const float values[][4] = {
        {seconds},
        {static_cast<float>(width_), static_cast<float>(height_)},
        {mouseX, mouseY, mouseDown ? 1.0f : 0.0f, 0.0f},
        {colour.r / 255.0f, colour.g / 255.0f, colour.b / 255.0f, colour.a / 255.0f}};
    const char *names[] = {"iTime", "iResolution", "iMouse", "uniform_color"};
    const std::size_t sizes[] = {1, 2, 4, 4};
    for (const Metal::TranslatedShader *shader : {&s.vertex, &s.fragment}) {
      if (shader->uniformBuffer < 0 || shader->uniformSize == 0)
        continue;
      std::vector<unsigned char> bytes(shader->uniformSize);
      for (int u = 0; u < 4; ++u) {
        const auto found = shader->uniformOffsets.find(names[u]);
        if (found != shader->uniformOffsets.end() &&
            found->second + sizes[u] * sizeof(float) <= bytes.size())
          std::memcpy(&bytes[found->second], values[u], sizes[u] * sizeof(float));
      }
      const auto index = static_cast<NS::UInteger>(shader->uniformBuffer);
      if (shader == &s.vertex)
        encoder->setVertexBytes(bytes.data(), bytes.size(), index);
      else
        encoder->setFragmentBytes(bytes.data(), bytes.size(), index);
    }
    encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, s.indexCount, MTL::IndexTypeUInt32,
                                   s.indices, 0);
  }
  encoder->endEncoding();
  commands->commit();
  pool->release();
}

} // namespace ScratchPadd::Scene::Desktop
