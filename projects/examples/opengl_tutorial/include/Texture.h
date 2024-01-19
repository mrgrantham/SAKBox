#pragma once

#include <GL/glew.h>
#include <ShaderProgram.h>
#include <string>

class Texture {
private:
  GLuint ID_;
  GLenum type_;

public:
  Texture(const std::string &image, GLenum textureType, GLenum slot,
          GLenum format, GLenum pixelType);

  void textureUnit(ShaderProgram &shaderProgram, const char *uniform,
                   GLuint unit);
  void bind();
  void unbind();
  void destroy();

  GLuint ID();
};