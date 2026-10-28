// Overlay.hpp - text and rectangles drawn over a frame's 3D, in screen
// points (top-left is 0, 0). It collects them during a frame as coloured
// triangles, which the renderer draws at the end of it; the text is
// stb_easy_font's, built from small rectangles, so every backend can draw
// it without a font texture.
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <string_view>
#include <vector>

namespace ScratchPadd::Scene {

class Overlay {
public:
  struct Vertex {
    float x, y; // points
    Colour colour;
  };

  // stb_easy_font's line height, in points at scale 1
  static constexpr float kLineHeight = 12;

  // `text` with its top-left at (x, y), `scale` times its usual size (a
  // line is kLineHeight points at 1). '\n' starts a new line.
  void text(float x, float y, std::string_view text, Colour colour, float scale = 1);
  // How wide `text`'s widest line is, in points, at `scale`
  static float textWidth(std::string_view text, float scale = 1);
  void rectangle(float x, float y, float width, float height, Colour colour);
  // A line `thickness` points wide from (x0, y0) to (x1, y1). With
  // Renderer::toScreen, it draws 3D lines over everything (a ragdoll's
  // joints through its bones).
  void line(float x0, float y0, float x1, float y1, Colour colour, float thickness = 1);

  // The triangles collected since clear(), three vertices each, in the
  // order drawn (later over earlier)
  const std::vector<Vertex> &triangles() const { return triangles_; }
  void clear() { triangles_.clear(); }

private:
  std::vector<Vertex> triangles_;
};

} // namespace ScratchPadd::Scene
