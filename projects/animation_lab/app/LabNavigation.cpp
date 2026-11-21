// The 3D view's navigation aids, drawn over it with ImGui: the corner
// widget (the ViewCube, or Blender's axis gizmo: a setting, kept in
// view.txt) and Rewind's strip along the bottom. Their shapes and what's
// under the cursor are in the library (ViewCube, AxisGizmo, ViewHistory);
// this draws them and turns clicks and drags into glides of the view.
#include "LabWindowPadd.hpp"

#include <AnimationLab/AxisGizmo.hpp>
#include <AnimationLab/ViewCube.hpp>

#include <ScratchPadd/Storage/Folders.hpp>

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace AnimationLab {

namespace {

// Sizes (points)
constexpr float kCubeHalf = 34; // (the cube's middle to a face)
// The faces' names: drawn at this size, then laid onto the face at most
// this wide and high (in the cube's units: a face is 2 across)
constexpr float kFaceNameSize = 26, kFaceNameWide = 1.3f, kFaceNameHigh = 0.5f;
constexpr float kAxesRadius = 36;  // (the gizmo's arms)
constexpr float kBallRadius = 9;   // (its balls)
constexpr float kCornerInset = 85; // (the widget's middle from the view's top-right corner)
constexpr float kDragStart = 3;    // (a press moving this far is a drag, not a click)
constexpr float kStripInset = 70;  // (the strip's ends from the view's sides)
constexpr float kStripFromBottom = 22;
constexpr float kStripHalfHigh = 7;
constexpr float kButtonRadius = 9; // (back and forward, at the strip's ends)

const ImU32 kHighlight = IM_COL32(232, 163, 58, 255); // (what a click would choose)
const ImU32 kLine = IM_COL32(150, 156, 170, 200);
const ImU32 kText = IM_COL32(235, 235, 240, 255);

// Blender's axis colours: X red, Y green, Z blue
ImU32 axisColour(int axis, int alpha = 255) {
  switch (axis) {
  case 0:
    return IM_COL32(222, 70, 70, alpha);
  case 1:
    return IM_COL32(110, 190, 60, alpha);
  default:
    return IM_COL32(70, 130, 230, alpha);
  }
}

// A face's fill: grey tinted with its axis's colour, lighter the more
// squarely it faces you
ImU32 faceColour(int face, float facing) {
  const int axis = face / 2;
  const float tint = 0.3f, light = 0.55f + 0.45f * facing;
  const float grey[3] = {78, 82, 94};
  const float colour[3][3] = {{222, 70, 70}, {110, 190, 60}, {70, 130, 230}};
  const auto mix = [&](int c) {
    return static_cast<int>((grey[c] * (1 - tint) + colour[axis][c] * tint) * light);
  };
  return IM_COL32(mix(0), mix(1), mix(2), 235);
}

ImVec2 im(ScreenPoint p) { return {p.x, p.y}; }
float distance(ScreenPoint a, ScreenPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }

void textCentred(ImDrawList *draw, ScreenPoint at, const char *text, ImU32 colour,
                 float fontSize = 0) {
  ImFont *font = ImGui::GetFont();
  if (fontSize <= 0)
    fontSize = ImGui::GetFontSize();
  const ImVec2 size = font->CalcTextSizeA(fontSize, 1e6f, 0, text);
  draw->AddText(font, fontSize, {at.x - size.x / 2, at.y - size.y / 2}, colour, text);
}

// One letter centred on `at` by its own outline (its glyph's box), not its
// line's: a capital sits in the middle of a ball, not above it
void letterCentred(ImDrawList *draw, ScreenPoint at, char letter, ImU32 colour) {
  ImFont *font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const ImFontGlyph *glyph = ImGui::GetFontBaked()->FindGlyph(static_cast<ImWchar>(letter));
  const char text[2] = {letter, 0};
  if (!glyph) {
    textCentred(draw, at, text, colour);
    return;
  }
  draw->AddText(font, size,
                {at.x - (glyph->X0 + glyph->X1) / 2, at.y - (glyph->Y0 + glyph->Y1) / 2}, colour,
                text);
}

// A face's name written onto it in 3D: drawn flat, then each of its
// vertices moved onto the face (the view is orthographic, so a face is a
// flat, sheared copy of the text: a 2 x 2 map does it). It's drawn round
// the face's middle, on the screen: ImGui leaves out letters drawn outside
// the window, as they would be about (0, 0)
void nameOnFace(ImDrawList *draw, const ViewCube::FaceAxes &face, const char *name, ImU32 colour) {
  ImFont *font = ImGui::GetFont();
  const ImVec2 size = font->CalcTextSizeA(kFaceNameSize, 1e6f, 0, name);
  // (cube units a text pixel: as wide as allowed, unless that's too high)
  const float scale = std::min(kFaceNameWide / size.x, kFaceNameHigh / size.y);
  const int first = draw->VtxBuffer.Size;
  const ImVec2 origin{face.middle.x - size.x / 2, face.middle.y - size.y / 2};
  draw->AddText(font, kFaceNameSize, origin, colour, name);
  for (int i = first; i < draw->VtxBuffer.Size; ++i) {
    ImVec2 &p = draw->VtxBuffer[i].pos;
    const float x = (p.x - face.middle.x) * scale, y = (p.y - face.middle.y) * scale;
    p = {face.middle.x + face.right.x * x + face.down.x * y,
         face.middle.y + face.right.y * x + face.down.y * y};
  }
}

} // namespace

bool LabWindowPadd::navigate(const float area[4]) {
  const bool rewind = navigateRewind(area);
  const bool corner = corner_ == Corner::ViewCube ? navigateCube(area) : navigateAxes(area);
  return rewind || corner;
}

bool LabWindowPadd::navigateCube(const float area[4]) {
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw = ImGui::GetBackgroundDrawList();
  const ScreenPoint centre{area[0] + area[2] - kCornerInset, area[1] + kCornerInset};
  const ScreenPoint mouse{io.MousePos.x, io.MousePos.y};
  const bool free = !io.WantCaptureMouse && !scrubbing_;
  const auto cells = ViewCube::cells(view_, centre, kCubeHalf);
  // The home button, above and left of the cube: frames everything
  const ScreenPoint home{centre.x - kCubeHalf * 1.9f, centre.y - kCubeHalf * 1.9f};
  const auto onHome = [&](ScreenPoint p) { return distance(p, home) < 10; };

  const bool held = cornerPress_ && cornerPress_->dragged;
  const auto hovered = free && !held ? ViewCube::pick(cells, mouse) : std::nullopt;
  const bool onRing = free && !hovered && ViewCube::onCompass(view_, centre, kCubeHalf, mouse);
  const bool homeHovered = free && onHome(mouse);

  // A press: a click (a cell, the home) or the start of a drag (turning;
  // on the compass, about the world's up only)
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (hovered || onRing || homeHovered))
    cornerPress_ = CornerPress{.from = mouse, .compass = onRing};
  if (cornerPress_) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      if (distance(mouse, cornerPress_->from) > kDragStart)
        cornerPress_->dragged = true;
      if (cornerPress_->dragged)
        view_.orbit(io.MouseDelta.x, cornerPress_->compass ? 0 : io.MouseDelta.y);
    } else {
      if (!cornerPress_->dragged) {
        if (onHome(cornerPress_->from))
          frameNext_ = true;
        else if (const auto way = ViewCube::pick(cells, cornerPress_->from))
          view_.glideTo(view_.facing(ViewCube::directionOf(*way)));
      } else if (!cornerPress_->compass) {
        // (let go near one of the 26 views: settle on it)
        if (const auto way = ViewCube::nearest(view_))
          view_.glideTo(view_.facing(ViewCube::directionOf(*way)));
      }
      cornerPress_.reset();
    }
  }

  // The compass, under the cube
  const auto ring = ViewCube::compass(view_, centre, kCubeHalf);
  std::vector<ImVec2> points;
  points.reserve(ring.size());
  for (const ScreenPoint &p : ring)
    points.push_back(im(p));
  draw->AddPolyline(points.data(), static_cast<int>(points.size()),
                    (onRing || (cornerPress_ && cornerPress_->compass)) ? kHighlight : kLine,
                    ImDrawFlags_Closed, 1.5f);
  const auto letters = ViewCube::compassLetters(view_, centre, kCubeHalf);
  const char *names[4] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; ++i)
    textCentred(draw, letters[static_cast<std::size_t>(i)], names[i], kLine);

  // The cube: each cell, the hovered view's cells lit (an edge or a corner
  // is cells on two or three faces); the grid shown only while it's hovered
  for (const auto &cell : cells) {
    const ImVec2 quad[4] = {im(cell.corners[0]), im(cell.corners[1]), im(cell.corners[2]),
                            im(cell.corners[3])};
    const bool lit = hovered && cell.way == *hovered;
    draw->AddConvexPolyFilled(quad, 4, lit ? kHighlight : faceColour(cell.face, cell.facing));
    if (hovered)
      draw->AddPolyline(quad, 4, IM_COL32(20, 22, 28, 90), ImDrawFlags_Closed, 1.0f);
  }
  // Each face's outline and name
  for (std::size_t i = 0; i + 8 < cells.size(); i += 9) {
    const ImVec2 outline[4] = {im(cells[i].corners[0]), im(cells[i + 6].corners[1]),
                               im(cells[i + 8].corners[2]), im(cells[i + 2].corners[3])};
    draw->AddPolyline(outline, 4, IM_COL32(20, 22, 28, 200), ImDrawFlags_Closed, 1.5f);
    const int face = cells[i].face;
    if (cells[i].facing > 0.15f) // (edge-on, a name would be a smear)
      nameOnFace(draw, ViewCube::faceAxes(view_, centre, kCubeHalf, face),
                 ViewCube::kFaceNames[static_cast<std::size_t>(face)], kText);
  }

  // The home button: a little house
  const ImU32 houseColour = homeHovered ? kHighlight : kLine;
  draw->AddTriangleFilled({home.x - 7, home.y - 1}, {home.x, home.y - 8}, {home.x + 7, home.y - 1},
                          houseColour);
  draw->AddRectFilled({home.x - 5, home.y - 1}, {home.x + 5, home.y + 7}, houseColour);
  if (homeHovered)
    ImGui::SetTooltip("Frame it all (Home)");

  return hovered.has_value() || onRing || homeHovered || cornerPress_.has_value();
}

