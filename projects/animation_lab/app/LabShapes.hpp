// LabShapes.hpp - what the lab shows its motions as (and tumbles as):
// pieces of the games, each centred on its middle and 1 across at its
// longest, with its points for its hull (hullCorners, Lab.hpp)
//
//   die     Cosmic Hex's die (DicePieces)
//   card    a Cosmic Hex card, its SVG picture on a thin card
//   rocket  the title screen's rocket (Blender)
//   ship    the example ship (Blender)
//   rock    the example rock (Blender)
#pragma once

#include <AnimationLab/Lab.hpp>

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <optional>
#include <string>
#include <vector>

namespace AnimationLab {

struct LabShape {
  std::string name;
  std::optional<ScratchPadd::Scene::MeshBuilder> built; // (made here)
  ScratchPadd::Scene::MeshData model;                   // (from Blender)
  ScratchPadd::Scene::Transform fit;                    // (its mesh to 1 across, centred)
  std::vector<Point> points;                            // (fitted)
  std::vector<ScratchPadd::Scene::Colour> picture;      // (the card's)
  int pictureWidth = 0, pictureHeight = 0;

  ScratchPadd::Scene::MeshData mesh() const { return built ? built->data() : model; }
  // Its hull's corners at `size` across
  std::vector<Point> corners(float size) const;
};

std::vector<LabShape> labShapes();

} // namespace AnimationLab
