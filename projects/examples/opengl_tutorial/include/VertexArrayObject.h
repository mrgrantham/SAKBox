#pragma once

#include <GL/glew.h>
#include <VertexBufferObject.h>

class VertexArrayObject {
private:
  GLuint ID_;

public:
  VertexArrayObject();
  void linkVertexBufferObject(VertexBufferObject vertexBufferObject,
                              GLuint layout);

  void bind();
  void unbind();
  void destroy();
};