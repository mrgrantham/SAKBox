// Backend.hpp - the OpenGL backend's state behind Scene::Renderer (private
// to the backend; Desktop.hpp's extras reach it through Renderer::backend())
#pragma once

#include <GL/glew.h>

#include <ScratchPadd/Scene/Culling.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/OutlineHull.hpp>
#include <ScratchPadd/Scene/Primitives.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::Scene {

// Compiles and links a vertex and fragment shader; on failure, puts the log
// in `error` (or writes it to stderr, without one) and returns the program
// anyway (it draws nothing)
GLuint linkProgram(const char *vertexSource, const char *fragmentSource,
                   std::string *error = nullptr);

// What the OpenGL backend keeps with a window (Display::surface): whether
// it's been nudged (see Window.cpp), and in a hidden window a copy of the
// last frame, RGBA from the bottom row (Desktop::readPixel)
struct WindowSurface {
  bool nudged = false;
  int width = 0, height = 0;
  std::vector<unsigned char> lastFrame;
};

// Sets up `vao`'s attributes for Scene::Vertex in the bound GL_ARRAY_BUFFER:
// 0 position, 1 normal, 2 colour (0-1)
void describeVertices();

struct Renderer::Backend {
  explicit Backend(Display &display);
  ~Backend();

  struct LoadedMesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint hullVao = 0, hullVbo = 0; // its outline's hull (shares ebo)
    GLsizei indexCount = 0;
    // Drawn translucent whatever its material (see hasTranslucentVertices)
    bool translucent = false;
    FrameCounts cost; // what it costs the DS (Primitives.hpp), counted a draw
    Bounds bounds;    // for frustum culling (Culling.hpp)
    bool doubleSided = false;
  };
  // A translucent draw, kept until the end of the frame
  struct Later {
    Mesh mesh;
    Transform where;
    Material material;
    float distance; // from the eye, to draw the farthest first
  };

  void drawNow(Mesh mesh, const Transform &where, const Material &material);
  void drawOutline(const LoadedMesh &loaded, const Material &material);
  void useEdgeTarget(int width, int height);
  void drawEdges();
  void drawOverlay(const Overlay &overlay);

  Display &display;
  std::vector<LoadedMesh> meshes; // by Mesh::id - 1; unloaded ones have vao 0
  std::vector<GLuint> textures;   // by Texture::id - 1; unloaded ones are 0
  std::vector<Later> later;
  FrameCounts counting, last;                    // this frame's so far; the last one's
  std::vector<Overlay::Vertex> overlayTriangles; // reused each frame

  GLuint program = 0;
  struct {
    GLint model, normalMatrix, viewProj, worldSnap, screenSnap, viewport, colour, lit, ambient,
        lightCount, lightDirections, lightColours, fogColour, fogDensity, extrude, toonBands,
        outlinePass, outlined, textured, texture;
  } uniform{};
  GLuint lineVao = 0, lineVbo = 0;
  GLuint overlayProgram = 0, overlayVao = 0, overlayVbo = 0;
  GLint overlayScreen = -1;

  Environment environment; // this frame's (toon bands, outlines)
  Camera camera;           // this frame's (the edge pass's distances)
  // Screen-space outlines: whether this frame has them, the offscreen
  // target (colour, normals, depth textures) and its size, the edge pass
  bool screenEdges = false;
  GLuint edgeFramebuffer = 0, edgeTextures[3] = {0, 0, 0};
  int edgeWidth = 0, edgeHeight = 0;
  GLuint edgeProgram = 0, edgeVao = 0;
  glm::vec3 eye{0.0f};
  glm::mat4 viewProj{1.0f}; // this frame's
  std::optional<Frustum> frustum;
  // Desktop.hpp's vertex snapping, for the rest of the frame
  float worldSnap = 0, screenSnap = 0;
};

} // namespace ScratchPadd::Scene
