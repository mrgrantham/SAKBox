// The OpenGL backend, drawing into a hidden window: each test draws a frame
// and reads pixels back just before it's shown (Display::drawLast runs
// then). Without a display or OpenGL 3.3 the tests are skipped.
#include <GL/glew.h>

#include <ScratchPadd/Scene/GL/Desktop.hpp>
#include <ScratchPadd/Scene/GL/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <functional>

using namespace ScratchPadd::Scene;

namespace {

using Pixel = std::array<int, 3>;

// A window and renderer, and the camera looking at the origin from +z
class RendererTest : public testing::Test {
protected:
  void SetUp() override {
    if (!display.isOpen())
      GTEST_SKIP() << "no OpenGL window: " << display.error();
    renderer = std::make_unique<Renderer>(display);
    camera.eye = {0, 0, 5};
    camera.target = {0, 0, 0};
    environment.background = {0, 0, 40};
    box = renderer->load(MeshBuilder().box({}, {1, 1, 1}).data());
  }

  // Draws a frame with `draw`, and reads the pixels at `points` (in points
  // from the top-left) just before it's shown
  std::vector<Pixel> frame(const std::function<void()> &draw,
                           std::initializer_list<std::array<float, 2>> points) {
    std::vector<Pixel> pixels;
    const int reader = display.drawLast([&] {
      const float scale = static_cast<float>(display.pixelWidth()) / display.width();
      for (const auto &[x, y] : points) {
        unsigned char rgba[4] = {};
        glReadPixels(static_cast<GLint>(x * scale),
                     static_cast<GLint>(display.pixelHeight() - 1 - y * scale), 1, 1, GL_RGBA,
                     GL_UNSIGNED_BYTE, rgba);
        pixels.push_back({rgba[0], rgba[1], rgba[2]});
      }
    });
    renderer->beginFrame(camera, environment);
    draw();
    renderer->endFrame();
    display.stopDrawingLast(reader);
    return pixels;
  }

  float centreX() const { return display.width() / 2.0f; }
  float centreY() const { return display.height() / 2.0f; }

