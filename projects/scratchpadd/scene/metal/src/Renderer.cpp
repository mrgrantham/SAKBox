// Scene::Renderer on Metal, drawing what the OpenGL backend draws, the same
// way: one shader for every material (lit or unlit, vertex colours times the
// material's colour, fog), translucent meshes kept to the end of the frame
// and drawn farthest first, and the overlay drawn last, in screen points.
// Each frame is drawn 4x multisampled into the window's own textures
// (Surface.hpp), then shown.
#include "Backend.hpp"

#include <ScratchPadd/Scene/OverlayText.hpp>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

namespace ScratchPadd::Scene {

namespace {

// The backend's shaders, in Metal's shading language: the OpenGL backend's,
// translated by hand. Frame and Draw match Backend::FrameUniforms and
// DrawUniforms below.
const char *kShaders = R"(
#include <metal_stdlib>
using namespace metal;

struct Frame {
  float4x4 viewProj;
  float2 viewport;
  float2 pad0;
  float4 ambient;
  float4 lightDirections[4];
  float4 lightColours[4];
  float4 fogColour;
  float fogDensity;
  int lightCount;
  float2 pad1;
};

struct Draw {
  float4x4 model;
  float4 normalMatrix[3]; // columns
  float4 colour;          // the material's: colour, and alpha 0-1
  float worldSnap;        // world grid vertices snap to (0: off)
  float screenSnap;       // pixel grid vertices snap to (0: off)
  int lit;
  int toonBands;          // lit in this many bands (toon shading), or 0: smoothly
  float extrude;          // an outline's hull: pushed out this far along the normals (0: not one)
  int outlinePass;        // an outline's hull: the colour alone
  float outlined;         // screen-space outlines: 1 for a mesh that has one
  int textured;           // the material's texture multiplies its colour
};

struct VertexIn {
  float3 position [[attribute(0)]];
  float3 normal [[attribute(1)]];
  float4 colour [[attribute(2)]];
  float2 uv [[attribute(3)]];
};

struct VertexOut {
  float4 position [[position]];
  float3 normal;
  float4 colour;
  float2 uv;
  float distance;
};

vertex VertexOut sceneVertex(VertexIn in [[stage_in]], constant Frame &frame [[buffer(1)]],
                             constant Draw &draw [[buffer(2)]]) {
  float4 world = draw.model * float4(in.position, 1.0);
  float3x3 normalMatrix = float3x3(draw.normalMatrix[0].xyz, draw.normalMatrix[1].xyz,
                                   draw.normalMatrix[2].xyz);
  if (draw.extrude > 0.0)
    world.xyz += normalize(normalMatrix * in.normal) * draw.extrude;
  if (draw.worldSnap > 0.0)
    world.xyz = floor(world.xyz / draw.worldSnap + 0.5) * draw.worldSnap;
  float4 clip = frame.viewProj * world;
  if (draw.screenSnap > 0.0 && clip.w > 0.0) {
    float2 pixel = (clip.xy / clip.w * 0.5 + 0.5) * frame.viewport;
    pixel = floor(pixel / draw.screenSnap + 0.5) * draw.screenSnap;
    clip.xy = (pixel / frame.viewport * 2.0 - 1.0) * clip.w;
  }
  VertexOut out;
  out.position = clip;
  out.normal = normalMatrix * in.normal;
  out.colour = in.colour;
  out.uv = in.uv;
  out.distance = clip.w;
  return out;
}

float4 shade(VertexOut in, constant Frame &frame, constant Draw &draw, texture2d<float> image) {
  constexpr sampler smooth(filter::linear, mip_filter::linear, address::clamp_to_edge);
  float4 texel = draw.textured != 0 && draw.outlinePass == 0 ? image.sample(smooth, in.uv) : float4(1.0);
  float3 colour = draw.outlinePass != 0 ? draw.colour.rgb : in.colour.rgb * draw.colour.rgb * texel.rgb;
  if (draw.lit != 0) {
    float3 n = normalize(in.normal);
    float3 light = frame.ambient.rgb;
    for (int i = 0; i < frame.lightCount; ++i) {
      float lit = max(dot(n, -frame.lightDirections[i].xyz), 0.0);
      if (draw.toonBands > 1) // (as Types.hpp's toonStep)
        lit = clamp(floor(lit * float(draw.toonBands)) / float(draw.toonBands - 1), 0.0, 1.0);
      light += frame.lightColours[i].rgb * lit;
    }
    colour *= light;
  }
  float fog = clamp(1.0 - exp(-frame.fogDensity * in.distance), 0.0, 1.0);
  return float4(mix(colour, frame.fogColour.rgb, fog), in.colour.a * draw.colour.a * texel.a);
}

fragment float4 sceneFragment(VertexOut in [[stage_in]], constant Frame &frame [[buffer(1)]],
                              constant Draw &draw [[buffer(2)]],
                              texture2d<float> image [[texture(0)]]) {
  return shade(in, frame, draw, image);
}

// Screen-space outlines' frames: the colour, and the surface: its direction
// and its distance from the eye, negative where the mesh isn't outlined
struct WithSurface {
  float4 colour [[color(0)]];
  float4 surface [[color(1)]];
};
fragment WithSurface sceneFragmentWithSurface(VertexOut in [[stage_in]],
                                              constant Frame &frame [[buffer(1)]],
                                              constant Draw &draw [[buffer(2)]],
                                              texture2d<float> image [[texture(0)]]) {
  WithSurface out;
  out.colour = shade(in, frame, draw, image);
  out.surface = float4(normalize(in.normal), draw.outlined > 0.5 ? in.distance : -in.distance);
  return out;
}

// The edge pass: the frame with the outline's colour wherever a pixel a few
// away is much nearer or farther, or faces a different way, and either is on
// an outlined mesh (as the OpenGL backend's), fogged at the nearer distance
struct EdgeOut {
  float4 position [[position]];
  float2 uv;
};
vertex EdgeOut edgeVertex(uint id [[vertex_id]]) {
  float2 corner = float2((id << 1) & 2, id & 2);
  EdgeOut out;
  out.position = float4(corner * 2.0 - 1.0, 0.0, 1.0);
  out.uv = float2(corner.x, 1.0 - corner.y);
  return out;
}
struct Edges {
  float4 line, fogColour;
  float2 texel;
  float pixels, fogDensity;
};
fragment float4 edgeFragment(EdgeOut in [[stage_in]], texture2d<float> colour [[texture(0)]],
                             texture2d<float> surfaces [[texture(1)]],
                             constant Edges &edges [[buffer(0)]]) {
  constexpr sampler nearest(filter::nearest, address::clamp_to_edge);
  float4 here = surfaces.sample(nearest, in.uv);
  float distance = here.w == 0.0 ? 1e9 : abs(here.w), nearestDistance = distance;
  bool edge = false;
  const float2 steps[4] = {float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1)};
  for (int i = 0; i < 4; ++i) {
    float4 there = surfaces.sample(nearest, in.uv + steps[i] * edges.texel * edges.pixels);
    if (here.w <= 0.0 && there.w <= 0.0)
      continue; // (neither outlined)
    float thereDistance = there.w == 0.0 ? 1e9 : abs(there.w);
    bool step = abs(thereDistance - distance) > 0.05 * min(thereDistance, distance);
    bool crease = dot(here.xyz, there.xyz) < 0.85; // (about 30 degrees)
    if (step || crease) {
      edge = true;
      nearestDistance = min(nearestDistance, thereDistance);
    }
  }
  float fog = clamp(1.0 - exp(-edges.fogDensity * nearestDistance), 0.0, 1.0);
  float3 line = mix(edges.line.rgb, edges.fogColour.rgb, fog);
  return float4(edge ? line : colour.sample(nearest, in.uv).rgb, 1.0);
}

struct OverlayIn {
  float2 point [[attribute(0)]];
  float4 colour [[attribute(1)]];
};

struct OverlayOut {
  float4 position [[position]];
  float4 colour;
};

vertex OverlayOut overlayVertex(OverlayIn in [[stage_in]], constant float2 &screen [[buffer(1)]]) {
  OverlayOut out;
  out.position = float4(in.point.x / screen.x * 2.0 - 1.0, 1.0 - in.point.y / screen.y * 2.0, 0.0, 1.0);
  out.colour = in.colour;
  return out;
}

fragment float4 overlayFragment(OverlayOut in [[stage_in]]) { return in.colour; }
)";