bool LabWindowPadd::navigateAxes(const float area[4]) {
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw = ImGui::GetBackgroundDrawList();
  const ScreenPoint centre{area[0] + area[2] - kCornerInset, area[1] + kCornerInset};
  const ScreenPoint mouse{io.MousePos.x, io.MousePos.y};
  const bool free = !io.WantCaptureMouse && !scrubbing_;
  const auto balls = AxisGizmo::balls(view_, centre, kAxesRadius);
  const bool over = free && distance(mouse, centre) < kAxesRadius + kBallRadius;
  const auto hovered = over ? AxisGizmo::pick(balls, mouse, kBallRadius) : std::nullopt;

  // A press: a click on a ball looks from its end (or, already there, the
  // opposite: Blender's way); a drag anywhere on it turns the view
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && over)
    cornerPress_ = CornerPress{.from = mouse};
  if (cornerPress_) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      if (distance(mouse, cornerPress_->from) > kDragStart)
        cornerPress_->dragged = true;
      if (cornerPress_->dragged)
        view_.orbit(io.MouseDelta.x, io.MouseDelta.y);
    } else {
      if (!cornerPress_->dragged)
        if (const auto ball = AxisGizmo::pick(balls, cornerPress_->from, kBallRadius))
          view_.glideTo(view_.facing(AxisGizmo::lookFrom(view_, *ball)));
      cornerPress_.reset();
    }
  }

  // Drawn as Blender's: a faint disc while it's hovered or held, then the
  // ends farthest first; the axes' own ends filled and named, the far ends
  // hollow
  if (over || cornerPress_)
    draw->AddCircleFilled(im(centre), kAxesRadius + kBallRadius + 2, IM_COL32(255, 255, 255, 30));
  const char names[3] = {'X', 'Y', 'Z'};
  for (const auto &ball : balls) {
    const bool lit = hovered && hovered->axis == ball.axis && hovered->sign == ball.sign;
    if (ball.sign > 0) {
      draw->AddLine(im(centre), im(ball.at), axisColour(ball.axis), 2.5f);
      draw->AddCircleFilled(im(ball.at), kBallRadius, axisColour(ball.axis));
      letterCentred(draw, ball.at, names[ball.axis], IM_COL32(255, 255, 255, 255));
    } else {
      draw->AddLine(im(centre), im(ball.at), axisColour(ball.axis, 110), 1);
      draw->AddCircleFilled(im(ball.at), kBallRadius - 1, IM_COL32(30, 32, 40, 200));
      draw->AddCircle(im(ball.at), kBallRadius - 1, axisColour(ball.axis), 0, 1.5f);
    }
    if (lit)
      draw->AddCircle(im(ball.at), kBallRadius + 1, IM_COL32(255, 255, 255, 255), 0, 2);
  }
  return over || cornerPress_.has_value();
}

