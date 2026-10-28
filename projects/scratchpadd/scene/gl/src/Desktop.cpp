#include "Backend.hpp"

#include <ScratchPadd/Scene/GL/Desktop.hpp>

namespace ScratchPadd::Scene::Desktop {

void setVertexSnap(Renderer &renderer, float worldGrid, float pixelGrid) {
  renderer.backend().worldSnap = worldGrid;
  renderer.backend().screenSnap = pixelGrid;
}

ShaderPanel::ShaderPanel(int width, int height, const std::string &vertexSource,
                         const std::string &fragmentSource, const MeshData &mesh)
    : width_(width), height_(height) {
  glGenFramebuffers(1, &framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  // RGB: a shader's alpha is ignored, so a panel is never see-through
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
  glGenRenderbuffers(1, &depth_);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  program_ = linkProgram(vertexSource.c_str(), fragmentSource.c_str());
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &ebo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size_bytes()),
               mesh.vertices.data(), GL_STATIC_DRAW);
  describeVertices();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size_bytes()),
               mesh.indices.data(), GL_STATIC_DRAW);
  glBindVertexArray(0);
  indexCount_ = static_cast<int>(mesh.indices.size());
}

ShaderPanel::~ShaderPanel() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &vbo_);
  glDeleteBuffers(1, &ebo_);
  glDeleteProgram(program_);
  glDeleteRenderbuffers(1, &depth_);
  glDeleteTextures(1, &texture_);
  glDeleteFramebuffers(1, &framebuffer_);
}

void ShaderPanel::render(float seconds, float mouseX, float mouseY, bool mouseDown, Colour colour) {
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glViewport(0, 0, width_, height_);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(program_);
  glUniform1f(glGetUniformLocation(program_, "iTime"), seconds);
  glUniform2f(glGetUniformLocation(program_, "iResolution"), static_cast<float>(width_),
              static_cast<float>(height_));
  glUniform4f(glGetUniformLocation(program_, "iMouse"), mouseX, mouseY, mouseDown ? 1.0f : 0.0f,
              0.0f);
  glUniform4f(glGetUniformLocation(program_, "uniform_color"), colour.r / 255.0f, colour.g / 255.0f,
              colour.b / 255.0f, colour.a / 255.0f);
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

} // namespace ScratchPadd::Scene::Desktop