// Buffers' indices in the scene shaders: the vertices, then Frame and Draw
constexpr NS::UInteger kVerticesBuffer = 0, kFrameBuffer = 1, kDrawBuffer = 2;

// The shaders' Draw
struct DrawUniforms {
  float model[16];
  float normalMatrix[3][4];
  float colour[4];
  float worldSnap, screenSnap;
  int lit;
  int toonBands;
  float extrude;
  int outlinePass;
  float outlined;
  int textured;
};

// The edge pass's uniforms (the shaders' Edges)
struct EdgeUniforms {
  float line[4], fogColour[4];
  float texel[2];
  float pixels, fogDensity;
};
constexpr MTL::PixelFormat kSurfaceFormat = MTL::PixelFormatRGBA16Float;

glm::vec3 toGlm(const Vector3 &v) { return {v.x, v.y, v.z}; }
glm::vec3 toGlm(const Colour &c) { return glm::vec3(c.r, c.g, c.b) / 255.0f; }

glm::mat4 toGlm(const Transform &t) {
  glm::mat4 m(1.0f);
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      m[column][row] = t.m[row][column]; // glm is column-major, as Metal is
  return m;
}

NS::String *string(const char *text) { return NS::String::string(text, NS::UTF8StringEncoding); }

MTL::RenderPipelineState *makePipeline(MTL::Library *library, const char *vertex,
                                       const char *fragment, MTL::VertexDescriptor *layout,
                                       bool blend, bool withSurface = false) {
  MTL::RenderPipelineDescriptor *d = MTL::RenderPipelineDescriptor::alloc()->init();
  MTL::Function *vertexFunction = library->newFunction(string(vertex));
  MTL::Function *fragmentFunction = library->newFunction(string(fragment));
  d->setVertexFunction(vertexFunction);
  d->setFragmentFunction(fragmentFunction);
  if (layout)
    d->setVertexDescriptor(layout);
  d->setRasterSampleCount(Metal::kSamples);
  d->setDepthAttachmentPixelFormat(Metal::kDepthFormat);
  MTL::RenderPipelineColorAttachmentDescriptor *colour = d->colorAttachments()->object(0);
  colour->setPixelFormat(Metal::kColourFormat);
  if (blend) {
    // As OpenGL's glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    colour->setBlendingEnabled(true);
    colour->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    colour->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
    colour->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    colour->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
  }
  if (withSurface) {
    // Screen-space outlines' second target (see-through meshes leave it)
    MTL::RenderPipelineColorAttachmentDescriptor *surface = d->colorAttachments()->object(1);
    surface->setPixelFormat(kSurfaceFormat);
    if (blend)
      surface->setWriteMask(MTL::ColorWriteMaskNone);
  }
  NS::Error *error = nullptr;
  MTL::RenderPipelineState *pipeline = Metal::device()->newRenderPipelineState(d, &error);
  if (!pipeline)
    std::fprintf(stderr, "Metal pipeline error: %s\n",
                 error ? error->localizedDescription()->utf8String() : "unknown");
  vertexFunction->release();
  fragmentFunction->release();
  d->release();
  return pipeline;
}

