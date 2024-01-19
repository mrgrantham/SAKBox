#include <ElementBufferObject.h>

ElementBufferObject::ElementBufferObject(GLuint *indices, GLsizeiptr size) {
  glGenBuffers(1, &ID_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
}

void ElementBufferObject::bind() { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID_); }
void ElementBufferObject::unbind() { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }

void ElementBufferObject::destroy() { glDeleteBuffers(1, &ID_); }