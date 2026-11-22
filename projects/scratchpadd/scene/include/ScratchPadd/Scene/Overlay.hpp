// Overlay.hpp - text, rectangles and lines drawn over a frame's 3D, in
// screen points (top-left is 0, 0). It collects them during a frame, and
// the renderer draws them at the end of it: the shapes as coloured
// triangles, then the text over them in the platform's font (Font.hpp, in
// overlay_text: Silkscreen's pixels in the DS's 2D bitmap layer, Orbitron's
// smooth letters on the 3DS and desktops).
#pragma once

#include <ScratchPadd/Scene/TextWrap.hpp>
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

  // Where text may show (points): what falls outside is cut off. Its
  // default lets everything show
  struct Clip {
    float left = -1e9f, top = -1e9f, right = 1e9f, bottom = 1e9f;
    bool operator==(const Clip &) const = default;
  };

  struct Text {
    float x, y; // its top-left, points
    std::string text;
    Colour colour;
    float scale;
    Clip clip = {};
  };

  // The text added from now on cut to `clip` (a number rolling up into a
  // window, say); clipText({}) to show it all again
  void clipText(Clip clip) { clip_ = clip; }

  // `text` with its top-left at (x, y), `scale` times its usual size (on
  // the DS, Silkscreen's pixels at the half pixel at or below it:
  // Font::pixelHalves). '\n' starts a new line.
  void text(float x, float y, std::string_view text, Colour colour, float scale = 1) {
    texts_.push_back({x, y, std::string(text), colour, scale, clip_});
  }
  // How wide `text`'s widest line is drawn, in points, at `scale`, and how
  // tall a line is: the platform's font (overlay_text defines them)
  static float textWidth(std::string_view text, float scale = 1);
  static float lineHeight(float scale = 1);
  // Where a text's letters' own pixels are, from its top left, at `scale`:
  // narrower than textWidth and lineHeight, which count the space round
  // the letters (numbers centred by those sat off centre)
  struct Ink {
    float left = 0, top = 0, width = 0, height = 0;
  };
  static Ink inkOf(std::string_view text, float scale = 1);
  // `text` with its letters' pixels centred on (x, y)
  void textCentred(float x, float y, std::string_view text, Colour colour, float scale = 1) {
    const Ink ink = inkOf(text, scale);
    this->text(x - ink.left - ink.width / 2, y - ink.top - ink.height / 2, text, colour, scale);
  }
  // `text` wrapped to `width` points (wrapText, in the backend's font), its
  // top-left at (x, y); says how tall it came out
  float paragraph(float x, float y, float width, std::string_view text, Colour colour,
                  float scale = 1) {
    const std::vector<std::string> lines =
        wrapText(text, width, [scale](std::string_view line) { return textWidth(line, scale); });
    for (std::size_t i = 0; i < lines.size(); ++i)
      this->text(x, y + lineHeight(scale) * static_cast<float>(i), lines[i], colour, scale);
    return lineHeight(scale) * static_cast<float>(lines.size());
  }
  void rectangle(float x, float y, float width, float height, Colour colour);
  // A triangle with its corners at (x0, y0), (x1, y1) and (x2, y2)
  void triangle(float x0, float y0, float x1, float y1, float x2, float y2, Colour colour);
  // A regular hexagon centred on (x, y), `radius` points from its middle to
  // each corner, its corners left and right (flat top and bottom) or, with
  // pointyTop, up and down
  void hexagon(float x, float y, float radius, Colour colour, bool pointyTop = false);
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
    clip_ = {};
  }

  // A group drawn as one, then slid together (a panel sliding sideways):
  // mark() where it starts, draw it, then moveSince(mark, dx, dy)
  struct Mark {
    std::size_t triangles = 0, texts = 0;
  };
  Mark mark() const { return {triangles_.size(), texts_.size()}; }
  void moveSince(Mark from, float dx, float dy) {
    for (std::size_t i = from.triangles; i < triangles_.size(); ++i) {
      triangles_[i].x += dx;
      triangles_[i].y += dy;
    }
    for (std::size_t i = from.texts; i < texts_.size(); ++i) {
      texts_[i].x += dx;
      texts_[i].y += dy;
    }
  }

private:
  std::vector<Vertex> triangles_;
  std::vector<Text> texts_;
  Clip clip_;
};

} // namespace ScratchPadd::Scene