MTL::DepthStencilState *makeDepthState(MTL::CompareFunction compare, bool write) {
  MTL::DepthStencilDescriptor *d = MTL::DepthStencilDescriptor::alloc()->init();
  d->setDepthCompareFunction(compare);
  d->setDepthWriteEnabled(write);
  MTL::DepthStencilState *state = Metal::device()->newDepthStencilState(d);
  d->release();
  return state;
}

} // namespace

namespace Metal {

MTL::Library *compileLibrary(const std::string &source, std::string &error) {
  NS::Error *nsError = nullptr;
  MTL::Library *library = device()->newLibrary(string(source.c_str()), nullptr, &nsError);
  if (!library)
    error = nsError ? nsError->localizedDescription()->utf8String() : "Metal couldn't compile it";
  return library;
}

MTL::VertexDescriptor *vertexLayout(NS::UInteger buffer) {
  MTL::VertexDescriptor *layout = MTL::VertexDescriptor::vertexDescriptor();
  auto attribute = [&](NS::UInteger index, MTL::VertexFormat format, std::size_t offset) {
    MTL::VertexAttributeDescriptor *a = layout->attributes()->object(index);
    a->setFormat(format);
    a->setOffset(offset);
    a->setBufferIndex(buffer);
  };
  attribute(0, MTL::VertexFormatFloat3, offsetof(Vertex, position));
  attribute(1, MTL::VertexFormatFloat3, offsetof(Vertex, normal));
  attribute(2, MTL::VertexFormatUChar4Normalized, offsetof(Vertex, colour));
  attribute(3, MTL::VertexFormatFloat2, offsetof(Vertex, uv));
  layout->layouts()->object(buffer)->setStride(sizeof(Vertex));
  return layout;
}

} // namespace Metal

namespace {
// A texture of `image` (RGBA, rows from the top), with its mipmaps made by
// the GPU, as the OpenGL backend's
MTL::Texture *textureOf(const TextureImage &image) {
  MTL::TextureDescriptor *describe = MTL::TextureDescriptor::texture2DDescriptor(
      MTL::PixelFormatRGBA8Unorm, static_cast<NS::UInteger>(image.width),
      static_cast<NS::UInteger>(image.height), true);
  describe->setUsage(MTL::TextureUsageShaderRead);
  MTL::Texture *texture = Metal::device()->newTexture(describe);
  texture->replaceRegion(MTL::Region(0, 0, static_cast<NS::UInteger>(image.width),
                                     static_cast<NS::UInteger>(image.height)),
                         0, image.pixels.data(), static_cast<NS::UInteger>(image.width) * 4);
  if (texture->mipmapLevelCount() > 1) {
    MTL::CommandBuffer *commands = Metal::queue()->commandBuffer();
    MTL::BlitCommandEncoder *blit = commands->blitCommandEncoder();
    blit->generateMipmaps(texture);
    blit->endEncoding();
    commands->commit();
    commands->waitUntilCompleted();
  }
  return texture;
}
} // namespace

