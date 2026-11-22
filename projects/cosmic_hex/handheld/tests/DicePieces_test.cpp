// The dice as drawn: every face's pips on it (21 in all), the
// glass container filling the screen in front of the camera, and a die
// turned as its quaternion says
#include <CosmicHex/Handheld/BoardView.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace CosmicHex::Handheld;
using CosmicHex::Hex;

TEST(DicePiecesTest, ADieHasItsPipsAndRoundedCorners) {
  const DicePieces pieces(1.2f);
  // The chamfered cube's 44 triangles and 21 pips of 2
  EXPECT_EQ(pieces.die.triangleCount(), 44u + 21u * 2u);
  // Every triangle faces out (its winding's normal away from the middle),
  // and nothing reaches past the die's half-size
  const Scene::MeshData data = pieces.die.data();
  for (std::size_t i = 0; i + 2 < data.indices.size(); i += 3) {
    const Scene::Vector3 a = data.vertices[data.indices[i]].position,
                         b = data.vertices[data.indices[i + 1]].position,
                         c = data.vertices[data.indices[i + 2]].position;
    const Scene::Vector3 normal = (b - a).cross(c - a);
    const Scene::Vector3 middle = (a + b + c) * (1.0f / 3);
    EXPECT_GT(normal.dot(middle), 0) << "triangle " << i / 3;
    for (const Scene::Vector3 &p : {a, b, c}) {
      EXPECT_LE(std::abs(p.x), 0.61f);
      EXPECT_LE(std::abs(p.y), 0.61f);
      EXPECT_LE(std::abs(p.z), 0.61f);
    }
  }
}

TEST(DicePiecesTest, TheGlassFillsTheScreen) {
  const Scene::Camera camera{.eye = {0, 9.5f, 9.5f}, .target = {0, 0, 1.2f}, .fieldOfView = 50};
  const float aspect = 400.0f / 240;
  const Scene::Transform glass = glassFrame(camera, aspect, 10, 6);
  const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
  const Scene::Vector3 right = forward.cross(camera.up).normalised();
  const Scene::Vector3 up = right.cross(forward);
  // Its middle straight ahead, 6 away
  const Scene::Vector3 middle = glass.apply({0, 0, 0}) - camera.eye;
  EXPECT_NEAR(middle.dot(forward), 6, 1e-3);
  EXPECT_NEAR(middle.dot(right), 0, 1e-3);
  // Its corners at the screen's edges (within it), the right on the right
  // and the far edge (-z) at the top
  const float halfTall = std::tan(25 * 3.14159265f / 180);
  for (const float sx : {-1.0f, 1.0f})
    for (const float sz : {-1.0f, 1.0f}) {
      const Scene::Vector3 p = glass.apply({5 * sx, 0, 3 * sz}) - camera.eye;
      const float across = p.dot(right) / p.dot(forward), along = p.dot(up) / p.dot(forward);
      EXPECT_LE(std::abs(across), halfTall * aspect + 1e-3f);
      EXPECT_LE(std::abs(along), halfTall + 1e-3f);
      EXPECT_GT(across * sx, 0);
      EXPECT_LT(along * sz, 0);
    }
  // A die at rest is in front of the floor, towards the camera
  const Scene::Vector3 die = glass.apply({0, 0.5f, 0}) - camera.eye;
  EXPECT_LT(die.dot(forward), 6);
}

TEST(DicePiecesTest, ADieTurnsAsItsQuaternion) {
  const Scene::Transform glass = Scene::Transform::identity();
  // A quarter turn about y (w = cos 45, y = sin 45): x goes to -z
  const float h = std::sqrt(0.5f);
  const Scene::Transform t = dieOnGlass(glass, {1, 2, 3}, {h, 0, h, 0}, 0.5f);
  const Scene::Vector3 x = t.applyToDirection({1, 0, 0});
  EXPECT_NEAR(x.z, -1, 1e-5);
  EXPECT_NEAR(x.x, 0, 1e-5);
  EXPECT_NEAR(t.position().x, 0.5f, 1e-5); // (moved half a unit left by the pull)
  EXPECT_NEAR(t.position().y, 2, 1e-5);
}

// Where the dice show on the screen, for the roll's words: the
// glass's middle in the screen's middle, its right edge to the right,
// nothing behind the camera
TEST(DicePiecesTest, PointsOnTheScreen) {
  const Scene::Camera camera{.eye = {0, 9.5f, 9.5f}, .target = {0, 0, 1.2f}, .fieldOfView = 50};
  const Scene::Transform glass = glassFrame(camera, 400.0f / 240, 10, 6);
  // (off the screen if missing: the checks below then fail)
  constexpr std::array<float, 2> kMissing{-1000, -1000};
  const auto middle = onScreen(camera, 400, 240, glass.apply({0, 0, 0})).value_or(kMissing);
  EXPECT_NEAR(middle[0], 200, 0.5f);
  EXPECT_NEAR(middle[1], 120, 0.5f);
  const auto right = onScreen(camera, 400, 240, glass.apply({5, 0, 0})).value_or(kMissing);
  EXPECT_NEAR(right[0], 200 + 0.82f * 200, 2); // (the glass's floor: 82% of the width)
  const auto top = onScreen(camera, 400, 240, glass.apply({0, 0, -3})).value_or(kMissing);
  EXPECT_LT(top[1], 120);
  EXPECT_GT(top[1], 0);
  EXPECT_FALSE(onScreen(camera, 400, 240, camera.eye * 2 - camera.target).has_value());
}

