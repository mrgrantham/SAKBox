#include "Backend.hpp"

#include <ScratchPadd/Scene/Desktop/Desktop.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>

#include <algorithm>

namespace ScratchPadd::Scene::Desktop {

void setVertexSnap(Renderer &renderer, float worldGrid, float pixelGrid) {
  renderer.backend().worldSnap = worldGrid;
  renderer.backend().screenSnap = pixelGrid;
}

Colour readPixel(const Renderer &renderer, float x, float y) {
  const Display &display = renderer.backend().display;
  const auto *surface = static_cast<const WindowSurface *>(display.surface());
  if (!surface || surface->lastFrame.empty())
    return {};
  // The copy is bottom row first, as OpenGL reads it
  const float scale = static_cast<float>(surface->width) / static_cast<float>(display.width());
  const int px = std::clamp(static_cast<int>(x * scale), 0, surface->width - 1);
  const int py =
      std::clamp(surface->height - 1 - static_cast<int>(y * scale), 0, surface->height - 1);
  const unsigned char *rgba =
      &surface->lastFrame[(static_cast<std::size_t>(py) * surface->width + px) * 4];
  return {rgba[0], rgba[1], rgba[2], rgba[3]};
}

struct ShaderPanel::State {
  GLuint framebuffer = 0, texture = 0, depth = 0;
  GLuint program = 0, vao = 0, vbo = 0, ebo = 0;
  GLsizei indexCount = 0;
};

ShaderPanel::ShaderPanel(int width, int height, const std::string &vertexSource,
                         const std::string &fragmentSource, const MeshData &mesh)
    : width_(width), height_(height), state_(std::make_unique<State>()) {
  State &s = *state_;
  glGenFramebuffers(1, &s.framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, s.framebuffer);
  glGenTextures(1, &s.texture);
  glBindTexture(GL_TEXTURE_2D, s.texture);
  // RGB: a shader's alpha is ignored, so a panel is never see-through
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.texture, 0);
  glGenRenderbuffers(1, &s.depth);
  glBindRenderbuffer(GL_RENDERBUFFER, s.depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s.depth);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  s.program = linkProgram(vertexSource.c_str(), fragmentSource.c_str(), &error_);
  glGenVertexArrays(1, &s.vao);
  glGenBuffers(1, &s.vbo);
  glGenBuffers(1, &s.ebo);
  glBindVertexArray(s.vao);
  glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size_bytes()),
               mesh.vertices.data(), GL_STATIC_DRAW);
  describeVertices();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.ebo);
  const std::vector<std::uint32_t> indices = triangleList(mesh);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)), indices.data(),
               GL_STATIC_DRAW);
  glBindVertexArray(0);
  s.indexCount = static_cast<GLsizei>(indices.size());
}

ShaderPanel::~ShaderPanel() {
  State &s = *state_;
  glDeleteVertexArrays(1, &s.vao);
  glDeleteBuffers(1, &s.vbo);
  glDeleteBuffers(1, &s.ebo);
  glDeleteProgram(s.program);
  glDeleteRenderbuffers(1, &s.depth);
  glDeleteTextures(1, &s.texture);
  glDeleteFramebuffers(1, &s.framebuffer);
}

std::uint64_t ShaderPanel::imageId() const { return state_->texture; }

void ShaderPanel::render(float seconds, float mouseX, float mouseY, bool mouseDown, Colour colour) {
  const State &s = *state_;
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glBindFramebuffer(GL_FRAMEBUFFER, s.framebuffer);
  glViewport(0, 0, width_, height_);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (ok()) {
    glUseProgram(s.program);
    glUniform1f(glGetUniformLocation(s.program, "iTime"), seconds);
    glUniform2f(glGetUniformLocation(s.program, "iResolution"), static_cast<float>(width_),
                static_cast<float>(height_));
    glUniform4f(glGetUniformLocation(s.program, "iMouse"), mouseX, mouseY, mouseDown ? 1.0f : 0.0f,
                0.0f);
    glUniform4f(glGetUniformLocation(s.program, "uniform_color"), colour.r / 255.0f,
                colour.g / 255.0f, colour.b / 255.0f, colour.a / 255.0f);
    glBindVertexArray(s.vao);
    glDrawElements(GL_TRIANGLES, s.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

} // namespace ScratchPadd::Scene::Desktop
