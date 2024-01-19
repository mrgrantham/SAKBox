#pragma once
#include <GL/glew.h>
#include <spdlog/spdlog.h>

static void checkOpenGLErrors(const std::string &&name = "") {
  while (GLenum error = glGetError()) {
    spdlog::error("OPENGL [{0}] ERROR: (0x{1:x})", name, error);
  }
}