// Desktop.hpp - what desktops can do that the scene API leaves out, as the
// DS can't: vertex snapping, and custom shaders drawn into a texture. An app
// that uses them builds only for desktops (see DESIGN.md).
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <string>

namespace ScratchPadd::Scene::Desktop {

// Snaps the vertices of what's drawn from now until the end of the frame to
// a grid: `worldGrid` apart in the world, then `pixelGrid` pixels apart on
// screen (0 for either: no snapping). Precision Flight shows coarse
// numbers' wobble with it.
void setVertexSnap(Renderer &renderer, float worldGrid, float pixelGrid);

// A custom shader drawn into a texture of its own, as Shadertoy does: the
// fragment shader gets `iTime` (seconds), `iResolution` (the texture's
// size), `iMouse` (x, y in its pixels from the bottom-left, and whether
// the button's down) and `uniform_color`. The mesh's vertices are given as
// they are (attribute 0 position, 1 normal, 2 colour), so a mesh covering
// -1 to 1 in x and y covers the texture. The texture is RGB: the shader's alpha
// is ignored. Show the texture with debug_ui
// (ImGui::Image).
class ShaderPanel {
public:
  ShaderPanel(int width, int height, const std::string &vertexSource,
              const std::string &fragmentSource, const MeshData &mesh);
  ~ShaderPanel();
  ShaderPanel(const ShaderPanel &) = delete;
  ShaderPanel &operator=(const ShaderPanel &) = delete;

  // Draws a frame into the texture
  void render(float seconds, float mouseX, float mouseY, bool mouseDown, Colour colour);
  unsigned texture() const { return texture_; }
  int width() const { return width_; }
  int height() const { return height_; }

private:
  int width_, height_;
  unsigned framebuffer_ = 0, texture_ = 0, depth_ = 0;
  unsigned program_ = 0, vao_ = 0, vbo_ = 0, ebo_ = 0;
  int indexCount_ = 0;
};

} // namespace ScratchPadd::Scene::Desktop
