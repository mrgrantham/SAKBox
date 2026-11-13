// Backend.hpp - the 3DS backend's state behind Scene::Renderer (private to
// the backend)
#pragma once

#include <ScratchPadd/Scene/3DS/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <citro3d.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ScratchPadd::Scene {

struct Renderer::Backend {
  explicit Backend(Display &display);
  ~Backend();
  Backend(const Backend &) = delete;
  Backend &operator=(const Backend &) = delete;

  struct LoadedMesh {
    // In linear memory, where the GPU reads: the vertices as Scene::Vertex
    // has them, and the triangles' indices (16-bit) or, for a mesh of more
    // than 65,536 vertices, the triangles' vertices in turn
    Vertex *vertices = nullptr;
    std::uint16_t *indices = nullptr;
    int count = 0; // indices, or vertices drawn in turn
    C3D_BufInfo buffers;
    // Its triangles' edges, each once, as pairs of indices (for a
    // wireframe), made the first time it's drawn as one
    std::vector<std::uint32_t> triangles; // (kept to make them)
    std::uint16_t *edges = nullptr;
    int edgeIndices = 0;
    bool translucent = false; // has see-through vertices
    FrameCounts cost;
  };
  // A draw, kept until the end of the frame: each eye draws them all
  struct Draw {
    Mesh mesh;
    Transform where;
    Material material;
    float distance; // from the eye, to draw translucent ones farthest first
  };
  struct Lines {
    std::size_t first, count; // in lineEnds
    Colour colour;
  };

  // A frame's own vertices (lines, the overlay), in linear memory, taken
  // in turn and given back when the next frame starts
  struct Arena {
    std::uint8_t *memory = nullptr;
    std::size_t size = 0, used = 0;
    void *take(std::size_t bytes);
  };

  void copyFrameVertices();
  void drawEye(C3D_RenderTarget *target, const C3D_Mtx &projection);
  void drawMesh(const Draw &draw, const C3D_Mtx &viewProjection);
  void drawWireframe(LoadedMesh &mesh);
  void drawLineBatches(const C3D_Mtx &viewProjection);
  void drawOverlay();
  void setMaterial(const Material &material, const Transform &where, const C3D_Mtx &mvp);
  void useScene(bool lines);

  Display &display;
  std::vector<LoadedMesh> meshes; // by Mesh::id - 1

  // The programs: scene.v.pica alone (triangles), with scene.g.pica
  // (lines), and overlay.v.pica; their uniforms' locations
  DVLB_s *sceneShaders = nullptr, *overlayShaders = nullptr;
  shaderProgram_s triangleProgram, lineProgram, overlayProgram;
  struct SceneUniforms {
    int mvp, normalMatrix, material, ambient, lightDirections, lightColours;
  };
  SceneUniforms triangleUniforms, lineUniforms;
  int overlayProjection = -1;
  C3D_AttrInfo sceneAttributes, overlayAttributes;
  const shaderProgram_s *bound = nullptr;

  // This frame's
  Camera camera;
  Environment environment;
  C3D_Mtx view;
  std::vector<Draw> solid, later;
  std::vector<Vector3> lineEnds;
  std::vector<Lines> lines;
  std::vector<Overlay::Vertex> overlayTriangles;
  Arena arena;
  Vertex *lineVertices = nullptr;             // in the arena
  Overlay::Vertex *overlayVertices = nullptr; // in the arena
  C3D_FogLut fog;
  FrameCounts counting, last;
  FrameTimes times;
};

} // namespace ScratchPadd::Scene