Renderer::Backend::Backend(Display &display)
    : display(display), surface(*static_cast<Metal::Surface *>(display.surface())) {
  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  if (Metal::device()) {
    const Colour white;
    blank = textureOf(TextureImage{1, 1, std::span<const Colour>(&white, 1)});
  }
  std::string error;
  MTL::Library *library = Metal::compileLibrary(kShaders, error);
  if (!library) {
    std::fprintf(stderr, "Metal shader error: %s\n", error.c_str());
  } else {
    MTL::VertexDescriptor *sceneLayout = Metal::vertexLayout(kVerticesBuffer);
    solid = makePipeline(library, "sceneVertex", "sceneFragment", sceneLayout, false);
    translucent = makePipeline(library, "sceneVertex", "sceneFragment", sceneLayout, true);
    solidWithSurface =
        makePipeline(library, "sceneVertex", "sceneFragmentWithSurface", sceneLayout, false, true);
    translucentWithSurface =
        makePipeline(library, "sceneVertex", "sceneFragmentWithSurface", sceneLayout, true, true);
    edgePipeline = makePipeline(library, "edgeVertex", "edgeFragment", nullptr, false);
    MTL::VertexDescriptor *overlayLayout = MTL::VertexDescriptor::vertexDescriptor();
    MTL::VertexAttributeDescriptor *point = overlayLayout->attributes()->object(0);
    point->setFormat(MTL::VertexFormatFloat2);
    point->setOffset(offsetof(Overlay::Vertex, x));
    point->setBufferIndex(kVerticesBuffer);
    MTL::VertexAttributeDescriptor *colour = overlayLayout->attributes()->object(1);
    colour->setFormat(MTL::VertexFormatUChar4Normalized);
    colour->setOffset(offsetof(Overlay::Vertex, colour));
    colour->setBufferIndex(kVerticesBuffer);
    overlayLayout->layouts()->object(kVerticesBuffer)->setStride(sizeof(Overlay::Vertex));
    overlayPipeline =
        makePipeline(library, "overlayVertex", "overlayFragment", overlayLayout, true);
    library->release();
  }
  depthWrite = makeDepthState(MTL::CompareFunctionLess, true);
  depthTest = makeDepthState(MTL::CompareFunctionLess, false);
  noDepth = makeDepthState(MTL::CompareFunctionAlways, false);
  pool->release();
}

Renderer::Backend::~Backend() {
  if (commands) {
    commands->waitUntilCompleted();
    commands->release();
  }
  for (MTL::Buffer *buffer : buffers)
    buffer->release();
  for (MTL::Texture *texture : textures)
    if (texture)
      texture->release();
  if (blank)
    blank->release();
  for (const LoadedMesh &mesh : meshes) {
    if (mesh.vertices) {
      mesh.vertices->release();
      mesh.indices->release();
      mesh.hullVertices->release();
    }
  }
  for (NS::Object *object :
       {static_cast<NS::Object *>(solid), static_cast<NS::Object *>(translucent),
        static_cast<NS::Object *>(overlayPipeline), static_cast<NS::Object *>(depthWrite),
        static_cast<NS::Object *>(depthTest), static_cast<NS::Object *>(noDepth),
        static_cast<NS::Object *>(solidWithSurface),
        static_cast<NS::Object *>(translucentWithSurface), static_cast<NS::Object *>(edgePipeline),
        static_cast<NS::Object *>(edgeColour), static_cast<NS::Object *>(surfaces),
        static_cast<NS::Object *>(surfaceSamples)})
    if (object)
      object->release();
}

MTL::Buffer *Renderer::Backend::frameBuffer(const void *bytes, std::size_t length) {
  MTL::Buffer *buffer = Metal::device()->newBuffer(bytes, length, MTL::ResourceStorageModeShared);
  buffers.push_back(buffer);
  return buffer;
}

