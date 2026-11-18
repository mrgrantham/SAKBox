// The board as the cameras see it on a DS (round 92, the user's photo: the
// far find tiles showed wedges of the lighter grid, whole triangles of the
// tile losing the depth test to the grid a hair under them): every thin
// layer stays far enough in front of its tile for the DS's coarse depth
// buffer, from anywhere the board's cameras look, and nothing of the grid
// lies under a tile at all
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/BoardView.hpp>

#include <DisplayList.hpp>
#include <HexGrid/Layout.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;
namespace DisplayList = ScratchPadd::Scene::DisplayList;

namespace {

constexpr float kPi = 3.14159265f;

// The hexes of a radius-4 board, the only size Cosmic Hex has
std::vector<Hex> radiusFourBoard() {
  std::vector<Hex> hexes;
  for (int q = -4; q <= 4; ++q)
    for (int r = std::max(-4, -q - 4); r <= std::min(4, -q + 4); ++r)
      hexes.push_back({q, r});
  return hexes;
}

// The cameras the top screen uses: the game's at every turn, the setup's from
// above, and the way down from one to the other
std::vector<Scene::Camera> boardCameras() {
  std::vector<Scene::Camera> cameras{topDownCamera()};
  for (int k = 0; k < 48; ++k) {
    const Scene::Camera game = gameCamera(2 * kPi * static_cast<float>(k) / 48);
    cameras.push_back(game);
    for (int step = 1; step < 10; ++step)
      cameras.push_back(cameraBetween(topDownCamera(), game, static_cast<float>(step) / 10));
  }
  return cameras;
}

// The DS's depth steps between a point on the table and the point `lift`
// above it on the same line of sight, as `camera` sees them
double stepsAbove(const Scene::Camera &camera, Scene::Vector3 onTable, float lift) {
  const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
  const float farther = (onTable - camera.eye).dot(forward);
  // (the lifted point on the same line: as much nearer as it is higher)
  const float nearer = farther * (camera.eye.y - lift) / camera.eye.y;
  return DisplayList::zBufferSteps(camera.nearPlane, camera.farPlane, nearer, farther);
}

// Whether `p` is inside hex `hex`'s tile (kTileInset of the hex), not on
// its edge: pointy-topped, so its sides face 0, 60 and 120 degrees
bool insideTile(Hex hex, Scene::Vector3 p) {
  const Scene::Vector3 c = BoardPieces::centre(hex);
  const float apothem = std::sqrt(3.0f) / 2 * kTileInset;
  for (int side = 0; side < 3; ++side) {
    const float angle = static_cast<float>(side) * kPi / 3;
    const float across = (p.x - c.x) * std::cos(angle) + (p.z - c.z) * std::sin(angle);
    if (std::fabs(across) >= apothem - 1e-4f)
      return false;
  }
  return true;
}

} // namespace

TEST(BoardViewTest, ChipsAndTheChosenRingStayInFrontOfTheirTilesOnADs) {
  const auto cameras = boardCameras();
  for (const Hex hex : radiusFourBoard()) {
    // The tile's middle and its corners: the chip and the ring lie within
    const auto corners = HexGrid::Layout{}.corners(hex);
    std::vector<Scene::Vector3> points{BoardPieces::centre(hex)};
    for (const auto corner : corners)
      points.push_back({corner.x, 0, corner.y});
    for (const Scene::Camera &camera : cameras) {
      for (const Scene::Vector3 &p : points) {
        for (const float lift : {kChipLift, kRingLift}) {
          const double steps = stepsAbove(camera, p, lift);
          if (steps < DisplayList::kSafeDepthSteps) {
            ADD_FAILURE() << "hex (" << hex.q << ", " << hex.r << "): " << steps
                          << " depth steps at a lift of " << lift;
            return;
          }
        }
      }
    }
  }
}

TEST(BoardViewTest, TheGridLiesOnlyBetweenTheTiles) {
  // Which way the tiles' triangles turn, seen from above
  const auto turning = [](Scene::Vector3 a, Scene::Vector3 b, Scene::Vector3 c) {
    return static_cast<double>((c.z - a.z) * (b.x - a.x) - (c.x - a.x) * (b.z - a.z));
  };
  const BoardPieces pieces;
  const Scene::MeshData tile = pieces.hexTile.data();
  const double tileTurn =
      turning(tile.vertices[tile.indices[0]].position, tile.vertices[tile.indices[1]].position,
              tile.vertices[tile.indices[2]].position);
  const auto tiles = radiusFourBoard();
  const Scene::MeshBuilder gaps = BoardPieces::gapsBetween(tiles);
  const Scene::MeshData data = gaps.data();
  double area = 0;
  for (std::size_t t = 0; t + 2 < data.indices.size(); t += 3) {
    const Scene::Vector3 a = data.vertices[data.indices[t]].position,
                         b = data.vertices[data.indices[t + 1]].position,
                         c = data.vertices[data.indices[t + 2]].position;
    // In the tiles' plane, and none of it inside a tile
    EXPECT_FLOAT_EQ(static_cast<float>(a.y + b.y + c.y), 0);
    const Scene::Vector3 middle = (a + b + c) * (1.0f / 3);
    for (const Hex hex : tiles) {
      if (insideTile(hex, middle)) {
        ADD_FAILURE() << "a gap triangle under the tile at (" << hex.q << ", " << hex.r << ")";
        return;
      }
    }
    // Facing up, as the tiles do (so back-face culling keeps both)
    const double turn = turning(a, b, c);
    EXPECT_GT(turn * tileTurn, 0);
    area += std::fabs(turn) / 2;
  }
  // and all of each hex the tile doesn't cover
  const double hexArea = 3 * std::sqrt(3.0) / 2;
  EXPECT_NEAR(area, static_cast<double>(tiles.size()) * hexArea * (1 - kTileInset * kTileInset),
              1e-3);
}
