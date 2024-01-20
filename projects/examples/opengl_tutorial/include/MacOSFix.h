#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

// This is a hacky fix for adjusting the opengl context to correctly size to the
// window There appear to be some issues around high DPI on mac.

// The correct solution is probably something like this
// https://github.com/kovidgoyal/kitty/commit/b82e74f99ae30599cd88ef6a1216625b57a0414a
// Desccribed here
// https://github.com/glfw/glfw/issues/1334#issuecomment-425298183 However I did
// not want to setup Objective-C or Objective-C++ compiling in this binary so I
// am using the following instead

// Must be called right after glViewport
void fix_render_on_mac(GLFWwindow *window) {
#ifdef __APPLE__
  spdlog::info("Fixing the window on macOS");
  static bool macMoved = false;
  if (!macMoved) {
    int x, y;
    glfwGetWindowPos(window, &x, &y);
    glfwSetWindowPos(window, ++x, y);
    macMoved = true;
  }
#endif
}