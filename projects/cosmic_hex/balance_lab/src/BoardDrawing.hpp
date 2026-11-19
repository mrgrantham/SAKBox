// BoardDrawing.hpp - what the balance lab's board pictures share: their
// colours and where a board's hexes sit on the screen (ImGui's draw list
// does the drawing).
#pragma once

#include "LabLayouts.hpp"

#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Layout.hpp>

#include "imgui.h"

#include <array>

namespace CosmicHex::Lab {

// The board's colours (the user's choice): planets light green,
// finds a lighter shade of empty space (unexplored space, not a place)
inline constexpr ImU32 kSpace = IM_COL32(24, 34, 55, 255);
inline constexpr ImU32 kGrid = IM_COL32(38, 52, 79, 255);
inline constexpr ImU32 kPlanet = IM_COL32(140, 214, 150, 255);
inline constexpr ImU32 kHome = IM_COL32(240, 180, 90, 255);
inline constexpr ImU32 kFind = IM_COL32(56, 74, 106, 255);
inline constexpr ImU32 kText = IM_COL32(226, 232, 244, 255);
inline constexpr ImU32 kDark = IM_COL32(20, 20, 20, 255);
inline constexpr ImU32 kEmptyCorner = IM_COL32(149, 163, 190, 200);
inline constexpr ImU32 kReach = IM_COL32(95, 212, 232, 255);
inline constexpr ImU32 kFuel = IM_COL32(232, 135, 95, 255);

inline ImVec2 toImVec(HexGrid::Point p) { return {p.x, p.y}; }

// Where a board of hexes `size` pixels across (centre to corner) sits, its
// centre at `centre`
inline HexGrid::Layout boardLayout(ImVec2 centre, float size) {
  return HexGrid::Layout{HexGrid::Orientation::PointyTop, {size, size}, {centre.x, centre.y}};
}

// A radius-4 board is about 16.5 hex sizes across
inline constexpr float kBoardAcross = 16.5f;

inline bool onBoard(Corner corner) {
  for (Hex hex : HexGrid::hexesTouching(corner))
    if (HexGrid::length(hex) <= kBoardRadius)
      return true;
  return false;
}

// A hex's outline as a filled shape with the grid's edge
inline void drawHex(ImDrawList *draw, const HexGrid::Layout &at, Hex hex, ImU32 fill) {
  std::array<ImVec2, 6> points{};
  const auto corners = at.corners(hex);
  for (std::size_t i = 0; i < 6; ++i)
    points[i] = toImVec(corners[i]);
  draw->AddConvexPolyFilled(points.data(), 6, fill);
  draw->AddPolyline(points.data(), 6, kGrid, ImDrawFlags_Closed, 1.0f);
}

// Text centred at a point
inline void centredText(ImDrawList *draw, ImVec2 at, ImU32 colour, const char *text) {
  const ImVec2 size = ImGui::CalcTextSize(text);
  draw->AddText(ImVec2(at.x - size.x / 2, at.y - size.y / 2), colour, text);
}

} // namespace CosmicHex::Lab
