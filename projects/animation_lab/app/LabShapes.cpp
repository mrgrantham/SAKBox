#include "LabShapes.hpp"

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Handheld/CardFace.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>

#include "rock.h"
#include "rocket.h"
#include "ship.h"

#include <algorithm>

namespace AnimationLab {

namespace Scene = ScratchPadd::Scene;

namespace {

// The shape centred on the middle of its bounds, 1 across at its longest
void fitted(LabShape &shape) {
  const Scene::MeshData mesh = shape.mesh();
  Scene::Vector3 low{1e9f, 1e9f, 1e9f}, high{-1e9f, -1e9f, -1e9f};
  for (const Scene::Vertex &v : mesh.vertices) {
    low = {std::min(low.x, v.position.x), std::min(low.y, v.position.y),
           std::min(low.z, v.position.z)};
    high = {std::max(high.x, v.position.x), std::max(high.y, v.position.y),
            std::max(high.z, v.position.z)};
  }
  const Scene::Vector3 middle = (low + high) * 0.5f;
  const float longest = std::max({high.x - low.x, high.y - low.y, high.z - low.z, 1e-6f});
  shape.fit = Scene::Transform::scaling({1 / longest, 1 / longest, 1 / longest}) *
              Scene::Transform::translation(-middle);
  std::vector<Point> points;
  points.reserve(mesh.vertices.size());
  for (const Scene::Vertex &v : mesh.vertices) {
    const Scene::Vector3 p = shape.fit.apply(v.position);
    points.push_back({p.x, p.y, p.z});
  }
  shape.points = hullCorners(points); // (the few that matter, once)
}

LabShape card() {
  LabShape shape{.name = "card"};
  // 2:3 (the card's shape), lying flat, a little thick; its face on top
  // (CardFace's layers and icon, as the consoles draw it), its back plain
  const float w = 0.5f / CosmicHex::Handheld::CardFace::kHeightPerWidth, l = 0.5f, t = 0.012f;
  const Scene::Colour edge{40, 44, 56};
  Scene::MeshBuilder b;
  b.quad({-w, t, l}, {w, t, l}, {w, t, -l}, {-w, t, -l}, {},
         {Scene::TexCoord{0, 1}, {1, 1}, {1, 0}, {0, 0}});
  b.quad({-w, -t, -l}, {w, -t, -l}, {w, -t, l}, {-w, -t, l}, edge);
  b.quad({-w, -t, l}, {w, -t, l}, {w, t, l}, {-w, t, l}, edge);
  b.quad({w, -t, -l}, {-w, -t, -l}, {-w, t, -l}, {w, t, -l}, edge);
  b.quad({w, -t, l}, {w, -t, -l}, {w, t, -l}, {w, t, l}, edge);
  b.quad({-w, -t, -l}, {-w, -t, l}, {-w, t, l}, {-w, t, -l}, edge);
  shape.built = std::move(b);
  shape.pictureWidth = 126;
  shape.pictureHeight = CosmicHex::Handheld::CardFace::heightOf(shape.pictureWidth);
  const auto icon = CosmicHex::Handheld::CardFace::iconPixelsOn(CosmicHex::Art::Card::Minerals,
                                                                shape.pictureWidth);
  shape.picture = CosmicHex::Handheld::CardFace::pixelsOf(
      CosmicHex::Art::Card::Minerals, shape.pictureWidth,
      CosmicHex::Handheld::CardFace::iconPixels(CosmicHex::Art::Card::Minerals, icon.width,
                                                icon.height));
  return shape;
}

} // namespace

std::vector<Point> LabShape::corners(float size) const {
  std::vector<Point> out = points;
  for (Point &p : out)
    for (float &c : p)
      c *= size;
  return out;
}

std::vector<LabShape> labShapes() {
  std::vector<LabShape> shapes;
  shapes.push_back({.name = "die", .built = CosmicHex::Handheld::DicePieces(1).die});
  shapes.push_back(card());
  shapes.push_back({.name = "rocket", .model = Assets::kRocket});
  shapes.push_back({.name = "ship", .model = Assets::kShip});
  shapes.push_back({.name = "rock", .model = Assets::kRock});
  for (LabShape &shape : shapes)
    fitted(shape);
  return shapes;
}

} // namespace AnimationLab
