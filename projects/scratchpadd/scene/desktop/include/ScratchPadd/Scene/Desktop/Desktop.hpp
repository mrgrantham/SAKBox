// Desktop.hpp - what desktops can do that the scene API leaves out, as the
// DS can't: vertex snapping, custom shaders drawn into a texture, and
// reading back a hidden window's frames. Each desktop backend (OpenGL,
// Metal, Vulkan) defines them. An app that uses them builds only for desktops (see
// ../../../../../DESIGN.md).
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace ScratchPadd::Scene::Desktop {

// Snaps the vertices of what's drawn from now until the end of the frame to
// a grid: `worldGrid` apart in the world, then `pixelGrid` pixels apart on
// screen (0 for either: no snapping). Precision Flight shows coarse
// numbers' wobble with it.
void setVertexSnap(Renderer &renderer, float worldGrid, float pixelGrid);

// The colour at (x, y), in points from the top-left, of the last frame
// `renderer` showed, in a hidden Display (one opened with visible false,
// which keeps its frames for this): for tests
Colour readPixel(const Renderer &renderer, float x, float y);

// A custom shader drawn into a texture of its own, as Shadertoy does. The
// shaders are GLSL 3.30 (the Metal and Vulkan backends compile them to
// SPIR-V when they're loaded, and Metal translates that to its language). The fragment shader gets
// `iTime` (seconds), `iResolution` (the texture's size), `iMouse` (x, y in its pixels from the
// bottom-left, and whether the button's down) and `uniform_color`. The mesh's vertices are given as
// they are (attribute 0 position, 1 normal, 2 colour), so a mesh covering -1 to 1 in x and y covers
// the texture. The texture is RGB: the shader's alpha is ignored. Show it with debug_ui
// (ImGui::Image(panel.imageId(), ...)).
class ShaderPanel {
public:
  ShaderPanel(int width, int height, const std::string &vertexSource,
              const std::string &fragmentSource, const MeshData &mesh);
  ~ShaderPanel();
  ShaderPanel(const ShaderPanel &) = delete;
  ShaderPanel &operator=(const ShaderPanel &) = delete;

  // Whether its shaders compiled (and translated); if not, error() says why
  // and it draws nothing
  bool ok() const { return error_.empty(); }
  const std::string &error() const { return error_; }

  // Draws a frame into the texture
  void render(float seconds, float mouseX, float mouseY, bool mouseDown, Colour colour);
  // The texture, as ImGui::Image takes it (an OpenGL texture's name, a
  // Metal texture, or the descriptor set ImGui's Vulkan backend made for it)
  std::uint64_t imageId() const;
  int width() const { return width_; }
  int height() const { return height_; }

  // The backend's state
  struct State;

private:
  int width_, height_;
  std::string error_;
  std::unique_ptr<State> state_;
};

} // namespace ScratchPadd::Scene::Desktop
