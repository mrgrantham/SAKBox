// Scene.hpp - ScratchPadd's scene layer: meshes, their transforms, a
// camera, materials, lines, and an overlay, drawn the same way on every
// platform. Each platform has a backend (OpenGL on desktops, so far) that
// defines Renderer's methods and its Display; a program links one, chosen
// when it's built. See DESIGN.md.
//
//   Display &display = ...;          // the backend's: a window, a screen
//   Renderer renderer(display);
//   Mesh box = renderer.load(MeshBuilder().box({}, {1, 1, 1}).data());
//   // each frame:
//   renderer.beginFrame(camera, environment);
//   renderer.draw(box, Transform::rotation({0, 1, 0}, angle), {});
//   renderer.overlay().text(10, 10, "Hello", {});
//   renderer.endFrame();             // shows it, at the display's refresh
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Overlay.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <cstdint>
#include <memory>
#include <span>

namespace ScratchPadd::Scene {

// Where frames go: a window on desktops, a screen on the DS. Each backend
// defines it (for OpenGL, ScratchPadd/Scene/GL/Window.hpp).
class Display;

// A loaded mesh, drawn by handle. 0 is no mesh.
struct Mesh {
  std::uint32_t id = 0;
  explicit operator bool() const { return id != 0; }
};

// What a backend can and can't do, for code shared between platforms
struct Capabilities {
  unsigned lights = Environment::kMostLights;
  // The most polygons a frame can draw, or 0 for no limit (the DS: 2048)
  unsigned polygonsPerFrame = 0;
};

class Renderer {
public:
  explicit Renderer(Display &display);
  ~Renderer();
  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;

  // Uploads a mesh to draw by handle; `mesh`'s data can go afterwards
  Mesh load(const MeshData &mesh);
  void unload(Mesh mesh);

  // Starts a frame: clears it to the environment's background
  void beginFrame(const Camera &camera, const Environment &environment);
  void draw(Mesh mesh, const Transform &where, const Material &material);
  // Lines, unlit, from ends[0] to ends[1], ends[2] to ends[3], and so on
  void drawLines(std::span<const Vector3> ends, Colour colour);
  // Text and rectangles over this frame's 3D (cleared each frame)
  Overlay &overlay() { return overlay_; }
  // Where `point` is on screen this frame, in the overlay's points; false
  // if it's behind the camera
  bool toScreen(const Vector3 &point, float &x, float &y) const;
  // Draws what's left (translucent meshes, farthest first, then the
  // overlay) and shows the frame, waiting for the display's refresh
  void endFrame();

  Capabilities capabilities() const;

  // The backend's own state, for its extras (on desktops, Desktop.hpp)
  struct Backend;
  Backend &backend() { return *backend_; }

private:
  std::unique_ptr<Backend> backend_;
  Overlay overlay_;
};

} // namespace ScratchPadd::Scene
