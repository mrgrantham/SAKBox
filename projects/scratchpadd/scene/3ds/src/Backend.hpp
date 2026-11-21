// Backend.hpp - the 3DS backend's state behind Scene::Renderer (private to
// the backend)
#pragma once

#include <ScratchPadd/Scene/3DS/Debug.hpp>
#include <ScratchPadd/Scene/3DS/Display.hpp>
#include <ScratchPadd/Scene/Culling.hpp>
#include <ScratchPadd/Scene/OutlineHull.hpp>
#include <ScratchPadd/Scene/OverlayText.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <citro3d.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
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

  // Materials' textures, by Texture::id - 1 (unloaded ones empty)
  std::vector<std::optional<C3D_Tex>> textures;
  // Each texture's pixels, kept (by Texture::id - 1): replace sends a band
  // into them and makes the texture again from them
  struct KeptPixels {
    int width = 0, height = 0;
    std::vector<Colour> pixels;
  };
  std::vector<KeptPixels> kept;
  KeptPixels keptPending; // (load's, until its texture is made)
  // The combiner's texture stage for `material` (none: passed on)
  void useTexture(const Material *material);

  struct LoadedMesh {
    // In linear memory, where the GPU reads: the vertices as Scene::Vertex
    // has them, and the triangles' indices (16-bit) or, for a mesh of more
    // than 65,536 vertices, the triangles' vertices in turn
    Vertex *vertices = nullptr;
    std::uint16_t *indices = nullptr;
    int count = 0;       // indices, or vertices drawn in turn
    int vertexCount = 0; // in `vertices`
    C3D_BufInfo buffers;
    // Its wireframe: its triangles' edges, each once (edges in the same
    // place joined, so a flat-shaded mesh's aren't doubled), as lines in
    // linear memory, made the first time it's drawn as one
    std::vector<std::uint32_t> triangles; // (kept to make it)
    LineVertex *wireframe = nullptr;
    int wireframeVertices = 0;
    // Its outline's hull (OutlineHull.hpp), made the first time it's drawn
    // with an outline
    Vertex *hull = nullptr;
    C3D_BufInfo hullBuffers;
    bool translucent = false; // has see-through vertices
    FrameCounts cost;
    Bounds bounds; // for frustum culling (Culling.hpp)
    bool doubleSided = false;
  };
  // A draw, kept until the end of the frame: each eye draws them all
  struct Draw {
    Mesh mesh;
    Transform where;
    Material material;
    float distance;        // from the eye, to draw translucent ones farthest first
    std::uint8_t view = 0; // (which of the frame's views)
  };
  struct Lines {
    std::size_t first, count; // in lineEnds
    Colour colour;
    std::uint8_t view = 0;
  };

  // A frame's own vertices (lines, the overlay), in linear memory, taken
  // in turn and given back when the next frame starts
  struct Arena {
    std::uint8_t *memory = nullptr;
    std::size_t size = 0, used = 0;
    void *take(std::size_t bytes);
  };

  void copyFrameVertices();
  // One eye's frame: each view in its part of the screen, `separation` apart
  void drawEye(C3D_RenderTarget *target, float separation);
  void drawMesh(const Draw &draw, const C3D_Mtx &viewProjection);
  void drawOutline(LoadedMesh &mesh, const Draw &draw, const C3D_Mtx &mvp);
  // Toon shading per pixel (toon.v.pica and the GPU's fragment lighting):
  // the program, the frame's lights, the band table, the combiner
  void useToon(const Material &material, const Transform &where, const C3D_Mtx &mvp);
  // The combiner passing each vertex's colour through, without fragment
  // lighting (every draw but useToon's)
  void plainColours();
  void overlayColours();
  void drawWireframe(LoadedMesh &mesh, const C3D_Mtx &mvp, const Material &material);
  void drawLineBatches(const C3D_Mtx &viewProjection, std::uint8_t view);
  void drawLineVertices(const LineVertex *vertices, int count, const C3D_Mtx &mvp,
                        const Material &material);
  void drawOverlay();
  void setMaterial(const Material &material, const Transform &where, const C3D_Mtx &mvp);
  void useScene();
  void setDebugView();

  Display &display;
  std::vector<LoadedMesh> meshes; // by Mesh::id - 1

  // The programs, scene.v.pica, line.v.pica and overlay.v.pica, and their
  // uniforms' locations
  DVLB_s *sceneShaders = nullptr, *lineShaders = nullptr, *overlayShaders = nullptr,
         *toonShaders = nullptr;
  // Toon shading per pixel: its program and uniforms; the light environment
  // (each light's colour as its specular, which the band table steps by the
  // angle between the normal and the light; the ambient light as is); the
  // table, made for this many bands
  shaderProgram_s toonProgram;
  struct {
    int mvp, normalMatrix, material;
  } toonUniforms{};
  C3D_LightEnv toonLights;
  std::array<C3D_Light, Environment::kMostLights> toonLight;
  C3D_LightLut toonTable;
  int toonTableBands = 0;
  shaderProgram_s sceneProgram, lineProgram, overlayProgram;
  struct SceneUniforms {
    int mvp, normalMatrix, material, ambient, lightDirections, lightColours, debugMix, debugAdd,
        toon, extrude;
  };
  SceneUniforms sceneUniforms;
  int lineMvp = -1, lineMaterial = -1, lineViewport = -1, overlayProjection = -1;
  C3D_AttrInfo sceneAttributes, lineAttributes, overlayAttributes;
  const shaderProgram_s *bound = nullptr;

  // This frame's
  Camera camera; // this view's
  // The frame's views (Renderer::beginView), each drawn in its part of the
  // screen at the frame's end, for each eye; the current is the last
  struct View {
    Camera camera;
    C3D_Mtx look;
    PixelRect rect; // (on the screen, 400 x 240, from its top left)
  };
  std::vector<View> views;
  // The render target's viewport for a part of the screen (the target is
  // turned: 240 x 400), and the line shader's sizes for it
  void useViewport(const PixelRect &rect);
  float lineViewportSize[4] = {120, 200, 1.0f / 240, 1.0f / 400};
  Environment environment;
  C3D_Mtx view;
  std::optional<Frustum> frustum;
  std::vector<Draw> solid, later;
  std::vector<Vector3> lineEnds;
  std::vector<Lines> lines;
  std::vector<TexturedVertex> overlayTriangles;
  Arena arena;
  LineVertex *lineVertices = nullptr;        // in the arena: lineEnds, as lines
  TexturedVertex *overlayVertices = nullptr; // in the arena
  // The overlay font's letters (Font::atlas: 128 KB of alpha for Orbitron)
  std::optional<C3D_Tex> fontAtlas;
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
