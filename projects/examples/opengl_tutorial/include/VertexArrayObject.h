#pragma once

#include <GL/glew.h>
#include <VertexBufferObject.h>

class VertexArrayObject {
private:
  GLuint ID_;

public:
  VertexArrayObject();
  void linkAttributes(VertexBufferObject vertexBufferObject, GLuint layout,
                      GLuint componentCount, GLenum type, GLsizeiptr stride,
                      void *offset);

  void bind();
  void unbind();
  void destroy();
};