void Renderer::Backend::drawNow(Mesh mesh, const Transform &where, const Material &material) {
  if (!encoder || !solid || !mesh || mesh.id > meshes.size() || !meshes[mesh.id - 1].vertices)
    return;
  const LoadedMesh &loaded = meshes[mesh.id - 1];
  const bool seeThrough = material.alpha < 31 || loaded.translucent;
  if (screenEdges)
    encoder->setRenderPipelineState(seeThrough ? translucentWithSurface : solidWithSurface);
  else
    encoder->setRenderPipelineState(seeThrough ? translucent : solid);
  encoder->setDepthStencilState(seeThrough ? depthTest : depthWrite);
  DrawUniforms draw{};
  const glm::mat4 model = toGlm(where);
  std::memcpy(draw.model, glm::value_ptr(model), sizeof(draw.model));
  const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));
  for (int column = 0; column < 3; ++column)
    for (int row = 0; row < 3; ++row)
      draw.normalMatrix[column][row] = normalMatrix[column][row];
  const glm::vec3 colour = toGlm(material.colour);
  draw.colour[0] = colour.r;
  draw.colour[1] = colour.g;
  draw.colour[2] = colour.b;
  draw.colour[3] = static_cast<float>(std::min<int>(material.alpha, 31)) / 31.0f;
  draw.lit = material.shading == Material::Shading::Unlit ? 0 : 1;
  draw.toonBands =
      material.shading == Material::Shading::Toon ? toonBandsFor(material, environment) : 0;
  draw.outlined = material.outline ? 1.0f : 0.0f;
  draw.worldSnap = worldSnap;
  draw.screenSnap = screenSnap;
  // Its texture (a 1 x 1 white one stands in for none, so the shader always
  // has one to sample)
  MTL::Texture *image = blank;
  if (material.texture && material.texture.id <= textures.size() &&
      textures[material.texture.id - 1]) {
    image = textures[material.texture.id - 1];
    draw.textured = 1;
  }
  encoder->setFragmentTexture(image, 0);
  encoder->setVertexBytes(&draw, sizeof(draw), kDrawBuffer);
  encoder->setFragmentBytes(&draw, sizeof(draw), kDrawBuffer);
  encoder->setVertexBuffer(loaded.vertices, 0, kVerticesBuffer);
  if (material.wireframe)
    encoder->setTriangleFillMode(MTL::TriangleFillModeLines);
  // Back faces culled (counter-clockwise is the front, as OpenGL has it),
  // unless either side shows: a double-sided mesh or material, or a wireframe
  if (!loaded.doubleSided && !material.doubleSided && !material.wireframe) {
    encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
    encoder->setCullMode(MTL::CullModeBack);
  }
  encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, loaded.indexCount,
                                 MTL::IndexTypeUInt32, loaded.indices, 0);
  encoder->setCullMode(MTL::CullModeNone);
  if (material.wireframe)
    encoder->setTriangleFillMode(MTL::TriangleFillModeFill);
  counting += loaded.cost;
  ++counting.draws;
  // The outline: the inverted hull, the mesh again pushed out along its
  // shared normals, front faces culled so only the rim shows, one colour
  if (material.outline && !material.wireframe && loaded.hullVertices &&
      environment.outline.style == Outline::Style::Hull) {
    const glm::vec3 line = toGlm(material.outlineColour.value_or(environment.outline.colour));
    draw.colour[0] = line.r;
    draw.colour[1] = line.g;
    draw.colour[2] = line.b;
    draw.lit = 0;
    draw.outlinePass = 1;
    draw.extrude = environment.outline.width;
    encoder->setVertexBytes(&draw, sizeof(draw), kDrawBuffer);
    encoder->setFragmentBytes(&draw, sizeof(draw), kDrawBuffer);
    encoder->setVertexBuffer(loaded.hullVertices, 0, kVerticesBuffer);
    encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
    encoder->setCullMode(MTL::CullModeFront);
    encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, loaded.indexCount,
                                   MTL::IndexTypeUInt32, loaded.indices, 0);
    encoder->setCullMode(MTL::CullModeNone);
    counting += loaded.cost;
    ++counting.draws;
  }
}

// Screen-space outlines' textures at the frame's size: the frame's colour
// and the surfaces (resolved), and the surfaces' multisampled target
void Renderer::Backend::makeEdgeTextures(int width, int height) {
  if (edgeColour && static_cast<int>(edgeColour->width()) == width &&
      static_cast<int>(edgeColour->height()) == height)
    return;
  for (MTL::Texture *t : {edgeColour, surfaces, surfaceSamples})
    if (t)
      t->release();
  const auto make = [&](MTL::PixelFormat format, bool multisampled) {
    MTL::TextureDescriptor *d = MTL::TextureDescriptor::texture2DDescriptor(
        format, static_cast<NS::UInteger>(width), static_cast<NS::UInteger>(height), false);
    d->setStorageMode(MTL::StorageModePrivate);
    d->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    if (multisampled) {
      d->setTextureType(MTL::TextureType2DMultisample);
      d->setSampleCount(Metal::kSamples);
      d->setUsage(MTL::TextureUsageRenderTarget);
    }
    return Metal::device()->newTexture(d);
  };
  edgeColour = make(Metal::kColourFormat, false);
  surfaces = make(kSurfaceFormat, false);
  surfaceSamples = make(kSurfaceFormat, true);
}

