#include <VertexBufferObject.h>

VertexBufferObject::VertexBufferObject(GLfloat *vertices, GLsizeiptr size) {
  glGenBuffers(1, &ID_);
  glBindBuffer(GL_ARRAY_BUFFER, ID_);
  glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

void VertexBufferObject::bind() { glBindBuffer(GL_ARRAY_BUFFER, ID_); }
void VertexBufferObject::unbind() { glBindBuffer(GL_ARRAY_BUFFER, 0); }

void VertexBufferObject::destroy() { glDeleteBuffers(1, &ID_); }