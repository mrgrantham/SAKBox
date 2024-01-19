#pragma once
#include <GL/glew.h>
#include <spdlog/spdlog.h>

// Helper function to debug and check for errors following any opengl calls

static void checkOpenGLErrors(const std::string &&name = "") {
  while (GLenum error = glGetError()) {
    spdlog::error("OPENGL [{0}] ERROR: (0x{1:x})", name, error);
  }
}