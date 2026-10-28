// Backend.hpp - the OpenGL backend's state behind Scene::Renderer (private
// to the backend; Desktop.hpp's extras reach it through Renderer::backend())
#pragma once

#include <GL/glew.h>

#include <ScratchPadd/Scene/GL/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <glm/glm.hpp>

#include <vector>

namespace ScratchPadd::Scene {

// Compiles and links a vertex and fragment shader; on failure, writes the
// log to stderr and returns the program anyway (it draws nothing)
GLuint linkProgram(const char *vertexSource, const char *fragmentSource);

// Sets up `vao`'s attributes for Scene::Vertex in the bound GL_ARRAY_BUFFER:
// 0 position, 1 normal, 2 colour (0-1)
void describeVertices();

struct Renderer::Backend {
  explicit Backend(Display &display);
  ~Backend();

  struct LoadedMesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
  };
  // A translucent draw, kept until the end of the frame
  struct Later {
    Mesh mesh;
    Transform where;
    Material material;
    float distance; // from the eye, to draw the farthest first
  };

  void drawNow(Mesh mesh, const Transform &where, const Material &material);
  void drawOverlay(const Overlay &overlay);

  Display &display;
  std::vector<LoadedMesh> meshes; // by Mesh::id - 1; unloaded ones have vao 0
  std::vector<Later> later;

  GLuint program = 0;
  struct {
    GLint model, normalMatrix, viewProj, worldSnap, screenSnap, viewport, colour, lit, ambient,
        lightCount, lightDirections, lightColours, fogColour, fogDensity;
  } uniform{};
  GLuint lineVao = 0, lineVbo = 0;
  GLuint overlayProgram = 0, overlayVao = 0, overlayVbo = 0;
  GLint overlayScreen = -1;

  glm::vec3 eye{0.0f};
  glm::mat4 viewProj{1.0f}; // this frame's
  // Desktop.hpp's vertex snapping, for the rest of the frame
  float worldSnap = 0, screenSnap = 0;
};

} // namespace ScratchPadd::Scene
