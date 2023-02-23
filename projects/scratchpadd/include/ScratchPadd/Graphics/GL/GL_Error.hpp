#pragma once
#include <GL/glew.h> 

static void glfw_error_callback(int error, const char* description)
{
    spdlog::error("Glfw Error {}: {}\n", error, description);
}

static void clearOpenGLErrors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

static void checkOpenGLErrors(const std::string &&name = "") {
  while (GLenum error = glGetError()) {
    spdlog::error("OPENGL [{0}] ERROR: (0x{1:x})", name, error);
  }
}