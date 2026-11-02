// Overlay.hpp - text, rectangles and lines drawn over a frame's 3D, in
// screen points (top-left is 0, 0). It collects them during a frame, and
// the renderer draws them at the end of it: the shapes as coloured
// triangles, then the text over them in the backend's own font
// (stb_easy_font's, built from small rectangles, on desktops; the DS's 2D
// text layer, in 8-point cells, on the DS).
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace ScratchPadd::Scene {

class Overlay {
public:
  struct Vertex {
    float x, y; // points
    Colour colour;
  };

  struct Text {
    float x, y; // its top-left, points
    std::string text;
    Colour colour;
    float scale;
  };

  // `text` with its top-left at (x, y), `scale` times its usual size (the
  // DS's text layer has one size: it ignores `scale`). '\n' starts a new
  // line.
  void text(float x, float y, std::string_view text, Colour colour, float scale = 1) {
    texts_.push_back({x, y, std::string(text), colour, scale});
  }
  // How wide `text`'s widest line is drawn, in points, at `scale`, and how
  // tall a line is: the backend's font (each backend defines them)
  static float textWidth(std::string_view text, float scale = 1);
  static float lineHeight(float scale = 1);
  void rectangle(float x, float y, float width, float height, Colour colour);
  // A line `thickness` points wide from (x0, y0) to (x1, y1). With
  // Renderer::toScreen, it draws 3D lines over everything (a ragdoll's
  // joints through its bones).
  void line(float x0, float y0, float x1, float y1, Colour colour, float thickness = 1);

  // The rectangles and lines collected since clear(), as triangles, three
  // vertices each, in the order drawn (later over earlier)
  const std::vector<Vertex> &triangles() const { return triangles_; }
  // The text collected since clear(), drawn over the triangles
  const std::vector<Text> &texts() const { return texts_; }
  void clear() {
    triangles_.clear();
    texts_.clear();
  }

private:
  std::vector<Vertex> triangles_;
  std::vector<Text> texts_;
};

} // namespace ScratchPadd::Scene
