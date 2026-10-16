#pragma once
#include <GL/glew.h>

inline void glfw_error_callback(int error, const char *description) {
  spdlog::error("Glfw Error {}: {}\n", error, description);
}

inline void clearOpenGLErrors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

inline void checkOpenGLErrors(const std::string &&name = "") {
  while (GLenum error = glGetError()) {
    spdlog::error("OPENGL [{0}] ERROR: (0x{1:x})", name, error);
  }
}