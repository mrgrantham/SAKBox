#pragma once
#include <string>

#include <GL/glew.h>

class ElementBufferObject {
private:
  GLuint ID_;

public:
  ElementBufferObject(GLuint *indices, GLsizeiptr size);

  void bind();
  void unbind();
  void destroy();
};