// A slide on the dice's curve: from 0 to 1, halfway at half the
// time, speeding up then slowing (each half the same curve), and still
// moving at the glass's pace (one screen width in a second or two)
TEST(DicePiecesTest, SlidesOnTheDicesCurve) {
  const float total = slideSeconds();
  EXPECT_GT(total, 0.8f);
  EXPECT_LT(total, 2.0f);
  EXPECT_FLOAT_EQ(slideAcross(0), 0);
  EXPECT_FLOAT_EQ(slideAcross(total), 1);
  EXPECT_NEAR(slideAcross(total / 2), 0.5f, 1e-5);
  EXPECT_NEAR(slideAcross(total / 4), 1 - slideAcross(total * 3 / 4), 1e-5); // (mirrored)
  EXPECT_LT(slideAcross(total / 4), 0.25f); // (speeding up: a quarter of the time, less of the way)
  float last = 0;
  for (int i = 1; i <= 20; ++i) {
    const float now = slideAcross(total * static_cast<float>(i) / 20);
    EXPECT_GE(now, last);
    last = now;
  }
}

// The home die: every face facing out (its normal away from the middle),
// a few dozen triangles, and each side brought to face the viewer (+z) by
// its turn, its colour the one there
TEST(DicePiecesTest, TheHomeDieShowsEachSide) {
  const std::array<Scene::Colour, 5> colours{
      Scene::Colour{200, 0, 0}, {0, 200, 0}, {0, 0, 200}, {200, 200, 0}, {0, 200, 200}};
  const Scene::Colour star{250, 180, 40};
  const Scene::MeshBuilder built = homeDie(colours, star);
  const Scene::MeshData mesh = built.data();
  EXPECT_LT(mesh.indices.size() / 3, 120u);
  for (const Scene::Vertex &v : mesh.vertices)
    EXPECT_GT(v.normal.dot(v.position), 0); // (outward: the middle is the origin)
  for (int face = 0; face < 6; ++face) {
    const float turn = homeDieTurn(face);
    // The colour of the inset whose normal faces most nearly +z once turned
    float best = -2;
    Scene::Colour seen{};
    for (const Scene::Vertex &v : mesh.vertices) {
      const float z = v.normal.y * std::sin(turn) + v.normal.z * std::cos(turn);
      if (z > best + 1e-4f && !(v.colour == Scene::Colour{196, 188, 172})) {
        best = z;
        seen = v.colour;
      }
    }
    EXPECT_NEAR(best, 1, 1e-4f) << face;
    EXPECT_TRUE(seen == (face < 5 ? colours[static_cast<std::size_t>(face)] : star)) << face;
  }
}

namespace {
// The board's tile's corners at `hex` (BoardPieces::hexTile's, placed)
std::vector<Scene::Vector3> tileCorners(Hex hex) {
  const BoardPieces pieces;
  const Scene::MeshData tile = pieces.hexTile.data(); // (a view of pieces')
  std::vector<Scene::Vector3> corners;
  for (const Scene::Vertex &v : tile.vertices)
    if (v.position.length() > 0.5f)
      corners.push_back(BoardPieces::onHex(hex).apply(v.position));
  return corners;
}
// The nearest of `points` to `p`
float nearest(const std::vector<Scene::Vector3> &points, const Scene::Vector3 &p) {
  float best = 1e9f;
  for (const Scene::Vector3 &q : points)
    best = std::min(best, (q - p).length());
  return best;
}
} // namespace

// The home die in the board is its tile: its face lies exactly where the
// board's tile does, its sides' corners on the tile's corners, the rest of
// it below the board
TEST(DicePiecesTest, TheHomeDieInTheBoardIsItsTile) {
  const Hex hex{2, -1};
  const Scene::Transform pose =
      homeDiePose(homeDieInBoard(BoardPieces::centre(hex)),
                  homeDieFacing(gameCamera(0), 256, 192, 128, 50, 40), {0, 1, 0}, 0, 0, 0);
  const Scene::Transform face = homeDieFace(pose), tile = BoardPieces::onHex(hex);
  for (const Scene::Vector3 &p : {Scene::Vector3{0, 0, 0}, {1, 0, 0}, {0, 0, 1}})
    EXPECT_NEAR((face.apply(p) - tile.apply(p)).length(), 0, 1e-4);
  const std::vector<Scene::Vector3> corners = tileCorners(hex);
  ASSERT_EQ(corners.size(), 12u); // (each of six triangles' two corners)
  const Scene::MeshBuilder built = homeDie({Scene::Colour{}, {}, {}, {}, {}}, Scene::Colour{});
  const Scene::MeshData body = built.data(); // (a view of built's)
  int onCorners = 0;
  for (const Scene::Vertex &v : body.vertices) {
    const Scene::Vector3 p = homeDieBody(pose).apply(v.position);
    EXPECT_LT(p.y, 1e-4f); // (none of it above the board)
    if (std::abs(p.y) < 1e-4f && nearest(corners, p) < 1e-3f)
      ++onCorners;
  }
  EXPECT_GE(onCorners, 6); // (its top's corners: the tile's)
}