// The edge pass: the scene's pass ends, and a new one draws the frame with
// its outlines into the frame's samples, resolved into the frame as usual
// (the overlay then goes over it in the same pass)
void Renderer::Backend::drawEdges() {
  encoder->endEncoding();
  encoder->release();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(surface.samples);
  colour->setResolveTexture(surface.frame);
  colour->setLoadAction(MTL::LoadActionDontCare);
  colour->setStoreAction(MTL::StoreActionMultisampleResolve);
  MTL::RenderPassDepthAttachmentDescriptor *depth = pass->depthAttachment();
  depth->setTexture(surface.depth);
  depth->setLoadAction(MTL::LoadActionClear);
  depth->setStoreAction(MTL::StoreActionDontCare);
  encoder = commands->renderCommandEncoder(pass)->retain();
  if (!edgePipeline)
    return;
  encoder->setRenderPipelineState(edgePipeline);
  encoder->setDepthStencilState(noDepth);
  EdgeUniforms u{};
  const glm::vec3 line = toGlm(environment.outline.colour), fog = toGlm(environment.fog.colour);
  u.line[0] = line.r, u.line[1] = line.g, u.line[2] = line.b;
  u.fogColour[0] = fog.r, u.fogColour[1] = fog.g, u.fogColour[2] = fog.b;
  u.texel[0] = 1.0f / static_cast<float>(edgeColour->width());
  u.texel[1] = 1.0f / static_cast<float>(edgeColour->height());
  u.pixels = static_cast<float>(std::max<int>(environment.outline.pixels, 1));
  u.fogDensity = environment.fog.density;
  encoder->setFragmentBytes(&u, sizeof(u), 0);
  encoder->setFragmentTexture(edgeColour, 0);
  encoder->setFragmentTexture(surfaces, 1);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
}

void Renderer::Backend::drawOverlay(const Overlay &overlay) {
  // The shapes, then the text over them
  overlayTriangles.assign(overlay.triangles().begin(), overlay.triangles().end());
  for (const Overlay::Text &text : overlay.texts())
    appendTextTriangles(text, overlayTriangles);
  if (overlayTriangles.empty() || !overlayPipeline)
    return;
  encoder->setRenderPipelineState(overlayPipeline);
  encoder->setDepthStencilState(noDepth);
  const float screen[2] = {static_cast<float>(display.width()),
                           static_cast<float>(display.height())};
  encoder->setVertexBytes(screen, sizeof(screen), kFrameBuffer);
  encoder->setVertexBuffer(
      frameBuffer(overlayTriangles.data(), overlayTriangles.size() * sizeof(Overlay::Vertex)), 0,
      kVerticesBuffer);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), overlayTriangles.size());
}

Renderer::Renderer(Display &display) : backend_(std::make_unique<Backend>(display)) {}

Renderer::~Renderer() = default;

Mesh Renderer::load(const MeshData &mesh) {
  Backend::LoadedMesh loaded;
  const std::vector<std::uint32_t> indices = triangleList(mesh);
  if (!mesh.vertices.empty() && !indices.empty()) {
    loaded.vertices = Metal::device()->newBuffer(mesh.vertices.data(), mesh.vertices.size_bytes(),
                                                 MTL::ResourceStorageModeShared);
    loaded.indices = Metal::device()->newBuffer(
        indices.data(), indices.size() * sizeof(std::uint32_t), MTL::ResourceStorageModeShared);
    loaded.indexCount = indices.size();
    // Its outline's hull: the same indices, the normals shared (OutlineHull.hpp)
    const std::vector<Vertex> hull = outlineHullVertices(mesh.vertices);
    loaded.hullVertices = Metal::device()->newBuffer(hull.data(), hull.size() * sizeof(Vertex),
                                                     MTL::ResourceStorageModeShared);
    loaded.translucent = hasTranslucentVertices(mesh);
    loaded.cost = costOf(mesh);
    loaded.bounds = boundsOf(mesh);
    loaded.doubleSided = mesh.doubleSided;
  }
  backend_->meshes.push_back(loaded);
  return Mesh{static_cast<std::uint32_t>(backend_->meshes.size())};
}

Texture Renderer::load(const TextureImage &image) {
  if (image.width <= 0 || image.height <= 0 ||
      image.pixels.size() <
          static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height))
    return {};
  backend_->textures.push_back(textureOf(image));
  return {static_cast<std::uint32_t>(backend_->textures.size())};
}

bool Renderer::replace(Texture texture, const TextureImage &image) {
  if (!texture || texture.id > backend_->textures.size() || !backend_->textures[texture.id - 1] ||
      image.width <= 0 || image.height <= 0 ||
      image.pixels.size() <
          static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height))
    return false;
  unload(texture);
  backend_->textures[texture.id - 1] = textureOf(image);
  return true;
}

void Renderer::unload(Texture texture) {
  if (!texture || texture.id > backend_->textures.size())
    return;
  MTL::Texture *&loaded = backend_->textures[texture.id - 1];
  if (!loaded)
    return;
  if (backend_->commands) // (a frame still being drawn may use it)
    backend_->commands->waitUntilCompleted();
  loaded->release();
  loaded = nullptr;
}

void Renderer::unload(Mesh mesh) {
  if (!mesh || mesh.id > backend_->meshes.size())
    return;
  Backend::LoadedMesh &loaded = backend_->meshes[mesh.id - 1];
  if (!loaded.vertices)
    return;
  // A frame still being drawn may use it
  if (backend_->commands)
    backend_->commands->waitUntilCompleted();
  loaded.vertices->release();
  loaded.indices->release();
  loaded.hullVertices->release();
  loaded = {};
}

