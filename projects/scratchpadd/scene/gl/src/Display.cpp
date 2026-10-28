// GLEW must be included before GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <ScratchPadd/Scene/GL/Display.hpp>

#include <utility>

namespace ScratchPadd::Scene {

namespace {
// GLFW's code for `key`: letters, digits and space are already GLFW's
int glfwKey(Key key) {
  switch (key) {
  case Key::Escape:
    return GLFW_KEY_ESCAPE;
  case Key::Enter:
    return GLFW_KEY_ENTER;
  case Key::Tab:
    return GLFW_KEY_TAB;
  case Key::Left:
    return GLFW_KEY_LEFT;
  case Key::Right:
    return GLFW_KEY_RIGHT;
  case Key::Up:
    return GLFW_KEY_UP;
  case Key::Down:
    return GLFW_KEY_DOWN;
  default:
    return static_cast<int>(key);
  }
}

// Windows open, so GLFW is initialised while any is (it's terminated with
// the last)
int openWindows = 0;
} // namespace

Display::Display(int width, int height, const std::string &title, bool visible) {
  if (openWindows == 0 && !glfwInit()) {
    error_ = "GLFW couldn't start (no display?)";
    return;
  }
  ++openWindows;
  glfwStarted_ = true;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
  window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!window_) {
    error_ = "no window: OpenGL 3.3 isn't available";
    return;
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(visible ? 1 : 0); // wait for the screen's refresh, if shown
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    error_ = "GLEW couldn't load OpenGL's functions";
    glfwDestroyWindow(window_);
    window_ = nullptr;
    return;
  }
  glfwSetWindowUserPointer(window_, this);
  glfwSetKeyCallback(window_, keyCallback);
}

Display::~Display() {
  if (window_)
    glfwDestroyWindow(window_);
  if (glfwStarted_ && --openWindows == 0)
    glfwTerminate();
}

void Display::keyCallback(GLFWwindow *window, int key, int, int action, int) {
  if (action == GLFW_PRESS)
    static_cast<Display *>(glfwGetWindowUserPointer(window))->pressing_.insert(key);
}

bool Display::isOpen() const { return window_ && !glfwWindowShouldClose(window_); }

void Display::close() {
  if (window_)
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

void Display::pollEvents() {
  glfwPollEvents();
  pressed_ = std::exchange(pressing_, {});
  if (!keyboardForApp())
    pressed_.clear();
}

bool Display::keyDown(Key key) const {
  return window_ && keyboardForApp() && glfwGetKey(window_, glfwKey(key)) == GLFW_PRESS;
}

bool Display::mouseDown(MouseButton button) const {
  const int glfwButton = button == MouseButton::Left    ? GLFW_MOUSE_BUTTON_LEFT
                         : button == MouseButton::Right ? GLFW_MOUSE_BUTTON_RIGHT
                                                        : GLFW_MOUSE_BUTTON_MIDDLE;
  return window_ && mouseForApp() && glfwGetMouseButton(window_, glfwButton) == GLFW_PRESS;
}

float Display::cursorX() const {
  double x = 0, y = 0;
  if (window_)
    glfwGetCursorPos(window_, &x, &y);
  return static_cast<float>(x);
}

float Display::cursorY() const {
  double x = 0, y = 0;
  if (window_)
    glfwGetCursorPos(window_, &x, &y);
  return static_cast<float>(y);
}

double Display::seconds() const { return glfwGetTime(); }

int Display::width() const {
  int w = 0, h = 0;
  if (window_)
    glfwGetWindowSize(window_, &w, &h);
  return w;
}

int Display::height() const {
  int w = 0, h = 0;
  if (window_)
    glfwGetWindowSize(window_, &w, &h);
  return h;
}

int Display::pixelWidth() const {
  int w = 0, h = 0;
  if (window_)
    glfwGetFramebufferSize(window_, &w, &h);
  return w;
}

int Display::pixelHeight() const {
  int w = 0, h = 0;
  if (window_)
    glfwGetFramebufferSize(window_, &w, &h);
  return h;
}

void Display::present() {
  if (!window_)
    return;
  for (const auto &[id, draw] : drawLast_)
    draw();
  glfwSwapBuffers(window_);
  // GLFW 3.3 on macOS can render into a quarter of a high-DPI window until
  // the window moves once
  if (!nudged_) {
    int x = 0, y = 0;
    glfwGetWindowPos(window_, &x, &y);
    glfwSetWindowPos(window_, x + 1, y);
    nudged_ = true;
  }
}

} // namespace ScratchPadd::Scene
