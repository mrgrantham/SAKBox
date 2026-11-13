// Backend.hpp - the Metal backend's state behind Scene::Renderer (private
// to the backend; Desktop.hpp's extras reach it through Renderer::backend())
#pragma once

#include "Surface.hpp"

#include <ScratchPadd/Scene/Culling.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Primitives.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <Metal/Metal.hpp>

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::Scene {

namespace Metal {
// A library from Metal shading language source, compiled when it's loaded
// (the Mac needs no Metal compiler installed); null with `error` set if it
// doesn't compile
MTL::Library *compileLibrary(const std::string &source, std::string &error);
// The vertex layout of Scene::Vertex: attribute 0 position, 1 normal (float3),
// 2 colour (four bytes, 0-1), in `buffer`
MTL::VertexDescriptor *vertexLayout(NS::UInteger buffer);
} // namespace Metal

struct Renderer::Backend {
  explicit Backend(Display &display);
  ~Backend();

  struct LoadedMesh {
    MTL::Buffer *vertices = nullptr, *indices = nullptr;
    NS::UInteger indexCount = 0;
    // Drawn translucent whatever its material (see hasTranslucentVertices)
    bool translucent = false;
    FrameCounts cost; // what it costs the DS (Primitives.hpp), counted a draw
    Bounds bounds;    // for frustum culling (Culling.hpp)
    bool doubleSided = false;
  };
  // A translucent draw, kept until the end of the frame
  struct Later {
    Mesh mesh;
    Transform where;
    Material material;
    float distance; // from the eye, to draw the farthest first
  };

  void drawNow(Mesh mesh, const Transform &where, const Material &material);
  void drawOverlay(const Overlay &overlay);
  // A buffer holding `bytes`, kept until the frame's drawn
  MTL::Buffer *frameBuffer(const void *bytes, std::size_t length);

  Display &display;
  Metal::Surface &surface;
  MTL::RenderPipelineState *solid = nullptr, *translucent = nullptr, *overlayPipeline = nullptr;
  MTL::DepthStencilState *depthWrite = nullptr, *depthTest = nullptr, *noDepth = nullptr;
  std::vector<LoadedMesh> meshes; // by Mesh::id - 1; unloaded ones have no vertices
  std::vector<Later> later;
  FrameCounts counting, last;                    // this frame's so far; the last one's
  std::vector<Overlay::Vertex> overlayTriangles; // reused each frame

  // This frame's
  MTL::CommandBuffer *commands = nullptr; // the last one's, until this one begins
  MTL::RenderCommandEncoder *encoder = nullptr;
  std::vector<MTL::Buffer *> buffers; // frameBuffer's, released when it's drawn
  glm::vec3 eye{0.0f};
  glm::mat4 viewProj{1.0f};
  std::optional<Frustum> frustum; // this frame's
  // The uniforms each draw shares (the shaders' Frame), and Desktop.hpp's
  // vertex snapping, for the rest of the frame
  struct FrameUniforms {
    float viewProj[16];
    float viewport[2];
    float pad0[2];
    float ambient[4];
    float lightDirections[4][4];
    float lightColours[4][4];
    float fogColour[4];
    float fogDensity;
    int lightCount;
    float pad1[2];
  } frameUniforms{};
  float worldSnap = 0, screenSnap = 0;
};

} // namespace ScratchPadd::Scene