// Facing you: in front of the camera at the pixel asked, as wide as asked,
// each side brought to face you by its turn, and its face (the tile) turned
// a little towards you too
TEST(DicePiecesTest, TheHomeDieFacesYou) {
  const Scene::Camera camera = gameCamera(0.4f);
  const Scene::Transform facing = homeDieFacing(camera, 256, 192, 128, 50, 40);
  const auto middle = onScreen(camera, 256, 192, facing.position());
  ASSERT_TRUE(middle.has_value());
  EXPECT_NEAR((*middle)[0], 128, 0.5f);
  EXPECT_NEAR((*middle)[1], 50, 0.5f);
  const Scene::Vector3 toCamera = (camera.eye - facing.position()).normalised();
  for (int side = 0; side < 6; ++side) {
    const Scene::Transform turned =
        facing * Scene::Transform::rotation({1, 0, 0}, homeDieTurn(side));
    // (side i's outward normal, a sixth of a turn round x from side 0's +z)
    const float a = static_cast<float>(side) * 3.14159265f / 3;
    const Scene::Vector3 normal{0, -std::sin(a), std::cos(a)};
    EXPECT_GT(turned.applyToDirection(normal).normalised().dot(toCamera), 0.85f) << side;
  }
  const Scene::Vector3 faceOut = facing.applyToDirection({1, 0, 0});
  EXPECT_GT(faceOut.dot(toCamera), 0.3f);               // (its face shows)
  EXPECT_GT(faceOut.dot(viewAxes(camera).right), 0.5f); // (on the right)
  // (its sides 40 pixels apart across the screen, as near as perspective gives)
  const Scene::Transform body = homeDieBody(facing);
  const auto top = onScreen(camera, 256, 192, body.apply({0, 0.5f, 0}));
  const auto bottom = onScreen(camera, 256, 192, body.apply({0, -0.5f, 0}));
  ASSERT_TRUE(top && bottom);
  EXPECT_NEAR(std::hypot((*top)[0] - (*bottom)[0], (*top)[1] - (*bottom)[1]), 40, 2);
}

// On its way: straight up out of its place first, upright, till its bottom
// clears the board; then turning as it arcs to face you; rigid throughout
// (its axes square and unit), and `sink` lowers it
TEST(DicePiecesTest, TheHomeDieRisesThenArcs) {
  const Scene::Camera camera = gameCamera(0);
  const Scene::Transform inBoard = homeDieInBoard(BoardPieces::centre({0, 0}));
  const Scene::Transform facing = homeDieFacing(camera, 256, 192, 128, 50, 40);
  const Scene::Vector3 bow = viewAxes(camera).up;
  const auto pose = [&](float lifted, float sink = 0) {
    return homeDiePose(inBoard, facing, bow, lifted, sink, 0);
  };
  const float length = kHomeDieLength * kHomeDieScale;
  for (float lifted = 0; lifted <= kHomeDieRisen; lifted += 0.05f) {
    EXPECT_NEAR(pose(lifted).applyToDirection({1, 0, 0}).y, 1, 1e-4) << lifted; // (upright)
    EXPECT_NEAR(pose(lifted).position().x, 0, 1e-4);
  }
  // (risen: its bottom above the board)
  EXPECT_GT(pose(kHomeDieRisen).apply({-length / 2, 0, 0}).y, 0.2f);
  EXPECT_NEAR((pose(1).position() - facing.position()).length(), 0, 1e-4);
  for (float lifted = 0; lifted <= 1; lifted += 0.1f) {
    const Scene::Transform p = pose(lifted);
    for (const Scene::Vector3 axis : {Scene::Vector3{1, 0, 0}, {0, 1, 0}, {0, 0, 1}})
      EXPECT_NEAR(p.applyToDirection(axis).length(), 1, 1e-4);
    EXPECT_NEAR(p.applyToDirection({1, 0, 0}).dot(p.applyToDirection({0, 1, 0})), 0, 1e-4);
  }
  EXPECT_NEAR(pose(0, 0.1f).position().y - pose(0).position().y, -0.1f, 1e-5);
}