bool LabWindowPadd::navigateRewind(const float area[4]) {
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw = ImGui::GetBackgroundDrawList();
  const ScreenPoint mouse{io.MousePos.x, io.MousePos.y};
  const bool free = !io.WantCaptureMouse && !cornerPress_;
  const float x0 = area[0] + kStripInset, x1 = area[0] + area[2] - kStripInset;
  const float y = area[1] + area[3] - kStripFromBottom;
  const std::size_t count = viewHistory_.count();
  const float last = count > 1 ? static_cast<float>(count - 1) : 1;
  const auto xOf = [&](float place) { return x0 + (x1 - x0) * place / last; };
  const auto placeOf = [&](float x) { return std::clamp((x - x0) / (x1 - x0), 0.0f, 1.0f) * last; };
  const ScreenPoint backAt{x0 - 26, y}, forwardAt{x1 + 26, y};

  const bool onStrip = free && mouse.x >= x0 - 6 && mouse.x <= x1 + 6 &&
                       std::fabs(mouse.y - y) <= kStripHalfHigh + 4;
  const bool onBack = free && distance(mouse, backAt) <= kButtonRadius + 2;
  const bool onForward = free && distance(mouse, forwardAt) <= kButtonRadius + 2;

  // Back and forward: a step each click (as Alt+Left and Alt+Right)
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (onBack)
      if (const auto pose = viewHistory_.back(view_.destination()))
        view_.glideTo(*pose);
    if (onForward)
      if (const auto pose = viewHistory_.forward(view_.destination()))
        view_.glideTo(*pose);
    if (onStrip && count > 1) {
      viewHistory_.keep(view_.destination()); // (the view you left stays on the strip)
      scrubbing_ = placeOf(mouse.x);
    }
  }
  // Scrubbing: the view follows the playhead smoothly through the steps;
  // letting go settles on the nearest step
  if (scrubbing_) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      scrubbing_ = placeOf(mouse.x);
      view_.glideTo(viewHistory_.along(*scrubbing_), 0);
    } else {
      viewHistory_.stopAt(*scrubbing_);
      view_.glideTo(viewHistory_.step(viewHistory_.at()));
      scrubbing_.reset();
    }
  }

  // The strip: brighter while it's being used; a dot for each step, the
  // one you're at lit; the playhead
  const bool active = onStrip || onBack || onForward || scrubbing_;
  const int alpha = active ? 230 : 120;
  draw->AddRectFilled({x0 - 8, y - kStripHalfHigh}, {x1 + 8, y + kStripHalfHigh},
                      IM_COL32(40, 44, 54, alpha), kStripHalfHigh);
  for (std::size_t i = 0; i < count; ++i) {
    const float x = count > 1 ? xOf(static_cast<float>(i)) : x0;
    const bool here = i == viewHistory_.at();
    draw->AddCircleFilled({x, y}, here ? 3.5f : 2.5f,
                          here ? kHighlight : IM_COL32(200, 205, 215, alpha));
  }
  if (count > 1) {
    const float head = xOf(scrubbing_ ? *scrubbing_ : static_cast<float>(viewHistory_.at()));
    draw->AddLine({head, y - kStripHalfHigh - 3}, {head, y + kStripHalfHigh + 3}, kHighlight, 2);
  }
  // Back and forward buttons: a circle with an arrowhead
  for (const bool forward : {false, true}) {
    const ScreenPoint at = forward ? forwardAt : backAt;
    const bool lit = forward ? onForward : onBack;
    draw->AddCircleFilled(im(at), kButtonRadius, IM_COL32(40, 44, 54, alpha));
    const float d = forward ? 1 : -1;
    draw->AddTriangleFilled({at.x + 4 * d, at.y}, {at.x - 3 * d, at.y - 5},
                            {at.x - 3 * d, at.y + 5},
                            lit ? kHighlight : IM_COL32(200, 205, 215, alpha));
  }
  if (onStrip || onBack || onForward)
    ImGui::SetTooltip("Rewind: the views you've been at. Drag along it to go back through them; "
                      "Alt+Left and Alt+Right step back and forward");
  return onStrip || onBack || onForward || scrubbing_.has_value();
}

// view.txt: "corner cube" or "corner axes"
void LabWindowPadd::readViewSettings() {
  std::ifstream in(folder_ + "view.txt");
  std::string word, value;
  while (in >> word >> value)
    if (word == "corner")
      corner_ = value == "axes" ? Corner::Axes : Corner::ViewCube;
}

void LabWindowPadd::writeViewSettings() const {
  ScratchPadd::Storage::makeFolder(folder_);
  std::ofstream out(folder_ + "view.txt");
  out << "corner " << (corner_ == Corner::Axes ? "axes" : "cube") << "\n";
}

} // namespace AnimationLab
