// The OpenGL backend's part of a desktop Display (WindowBackend.hpp): a 3.3
// core context with 4x multisampling, made current when the window opens,
// and buffers swapped to show a frame
#include "Backend.hpp"

#include <GLFW/glfw3.h>

#include <ScratchPadd/Scene/Desktop/WindowBackend.hpp>

#include <memory>

namespace ScratchPadd::Scene::WindowBackend {

const std::string kName = "OpenGL 3.3";

void hint() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS
  glfwWindowHint(GLFW_SAMPLES, 4);
}

std::string attach(Display &display) {
  glfwMakeContextCurrent(display.glfw());
  glfwSwapInterval(display.visible() ? 1 : 0); // wait for the screen's refresh, if shown
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK)
    return "GLEW couldn't load OpenGL's functions";
  display.setSurface(std::make_shared<WindowSurface>());
  return "";
}

void show(Display &display) {
  auto &surface = *static_cast<WindowSurface *>(display.surface());
  if (!display.visible()) {
    // A hidden window keeps a copy of each frame, for Desktop::readPixel
    surface.width = display.pixelWidth();
    surface.height = display.pixelHeight();
    surface.lastFrame.resize(static_cast<std::size_t>(surface.width) * surface.height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, surface.width, surface.height, GL_RGBA, GL_UNSIGNED_BYTE,
                 surface.lastFrame.data());
  }
  glfwSwapBuffers(display.glfw());
  // GLFW 3.3 on macOS can render into a quarter of a high-DPI window until
  // the window moves once
  if (!surface.nudged) {
    int x = 0, y = 0;
    glfwGetWindowPos(display.glfw(), &x, &y);
    glfwSetWindowPos(display.glfw(), x + 1, y);
    surface.nudged = true;
  }
}

} // namespace ScratchPadd::Scene::WindowBackend
