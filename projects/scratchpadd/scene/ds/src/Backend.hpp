// Backend.hpp - the DS backend's state behind Scene::Renderer (private to
// the backend)
#pragma once

#include "DisplayList.hpp"

#include <ScratchPadd/Scene/Culling.hpp>
#include <ScratchPadd/Scene/DS/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <array>
#include <cstdint>
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
    // Toon shading's textures, one for each material colour it's been drawn
    // in: the colour, and the texture's TEXIMAGE_PARAM
    std::vector<std::pair<Colour, std::uint32_t>> textures;
  };
  // Materials' textures (Renderer::load(TextureImage)), by Texture::id - 1:
  // each one's TEXIMAGE_PARAM (0: unloaded, or no room) and size, in VRAM
  // bank B after toon shading's
  struct LoadedImage {
    std::uint32_t parameter = 0;
    int width = 0, height = 0;
  };
  std::vector<LoadedImage> images;

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
  // Toon shading's texture for `mesh` in the material's `colour`, made the
  // first time (TEXIMAGE_PARAM's value, or 0 if there's no room)
  std::uint32_t textureFor(LoadedMesh &mesh, Colour colour);
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
  // Toon shading's table this frame (ToonTable.hpp), and what's in the
  // hardware's (only written again when it changes)
  DsToonTable toonTable = DsToonTable::Highlights;
  std::array<std::uint16_t, 32> toonTableSet{};
  // Outlines: whether this frame has any, and the next outlined mesh's ID
  bool outlining = false;
  unsigned nextOutlineId = 0;
  // The next translucent polygon ID: meshes then the overlay (one count a frame)
  unsigned nextTranslucentId = 1;
  // Where the next toon texture goes in VRAM bank B, in bytes
  std::uint32_t nextTextureOffset = 0;
  Fog fogSet;
  Scalar fogNear = 0, fogFar = 0;
  // What the text layer shows, redrawn only when the overlay's text changes,
  // in main RAM first (VRAM takes no single bytes) and copied in whole
  std::vector<Overlay::Text> textShown;
  std::vector<std::uint8_t> textPixels;
};

} // namespace ScratchPadd::Scene
