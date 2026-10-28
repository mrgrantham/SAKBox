// Display.hpp - the OpenGL backend's Display: a GLFW window with an OpenGL
// 3.3 core context, and its keyboard and mouse. Open it on the main thread
// (macOS insists), in a main-thread padd, and draw into it with a
// Scene::Renderer.
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;

namespace ScratchPadd::Scene {

// The keys apps use; letters and digits by their character (keyFor('W'))
enum class Key : int {
  Space = ' ',
  Escape = 256,
  Enter,
  Tab,
  Left,
  Right,
  Up,
  Down,
};
constexpr Key keyFor(char letterOrDigit) {
  return static_cast<Key>(letterOrDigit >= 'a' && letterOrDigit <= 'z' ? letterOrDigit - 'a' + 'A'
                                                                       : letterOrDigit);
}

enum class MouseButton { Left, Right, Middle };

class Display {
public:
  // Opens a window `width` x `height` points. If it can't (no display, no
  // OpenGL 3.3), isOpen() is false and error() says why. `visible` false
  // opens it hidden, for drawing offscreen (tests).
  Display(int width, int height, const std::string &title, bool visible = true);
  ~Display();
  Display(const Display &) = delete;
  Display &operator=(const Display &) = delete;

  // False once it's been closed (by the user, close(), or Escape if
  // closeOnEscape) or if it never opened
  bool isOpen() const;
  void close();
  const std::string &error() const { return error_; }

  // Reads the keyboard and mouse: call once a frame, before looking at them
  void pollEvents();
  bool keyDown(Key key) const;
  // Whether `key` went down since the last pollEvents (each press once)
  bool keyPressed(Key key) const { return pressed_.contains(static_cast<int>(key)); }
  bool mouseDown(MouseButton button) const;
  // Where the cursor is, in points from the top-left
  float cursorX() const;
  float cursorY() const;
  // Whether keys and the mouse are for the app, not a panel on top (set by
  // debug_ui while ImGui wants them)
  bool keyboardForApp() const { return !keyboardTaken_; }
  bool mouseForApp() const { return !mouseTaken_; }
  void setTaken(bool keyboard, bool mouse) {
    keyboardTaken_ = keyboard;
    mouseTaken_ = mouse;
  }

  // Seconds since it opened
  double seconds() const;
  // Its size in points, and in pixels (twice as many on a Retina screen)
  int width() const;
  int height() const;
  int pixelWidth() const;
  int pixelHeight() const;

  // Shows the frame drawn (Renderer::endFrame calls it): first runs each
  // drawing added with drawLast (debug_ui's panels), then swaps buffers,
  // waiting for the screen's refresh
  void present();
  // Adds a drawing for present() to run last; returns a number to stop it
  // with stopDrawingLast (before whatever it draws is gone)
  int drawLast(std::function<void()> draw) {
    drawLast_.push_back({++lastDrawingId_, std::move(draw)});
    return lastDrawingId_;
  }
  void stopDrawingLast(int id) {
    std::erase_if(drawLast_, [id](const auto &drawing) { return drawing.first == id; });
  }

  // The GLFW window, for desktop libraries that need it (ImGui's backend)
  GLFWwindow *glfw() const { return window_; }

private:
  static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

  GLFWwindow *window_ = nullptr;
  std::string error_;
  std::set<int> pressed_, pressing_; // since the last poll; arriving
  std::vector<std::pair<int, std::function<void()>>> drawLast_;
  int lastDrawingId_ = 0;
  bool keyboardTaken_ = false, mouseTaken_ = false;
  bool glfwStarted_ = false; // this window counts towards GLFW staying started
  bool nudged_ = false;
};

} // namespace ScratchPadd::Scene
