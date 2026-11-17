// The title scene's pieces: the letters as boxes with an outline each, as
// wide as their words, centred; the sky's stars on their sphere
#include <ScratchPadd/TitleScene/TitleScene.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace ScratchPadd;
using namespace ScratchPadd::TitleScene;

TEST(TitleSceneTest, LettersAreBoxesWithOutlines) {
  const Letters letters = makeLetters("HEX", 1, 0.5f, {255, 255, 255});
  ASSERT_GT(letters.mesh.triangleCount(), 0u);
  // Each stroke a box (12 triangles) and its front's four edges (8 points)
  const std::size_t boxes = letters.mesh.triangleCount() / 12;
  EXPECT_EQ(letters.mesh.triangleCount() % 12, 0u);
  EXPECT_EQ(letters.outlines.size(), boxes * 8);
  // Centred across, about a capital's height tall, in front at half depth
  float left = 1e9f, right = -1e9f, low = 1e9f, high = -1e9f;
  for (const Scene::Vector3 &p : letters.outlines) {
    left = std::min(left, p.x);
    right = std::max(right, p.x);
    low = std::min(low, p.y);
    high = std::max(high, p.y);
    EXPECT_NEAR(p.z, 0.26f, 1e-3);
  }
  EXPECT_NEAR(left, -right, 0.05f);
  EXPECT_NEAR(right - left, letters.width, 0.05f);
  EXPECT_NEAR(high - low, 1, 0.2f);
  // A wider word is wider
  EXPECT_GT(makeLetters("COSMIC HEX", 1, 0.5f, {}).width, letters.width * 2);
}

TEST(TitleSceneTest, TheSkysStarsAreOnItsSphere) {
  const Scene::MeshBuilder sky = makeSky(50, 40, 7);
  EXPECT_EQ(sky.triangleCount(), 100u); // (two a star)
  for (const Scene::Vertex &v : sky.data().vertices)
    EXPECT_NEAR(v.position.length(), 40, 0.3f);
}
