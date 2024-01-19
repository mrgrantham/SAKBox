#pragma once
#include <string>

#include <GL/glew.h>

class VertexBufferObject {
private:
  GLuint ID_;

public:
  VertexBufferObject(GLfloat *vertices, GLsizeiptr size);

  void bind();
  void unbind();
  void destroy();
};