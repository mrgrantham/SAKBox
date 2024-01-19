#include <Error.h>
#include <VertexArrayObject.h>

VertexArrayObject::VertexArrayObject() { glGenVertexArrays(1, &ID_); }

void VertexArrayObject::linkVertexBufferObject(
    VertexBufferObject vertexBufferObject, GLuint layout) {
  vertexBufferObject.bind();
  glVertexAttribPointer(layout, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                        (void *)0);
  glEnableVertexAttribArray(layout);
  checkOpenGLErrors("glEnableVertexAttribArray");
  vertexBufferObject.unbind();
}

void VertexArrayObject::bind() { glBindVertexArray(ID_); }
void VertexArrayObject::unbind() { glBindVertexArray(0); }
void VertexArrayObject::destroy() { glDeleteVertexArrays(1, &ID_); }