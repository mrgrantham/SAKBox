// Backend.hpp - the DS backend's state behind Scene::Renderer (private to
// the backend)
#pragma once

#include "DisplayList.hpp"

#include <ScratchPadd/Scene/Culling.hpp>
#include <ScratchPadd/Scene/DS/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ScratchPadd::Scene {

struct Renderer::Backend {
  explicit Backend(Display &display) : display(display) {}

  struct LoadedMesh {
    bool loaded = false;
    DisplayList::Source source;
    // Drawn translucent whatever its material (see hasTranslucentVertices)
    bool translucent = false;
    Bounds bounds; // for frustum culling (Culling.hpp)
    bool doubleSided = false;
    // Its display lists, packed the first time it's drawn each way: a few
    // ways, the oldest dropped first
    std::vector<std::pair<DisplayList::Variant, std::vector<DisplayList::Packed>>> lists;
    static constexpr std::size_t kMostLists = 4;
  };
  // A translucent draw, kept until the end of the frame
  struct Later {
    Mesh mesh;
    Transform where;
    Material material;
    Scalar distance; // from the eye, to draw the farthest first
  };

  const std::vector<DisplayList::Packed> &listsFor(LoadedMesh &mesh,
                                                   const DisplayList::Variant &variant);
  void drawNow(Mesh mesh, const Transform &where, const Material &material, unsigned polygonId);
  void setFog(const Fog &fog);
  void drawOverlayShapes(const Overlay &overlay);
  void drawOverlayText(const Overlay &overlay);

  Display &display;
  std::vector<LoadedMesh> meshes; // by Mesh::id - 1
  std::vector<Later> later;

  // This frame's
  Camera camera;
  Colour ambient;
  std::uint32_t lightBits = 0; // the polygon format's bits for the lights in use
  FrameCounts last;
  unsigned draws = 0, culled = 0; // this frame's so far
  std::optional<Frustum> frustum; // this frame's
  // Fog: the polygon format's bit for it (0 without), and the fog and
  // camera its table was worked out for (only again when they change)
  std::uint32_t fogBit = 0;
  Fog fogSet;
  Scalar fogNear = 0, fogFar = 0;
  // What the text layer shows, redrawn only when the overlay's text changes,
  // in main RAM first (VRAM takes no single bytes) and copied in whole
  std::vector<Overlay::Text> textShown;
  std::vector<std::uint8_t> textPixels;
};

} // namespace ScratchPadd::Scene
