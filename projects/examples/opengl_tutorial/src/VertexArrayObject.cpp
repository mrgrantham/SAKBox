#include <Error.h>
#include <VertexArrayObject.h>

VertexArrayObject::VertexArrayObject() { glGenVertexArrays(1, &ID_); }

void VertexArrayObject::linkAttributes(VertexBufferObject vertexBufferObject,
                                       GLuint layout, GLuint componentCount,
                                       GLenum type, GLsizeiptr stride,
                                       void *offset) {
  vertexBufferObject.bind();
  glVertexAttribPointer(layout, componentCount, type, GL_FALSE, stride, offset);
  glEnableVertexAttribArray(layout);
  checkOpenGLErrors("glEnableVertexAttribArray");
  vertexBufferObject.unbind();
}

void VertexArrayObject::bind() { glBindVertexArray(ID_); }
void VertexArrayObject::unbind() { glBindVertexArray(0); }
void VertexArrayObject::destroy() { glDeleteVertexArrays(1, &ID_); }