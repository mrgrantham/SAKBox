#pragma once
#include <string>

#include <GL/glew.h>

static std::string GetFileContents(const std::string &pathString);

// Handles generating the shader program from the provided vertex and fragment
// shaders

class ShaderProgram {
public:
  GLuint ID_;
  ShaderProgram(const std::string &vertexFile, const std::string &fragmentFile);

  void activate();
  void destroy();
  GLuint ID();
};