void Renderer::beginFrame(const Camera &camera, const Environment &environment) {
  Backend &b = *backend_;
  // One frame drawn at a time: the last one's buffers go once it's done
  if (b.commands) {
    b.commands->waitUntilCompleted();
    b.commands->release();
    b.commands = nullptr;
  }
  for (MTL::Buffer *buffer : b.buffers)
    buffer->release();
  b.buffers.clear();
  b.later.clear();
  b.counting = {};
  b.environment = environment;
  b.worldSnap = b.screenSnap = 0;
  overlay_.clear();

  const int width = b.display.pixelWidth(), height = b.display.pixelHeight();
  b.surface.resize(width, height);
  b.commands = Metal::queue()->commandBuffer()->retain();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(b.surface.samples);
  // Screen-space outlines: the frame resolved into a texture of its own for
  // the edge pass (endFrame), with the surfaces' as the second target
  b.screenEdges = environment.outline.style == Outline::Style::ScreenSpace;
  if (b.screenEdges) {
    b.makeEdgeTextures(width, height);
    colour->setResolveTexture(b.edgeColour);
    MTL::RenderPassColorAttachmentDescriptor *surface = pass->colorAttachments()->object(1);
    surface->setTexture(b.surfaceSamples);
    surface->setResolveTexture(b.surfaces);
    surface->setLoadAction(MTL::LoadActionClear);
    surface->setStoreAction(MTL::StoreActionMultisampleResolve);
    surface->setClearColor(MTL::ClearColor::Make(0, 0, 0, 0)); // (no surface)
  } else {
    colour->setResolveTexture(b.surface.frame);
  }
  colour->setLoadAction(MTL::LoadActionClear);
  colour->setStoreAction(MTL::StoreActionMultisampleResolve);
  const glm::vec3 background = toGlm(environment.backgroundSeen(camera.farPlane));
  colour->setClearColor(MTL::ClearColor::Make(background.r, background.g, background.b, 1.0));
  MTL::RenderPassDepthAttachmentDescriptor *depth = pass->depthAttachment();
  depth->setTexture(b.surface.depth);
  depth->setLoadAction(MTL::LoadActionClear);
  depth->setStoreAction(MTL::StoreActionDontCare);
  depth->setClearDepth(1.0);
  b.encoder = b.commands->renderCommandEncoder(pass)->retain();

  b.eye = toGlm(camera.eye);
  const glm::mat4 view = glm::lookAt(b.eye, toGlm(camera.target), toGlm(camera.up));
  // Metal's depth goes from 0 to 1, not OpenGL's -1 to 1
  const glm::mat4 projection =
      glm::perspectiveRH_ZO(glm::radians(camera.fieldOfView),
                            static_cast<float>(width) / static_cast<float>(std::max(height, 1)),
                            camera.nearPlane, camera.farPlane);
  b.viewProj = projection * view;
  b.frustum.emplace(camera, static_cast<float>(width) / static_cast<float>(std::max(height, 1)));
  Backend::FrameUniforms &f = b.frameUniforms;
  f = {};
  std::memcpy(f.viewProj, glm::value_ptr(b.viewProj), sizeof(f.viewProj));
  f.viewport[0] = static_cast<float>(width);
  f.viewport[1] = static_cast<float>(height);
  const glm::vec3 ambient = toGlm(environment.ambient);
  f.ambient[0] = ambient.r;
  f.ambient[1] = ambient.g;
  f.ambient[2] = ambient.b;
  for (unsigned i = 0; i < environment.lightCount; ++i) {
    const glm::vec3 direction = glm::normalize(toGlm(environment.lights[i].direction));
    const glm::vec3 lightColour = toGlm(environment.lights[i].colour);
    for (int c = 0; c < 3; ++c) {
      f.lightDirections[i][c] = direction[c];
      f.lightColours[i][c] = lightColour[c];
    }
  }
  f.lightCount = static_cast<int>(environment.lightCount);
  const glm::vec3 fog = toGlm(environment.fog.colour);
  f.fogColour[0] = fog.r;
  f.fogColour[1] = fog.g;
  f.fogColour[2] = fog.b;
  f.fogDensity = environment.fog.density;
  b.encoder->setVertexBytes(&f, sizeof(f), kFrameBuffer);
  b.encoder->setFragmentBytes(&f, sizeof(f), kFrameBuffer);
}

void Renderer::draw(Mesh mesh, const Transform &where, const Material &material) {
  const auto &meshes = backend_->meshes;
  // Wholly out of view: not drawn (Culling.hpp)
  if (mesh && mesh.id <= meshes.size() && backend_->frustum &&
      backend_->frustum->outside(meshes[mesh.id - 1].bounds, where)) {
    ++backend_->counting.culled;
    return;
  }
  if (material.alpha < 31 ||
      (mesh && mesh.id <= meshes.size() && meshes[mesh.id - 1].translucent)) {
    const glm::vec3 position = toGlm(where.position());
    backend_->later.push_back({mesh, where, material, glm::length(position - backend_->eye)});
    return;
  }
  backend_->drawNow(mesh, where, material);
}

