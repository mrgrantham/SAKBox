// Backend.hpp - the 3DS backend's state behind Scene::Renderer (private to
// the backend)
#pragma once

#include <ScratchPadd/Scene/3DS/Debug.hpp>
#include <ScratchPadd/Scene/3DS/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <citro3d.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ScratchPadd::Scene {

struct Renderer::Backend {
  explicit Backend(Display &display);
  ~Backend();
  Backend(const Backend &) = delete;
  Backend &operator=(const Backend &) = delete;

  // A line's corner, as line.v.pica takes it: this end, the line's other
  // end, the colour, and which side of the line it goes (+1 or -1). Six
  // make a line (two triangles).
  struct LineVertex {
    Vector3 end, otherEnd;
    Colour colour;
    float side;
  };

  struct LoadedMesh {
    // In linear memory, where the GPU reads: the vertices as Scene::Vertex
    // has them, and the triangles' indices (16-bit) or, for a mesh of more
    // than 65,536 vertices, the triangles' vertices in turn
    Vertex *vertices = nullptr;
    std::uint16_t *indices = nullptr;
    int count = 0; // indices, or vertices drawn in turn
    C3D_BufInfo buffers;
    // Its wireframe: its triangles' edges, each once (edges in the same
    // place joined, so a flat-shaded mesh's aren't doubled), as lines in
    // linear memory, made the first time it's drawn as one
    std::vector<std::uint32_t> triangles; // (kept to make it)
    LineVertex *wireframe = nullptr;
    int wireframeVertices = 0;
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
  void drawWireframe(LoadedMesh &mesh, const C3D_Mtx &mvp, const Material &material);
  void drawLineBatches(const C3D_Mtx &viewProjection);
  void drawLineVertices(const LineVertex *vertices, int count, const C3D_Mtx &mvp,
                        const Material &material);
  void drawOverlay();
  void setMaterial(const Material &material, const Transform &where, const C3D_Mtx &mvp);
  void useScene();

  Display &display;
  std::vector<LoadedMesh> meshes; // by Mesh::id - 1

  // The programs, scene.v.pica, line.v.pica and overlay.v.pica, and their
  // uniforms' locations
  DVLB_s *sceneShaders = nullptr, *lineShaders = nullptr, *overlayShaders = nullptr;
  shaderProgram_s sceneProgram, lineProgram, overlayProgram;
  struct SceneUniforms {
    int mvp, normalMatrix, material, ambient, lightDirections, lightColours, debugMix, debugAdd;
  };
  SceneUniforms sceneUniforms;
  int lineMvp = -1, lineMaterial = -1, overlayProjection = -1;
  C3D_AttrInfo sceneAttributes, lineAttributes, overlayAttributes;
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
  LineVertex *lineVertices = nullptr;         // in the arena: lineEnds, as lines
  Overlay::Vertex *overlayVertices = nullptr; // in the arena
  C3D_FogLut fog;
  FrameCounts counting, last;
  FrameTimes times;

  // Debug mode (Debug.hpp): its settings, the draws the GPU was sent (for
  // the watchdog), and the probe's reading
  struct Step {
    const char *what = ""; // (a literal)
    std::uint8_t eye = 0;
    std::uint16_t index = 0;
  };
  static constexpr std::size_t kSteps = 32; // the last ones kept
  struct DebugState {
    Debug::Settings settings;
    std::array<Step, kSteps> steps{};
    std::atomic<unsigned> stepCount{0};
    // Whether endFrame is drawing, and since when (ms of the system tick)
    std::atomic<bool> drawing{false};
    std::atomic<std::uint64_t> drawingSince{0};
    Colour probed;
    // Finding a hang: each frame sends only its first `drawLimit` draws,
    // one more every half second, so the frame that hangs the GPU ends with
    // the draw that does it
    unsigned drawLimit = 1, drawsThisFrame = 0, drawsLastFrame = 0, framesAtLimit = 0;
  } debug;
  std::uint8_t eye = 0; // the eye being drawn (1 left, 2 right)
  // Whether the next draw goes to the GPU (finding a hang, only so many)
  bool mayDraw() const {
    return !debug.settings.findHang || debug.drawsThisFrame < debug.drawLimit;
  }
  // Records a draw sent to the GPU
  void sent(const char *what, std::size_t index);
};

} // namespace ScratchPadd::Scene
