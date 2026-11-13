// Debug.hpp - the 3DS backend's debug mode, for finding what's wrong on the
// hardware, where a shader can't log and a hung GPU says nothing:
//
// - Settings: what's drawn (each part can be left out: meshes, translucent
//   ones, lines, wireframes, the overlay, fog, stereo, and culling), how it's coloured
//   (the view: as usual, the normals, the lighting alone, the vertex colours
//   alone, or overdraw: how many times each pixel is drawn), finding a hang
//   (each frame sends only its first few draws, one more every half second,
//   so the frame that hangs the GPU ends with the draw that does), and a probe
//   reading the top screen's pixel under a cross.
// - Watchdog: a thread that notices the GPU hasn't finished a frame in two
//   seconds, and says which draws it was sent last, on the bottom screen's
//   console and in sdmc:/scene-3ds-hang.txt.
// - Panel: the settings on the bottom screen's console, with the probe's
//   reading and the last frame's figures.
//
//   Scene::Debug::Watchdog watchdog(renderer);
//   Scene::Debug::Panel panel(renderer);
//   // each frame: if the panel is shown, the buttons are its
//   if (!panel.handle(lastFrame, pad)) { /* the app's */ }   // Input::Gamepads
//   panel.draw();
//
// 3DS only. See the backend's README section and DESIGN.md.
#pragma once

#include <ScratchPadd/Input/Gamepad.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <atomic>
#include <cstdint>
#include <thread>

namespace ScratchPadd::Scene::Debug {

// How meshes are coloured
enum class View : std::uint8_t {
  normal,        // as usual
  normals,       // each normal as a colour: x, y, z from -1 to 1 as 0 to 255
  lighting,      // the light each vertex gets, without its colours
  vertexColours, // the vertex colours times the material's, unlit
  overdraw,      // how many times each pixel is drawn: brighter for more
};
const char *nameOf(View view);

struct Settings {
  bool meshes = true, translucent = true, lines = true, wireframes = true, overlay = true,
       fog = true, stereo = true;
  // Back faces and meshes out of view culled (off: everything drawn, to
  // see what culling saves, or find a mesh wound the wrong way)
  bool culling = true;
  View view = View::normal;
  // Each frame sends only its first few draws, one more every half second
  // (from one, when it's turned on): the frame that hangs the GPU ends with
  // the draw that does, which the watchdog names
  bool findHang = false;
  // A cross on the top screen, the pixel under it read back
  bool probe = false;
  int probeX = 200, probeY = 120; // top screen pixels, from the top-left
};

void set(Renderer &renderer, const Settings &settings);
const Settings &settingsOf(const Renderer &renderer);
// The top screen's pixel under the probe, as shown a frame or two ago
Colour probed(const Renderer &renderer);
// Finding a hang: how many draws a frame are sent now, of how many it has
struct HangSearch {
  unsigned sending, of;
};
HangSearch hangSearch(const Renderer &renderer);
// How many times overdraw adds up to full brightness (the view's scale)
inline constexpr int kOverdrawSteps = 12;

// Watches the renderer from a thread of its own while it lives
class Watchdog {
public:
  explicit Watchdog(Renderer &renderer);
  ~Watchdog();
  Watchdog(const Watchdog &) = delete;
  Watchdog &operator=(const Watchdog &) = delete;

private:
  void watch();
  Renderer &renderer_;
  std::atomic<bool> stopping_{false};
  std::thread thread_;
};

// The settings on the bottom screen's console (40 x 30), shown or hidden
// as the app chooses: up and down choose a setting, A or left and right
// change it, and while the probe is on the circle pad moves it
class Panel {
public:
  explicit Panel(Renderer &renderer) : renderer_(renderer) {}

  // This frame's pad, and the last frame's copy (what was pressed since);
  // whether the panel took them (it does while it's shown)
  bool handle(const Input::Gamepad &before, const Input::Gamepad &now);
  // Prints the panel on the current console, while it's shown
  void draw();
  bool visible() const { return visible_; }
  void setVisible(bool visible);

private:
  void change(int row, int direction);
  Renderer &renderer_;
  bool visible_ = false;
  bool cleared_ = false;
  int row_ = 0;
};

} // namespace ScratchPadd::Scene::Debug