void Renderer::drawLines(std::span<const Vector3> ends, Colour colour) {
  Backend &b = *backend_;
  if (ends.size() < 2 || !b.encoder || !b.solid)
    return;
  std::vector<Vertex> vertices;
  vertices.reserve(ends.size());
  for (const Vector3 &end : ends)
    vertices.push_back({end, {0, 1, 0}, Colour{}});
  const std::size_t count = vertices.size() / 2 * 2;
  b.encoder->setRenderPipelineState(b.solid);
  b.encoder->setDepthStencilState(b.depthWrite);
  DrawUniforms draw{};
  const glm::mat4 identity(1.0f);
  std::memcpy(draw.model, glm::value_ptr(identity), sizeof(draw.model));
  for (int i = 0; i < 3; ++i)
    draw.normalMatrix[i][i] = 1;
  const glm::vec3 c = toGlm(colour);
  draw.colour[0] = c.r;
  draw.colour[1] = c.g;
  draw.colour[2] = c.b;
  draw.colour[3] = colour.a / 255.0f;
  draw.worldSnap = b.worldSnap;
  draw.screenSnap = b.screenSnap;
  b.encoder->setVertexBytes(&draw, sizeof(draw), kDrawBuffer);
  b.encoder->setFragmentBytes(&draw, sizeof(draw), kDrawBuffer);
  b.encoder->setFragmentTexture(b.blank, 0); // (lines aren't textured)
  b.encoder->setVertexBuffer(b.frameBuffer(vertices.data(), vertices.size() * sizeof(Vertex)), 0,
                             kVerticesBuffer);
  b.encoder->drawPrimitives(MTL::PrimitiveTypeLine, NS::UInteger(0), count);
  b.counting.polygons += static_cast<unsigned>(count / 2);
  b.counting.vertices += static_cast<unsigned>(count / 2 * 3);
  ++b.counting.draws;
}

void Renderer::endFrame() {
  Backend &b = *backend_;
  if (!b.encoder)
    return;
  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  // Translucent meshes over the solid ones, farthest first, each blending
  // over what's behind it without hiding what's drawn after
  std::ranges::stable_sort(b.later, [](const Backend::Later &x, const Backend::Later &y) {
    return x.distance > y.distance;
  });
  for (const Backend::Later &item : b.later)
    b.drawNow(item.mesh, item.where, item.material);
  b.later.clear();
  if (b.screenEdges)
    b.drawEdges();
  b.drawOverlay(overlay_);
  overlay_.clear();
  b.encoder->endEncoding();
  b.encoder->release();
  b.encoder = nullptr;

  // The display's last drawings (ImGui) over the finished frame
  b.surface.current = {b.commands, b.surface.overFrame};
  b.surface.inPresent = true;
  b.display.present();
  b.surface.inPresent = false;

  // Shown: copied into the layer's next drawable, which is presented at the
  // screen's refresh. Hidden: copied where Desktop::readPixel reads it.
  MTL::BlitCommandEncoder *blit = b.commands->blitCommandEncoder();
  const MTL::Size size = MTL::Size::Make(static_cast<NS::UInteger>(b.surface.width),
                                         static_cast<NS::UInteger>(b.surface.height), 1);
  CA::MetalDrawable *drawable = nullptr;
  if (b.display.visible()) {
    drawable = b.surface.layer->nextDrawable();
    if (drawable && drawable->texture()->width() == size.width &&
        drawable->texture()->height() == size.height)
      blit->copyFromTexture(b.surface.frame, 0, 0, MTL::Origin::Make(0, 0, 0), size,
                            drawable->texture(), 0, 0, MTL::Origin::Make(0, 0, 0));
  } else {
    const NS::UInteger rowBytes = size.width * 4;
    if (!b.surface.readback)
      b.surface.readback =
          Metal::device()->newBuffer(rowBytes * size.height, MTL::ResourceStorageModeShared);
    blit->copyFromTexture(b.surface.frame, 0, 0, MTL::Origin::Make(0, 0, 0), size,
                          b.surface.readback, 0, rowBytes, rowBytes * size.height);
  }
  blit->endEncoding();
  if (drawable)
    b.commands->presentDrawable(drawable);
  b.commands->commit();
  if (!b.display.visible())
    b.commands->waitUntilCompleted();
  b.last = std::exchange(b.counting, {});
  pool->release();
}

bool Renderer::toScreen(const Vector3 &point, float &x, float &y) const {
  const glm::vec4 clip = backend_->viewProj * glm::vec4(toGlm(point), 1.0f);
  if (clip.w <= 0)
    return false;
  x = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(backend_->display.width());
  y = (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(backend_->display.height());
  return true;
}

Capabilities Renderer::capabilities() const { return {}; }

FrameCounts Renderer::lastFrame() const { return backend_->last; }

FrameTimes Renderer::lastFrameTimes() const { return {}; } // (not measured here)

} // namespace ScratchPadd::Scene
