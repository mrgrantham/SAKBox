// Backend.hpp - the Vulkan backend's state behind Scene::Renderer (private
// to the backend; Desktop.hpp's extras reach it through Renderer::backend())
#pragma once

#include "Surface.hpp"

#include <ScratchPadd/Scene/Culling.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Primitives.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace ScratchPadd::Scene {

struct Renderer::Backend {
  explicit Backend(Display &display);
  ~Backend();

  struct LoadedMesh {
    Vulkan::Buffer vertices, indices;
    std::uint32_t indexCount = 0;
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
  // Room in a buffer for this frame's vertices (drawLines', the overlay's)
  struct Slice {
    VkBuffer buffer;
    VkDeviceSize offset;
  };

  void drawNow(Mesh mesh, const Transform &where, const Material &material);
  void drawOverlay(const Overlay &overlay);
  // `bytes` copied into this frame's buffers, kept until the frame's drawn
  Slice frameBytes(const void *bytes, std::size_t length);

  Display &display;
  Vulkan::Surface &surface;
  VkShaderModule sceneVertex = VK_NULL_HANDLE, sceneFragment = VK_NULL_HANDLE,
                 overlayVertex = VK_NULL_HANDLE, overlayFragment = VK_NULL_HANDLE;
  VkDescriptorSetLayout frameLayout = VK_NULL_HANDLE;
  VkPipelineLayout sceneLayout = VK_NULL_HANDLE, overlayLayout = VK_NULL_HANDLE;
  // Solid and translucent, each filled (both sides, or the back culled) or
  // as a wireframe; lines; the overlay
  VkPipeline solid = VK_NULL_HANDLE, translucent = VK_NULL_HANDLE, solidCulled = VK_NULL_HANDLE,
             translucentCulled = VK_NULL_HANDLE, solidWire = VK_NULL_HANDLE,
             translucentWire = VK_NULL_HANDLE, lines = VK_NULL_HANDLE,
             overlayPipeline = VK_NULL_HANDLE;
  VkDescriptorPool descriptors = VK_NULL_HANDLE;
  VkDescriptorSet frameSet = VK_NULL_HANDLE;
  Vulkan::Buffer frameUniformBuffer; // FrameUniforms, written each frame
  std::vector<LoadedMesh> meshes;    // by Mesh::id - 1; unloaded ones have no vertices
  std::vector<Later> later;
  FrameCounts counting, last;                    // this frame's so far; the last one's
  std::vector<Overlay::Vertex> overlayTriangles; // reused each frame
  // frameBytes' buffers, reused each frame (the last ones filled so far)
  std::vector<Vulkan::Buffer> frameBuffers;
  std::size_t frameBufferAt = 0;
  VkDeviceSize frameBufferUsed = 0;

  // This frame's
  bool recording = false;
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
  };
  float worldSnap = 0, screenSnap = 0;
};

} // namespace ScratchPadd::Scene
