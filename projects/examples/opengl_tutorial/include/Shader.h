#pragma once
#include <string>

#include <GL/glew.h>

static std::string GetFileContents(const std::string &pathString);

class Shader {
public:
  GLuint ID_;
  Shader(const std::string &vertexFile, const std::string &fragmentFile);

  void activate();
  void destroy();
};