  Display display{320, 240, "Renderer_test", false};
  std::unique_ptr<Renderer> renderer;
  Camera camera;
  Environment environment;
  Mesh box;
};

void expectNear(const Pixel &actual, const Pixel &expected, int tolerance = 3) {
  for (int i = 0; i < 3; ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "channel " << i;
}

} // namespace

// The background fills what nothing covers; an unlit mesh is its vertex
// colours times its material's colour, exactly.
TEST_F(RendererTest, UnlitMeshesAreTheirColours) {
  const Material red{.shading = Material::Shading::unlit, .colour = {255, 0, 0}};
  const auto pixels = frame([&] { renderer->draw(box, Transform::identity(), red); },
                            {{centreX(), centreY()}, {5, 5}});
  expectNear(pixels[0], {255, 0, 0});
  expectNear(pixels[1], {0, 0, 40});

  // The vertex colours are multiplied by the material's
  const Mesh grey = renderer->load(MeshBuilder().box({}, {1, 1, 1}, {128, 128, 128}).data());
  const auto tinted =
      frame([&] { renderer->draw(grey, Transform::identity(), red); }, {{centreX(), centreY()}});
  expectNear(tinted[0], {128, 0, 0});
}

// A lit face is the ambient light plus each light shining on it, times its
// colour; a face the light doesn't reach gets only the ambient.
TEST_F(RendererTest, LitMeshesAreLitByAmbientAndLights) {
  environment.ambient = {50, 50, 50};
  environment.addLight({.direction = {0, 0, -1}, .colour = {100, 100, 100}}); // towards -z
  const auto facing =
      frame([&] { renderer->draw(box, Transform::identity(), {}); }, {{centreX(), centreY()}});
  expectNear(facing[0], {150, 150, 150});

  environment.lights[0].direction = {0, 0, 1}; // from behind: the front face is unlit
  const auto away =
      frame([&] { renderer->draw(box, Transform::identity(), {}); }, {{centreX(), centreY()}});
  expectNear(away[0], {50, 50, 50});
}

// A translucent mesh blends over what's behind it by its alpha, even when
// drawn before it; the solid one behind still shows.
TEST_F(RendererTest, TranslucentMeshesBlendOverSolidOnes) {
  const Material green{.shading = Material::Shading::unlit, .colour = {0, 255, 0}, .alpha = 15};
  const Material red{.shading = Material::Shading::unlit, .colour = {255, 0, 0}};
  const auto pixels = frame(
      [&] {
        renderer->draw(box, Transform::translation({0, 0, 1.5f}), green); // in front, first
        renderer->draw(box, Transform::identity(), red);
      },
      {{centreX(), centreY()}});
  const float a = 15 / 31.0f;
  expectNear(pixels[0], {static_cast<int>(255 * (1 - a)), static_cast<int>(255 * a), 0}, 4);
}

// Fog fades a far mesh almost to the fog's colour, and leaves a near one
// nearly as it is.
TEST_F(RendererTest, FogFadesFarMeshes) {
  environment.fog = {.colour = {200, 200, 200}, .density = 0.05f};
  camera.farPlane = 500;
  const Material black{.shading = Material::Shading::unlit, .colour = {0, 0, 0}};
  const auto far = frame([&] { renderer->draw(box, Transform::translation({0, 0, -200}), black); },
                         {{centreX(), centreY()}});
  EXPECT_GT(far[0][0], 190);
  const auto near =
      frame([&] { renderer->draw(box, Transform::identity(), black); }, {{centreX(), centreY()}});
  EXPECT_LT(near[0][0], 40); // 1 - exp(-0.05 x 4) is about 0.18
}

// A wireframe mesh draws only its edges: the middle of its face is
// background. Lines are drawn in their colour.
TEST_F(RendererTest, WireframeAndLinesDrawEdgesOnly) {
  const Material wire{
      .shading = Material::Shading::unlit, .colour = {255, 255, 0}, .wireframe = true};
  const Vector3 ends[] = {{-10, 0, 0}, {10, 0, 0}};
  const auto pixels = frame(
      [&] {
        renderer->draw(box, Transform::translation({0, 0, 0}), wire);
        renderer->drawLines(ends, {0, 255, 255});
      },
      {{centreX() + 20, centreY() - 20}, {10, centreY()}});
  expectNear(pixels[0], {0, 0, 40}); // inside the face, off its diagonal
  // The line, across the window: 1 pixel wide, so multisampling blends it
  // part way into the background
  EXPECT_LT(pixels[1][0], 20);
  EXPECT_GT(pixels[1][1], 100);
  EXPECT_GT(pixels[1][2], 100);
}

// The overlay is drawn over the 3D, in points from the top-left.
TEST_F(RendererTest, OverlayDrawsOverTheScene) {
  const Material red{.shading = Material::Shading::unlit, .colour = {255, 0, 0}};
  const auto pixels = frame(
      [&] {
        renderer->draw(box, Transform::identity(), red);
        renderer->overlay().rectangle(centreX() - 5, centreY() - 5, 10, 10, {255, 255, 255});
        renderer->overlay().rectangle(0, 0, 20, 10, {0, 255, 0, 128});
      },
      {{centreX(), centreY()}, {10, 5}});
  expectNear(pixels[0], {255, 255, 255});
  expectNear(pixels[1], {0, 128, 20}, 4); // half green over the background
}

// Vertex snapping (Desktop.hpp) moves vertices to the grid: a box 0.4 wide
// snapped to a 1-unit world grid collapses to nothing.
TEST_F(RendererTest, VertexSnappingMovesVerticesToTheGrid) {
  const Mesh small = renderer->load(MeshBuilder().box({}, {0.2f, 0.2f, 0.2f}).data());
  const Material red{.shading = Material::Shading::unlit, .colour = {255, 0, 0}};
  const auto pixels = frame(
      [&] {
        Desktop::setVertexSnap(*renderer, 1.0f, 0.0f);
        renderer->draw(small, Transform::translation({0.1f, 0.1f, 0}), red);
      },
      {{centreX(), centreY()}});
  expectNear(pixels[0], {0, 0, 40});
}

// toScreen gives where a point is drawn, in the overlay's points: the
// point looked at is the window's centre, and one behind the camera isn't
// on screen.
TEST_F(RendererTest, ToScreenMatchesWhereThingsAreDrawn) {
  renderer->beginFrame(camera, environment);
  float x = 0, y = 0;
  ASSERT_TRUE(renderer->toScreen({0, 0, 0}, x, y));
  EXPECT_NEAR(x, centreX(), 0.5f);
  EXPECT_NEAR(y, centreY(), 0.5f);
  ASSERT_TRUE(renderer->toScreen({0, 1, 0}, x, y));
  EXPECT_LT(y, centreY()); // up is up the screen
  EXPECT_FALSE(renderer->toScreen({0, 0, 10}, x, y));
  renderer->endFrame();
}
