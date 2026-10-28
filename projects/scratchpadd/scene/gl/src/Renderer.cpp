// Scene::Renderer on OpenGL 3.3 core: one shader for every material (lit or
// unlit, vertex colours times the material's colour, fog), translucent
// meshes kept to the end of the frame and drawn farthest first, and the
// overlay drawn last, in screen points.
#include "Backend.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdio>

namespace ScratchPadd::Scene {

namespace {

const char *kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColour;

uniform mat4 uModel;
uniform mat3 uNormalMatrix;
uniform mat4 uViewProj;
uniform float uWorldSnap;  // world grid vertices snap to (0: off)
uniform float uScreenSnap; // pixel grid vertices snap to (0: off)
uniform vec2 uViewport;

out vec3 vNormal;
out vec4 vColour;
out float vDistance;

void main() {
  vec4 world = uModel * vec4(aPosition, 1.0);
  if (uWorldSnap > 0.0)
    world.xyz = floor(world.xyz / uWorldSnap + 0.5) * uWorldSnap;
  vec4 clip = uViewProj * world;
  if (uScreenSnap > 0.0 && clip.w > 0.0) {
    vec2 pixel = (clip.xy / clip.w * 0.5 + 0.5) * uViewport;
    pixel = floor(pixel / uScreenSnap + 0.5) * uScreenSnap;
    clip.xy = (pixel / uViewport * 2.0 - 1.0) * clip.w;
  }
  gl_Position = clip;
  vNormal = uNormalMatrix * aNormal;
  vColour = aColour;
  vDistance = clip.w;
}
)";

const char *kFragmentShader = R"(#version 330 core
in vec3 vNormal;
in vec4 vColour;
in float vDistance;

uniform vec4 uColour; // the material's: colour, and alpha 0-1
uniform bool uLit;
uniform vec3 uAmbient;
uniform int uLightCount;
uniform vec3 uLightDirections[4];
uniform vec3 uLightColours[4];
uniform vec3 uFogColour;
uniform float uFogDensity;

out vec4 fragmentColour;

void main() {
  vec3 colour = vColour.rgb * uColour.rgb;
  if (uLit) {
    vec3 n = normalize(vNormal);
    vec3 light = uAmbient;
    for (int i = 0; i < uLightCount; ++i)
      light += uLightColours[i] * max(dot(n, -uLightDirections[i]), 0.0);
    colour *= light;
  }
  float fog = clamp(1.0 - exp(-uFogDensity * vDistance), 0.0, 1.0);
  fragmentColour = vec4(mix(colour, uFogColour, fog), vColour.a * uColour.a);
}
)";

const char *kOverlayVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPoint;
layout(location = 1) in vec4 aColour;
uniform vec2 uScreen; // points
out vec4 vColour;
void main() {
  gl_Position = vec4(aPoint.x / uScreen.x * 2.0 - 1.0, 1.0 - aPoint.y / uScreen.y * 2.0, 0.0, 1.0);
  vColour = aColour;
}
)";

const char *kOverlayFragmentShader = R"(#version 330 core
in vec4 vColour;
out vec4 fragmentColour;
void main() { fragmentColour = vColour; }
)";

GLuint compileShader(GLenum type, const char *source) {
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::fprintf(stderr, "Shader compile error: %s\n", log);
  }
  return shader;
}

glm::vec3 toGlm(const Vector3 &v) { return {v.x, v.y, v.z}; }
glm::vec3 toGlm(const Colour &c) { return glm::vec3(c.r, c.g, c.b) / 255.0f; }

glm::mat4 toGlm(const Transform &t) {
  glm::mat4 m(1.0f);
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      m[column][row] = t.m[row][column]; // glm is column-major
  return m;
}

} // namespace

GLuint linkProgram(const char *vertexSource, const char *fragmentSource) {
  const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
  const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
  const GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::fprintf(stderr, "Shader link error: %s\n", log);
  }
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  return program;
}

void describeVertices() {
  const auto stride = static_cast<GLsizei>(sizeof(Vertex));
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                        reinterpret_cast<void *>(offsetof(Vertex, colour)));
  glEnableVertexAttribArray(2);
}

Renderer::Backend::Backend(Display &display) : display(display) {
  program = linkProgram(kVertexShader, kFragmentShader);
  auto at = [this](const char *name) { return glGetUniformLocation(program, name); };
  uniform = {at("uModel"),      at("uNormalMatrix"), at("uViewProj"),        at("uWorldSnap"),
             at("uScreenSnap"), at("uViewport"),     at("uColour"),          at("uLit"),
             at("uAmbient"),    at("uLightCount"),   at("uLightDirections"), at("uLightColours"),
             at("uFogColour"),  at("uFogDensity")};

  glGenVertexArrays(1, &lineVao);
  glGenBuffers(1, &lineVbo);
  glBindVertexArray(lineVao);
  glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
  describeVertices();

  overlayProgram = linkProgram(kOverlayVertexShader, kOverlayFragmentShader);
  overlayScreen = glGetUniformLocation(overlayProgram, "uScreen");
  glGenVertexArrays(1, &overlayVao);
  glGenBuffers(1, &overlayVbo);
  glBindVertexArray(overlayVao);
  glBindBuffer(GL_ARRAY_BUFFER, overlayVbo);
  const auto stride = static_cast<GLsizei>(sizeof(Overlay::Vertex));
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void *>(offsetof(Overlay::Vertex, x)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                        reinterpret_cast<void *>(offsetof(Overlay::Vertex, colour)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
}

Renderer::Backend::~Backend() {
  for (const LoadedMesh &mesh : meshes) {
    if (mesh.vao) {
      glDeleteVertexArrays(1, &mesh.vao);
      glDeleteBuffers(1, &mesh.vbo);
      glDeleteBuffers(1, &mesh.ebo);
    }
  }
  glDeleteVertexArrays(1, &lineVao);
  glDeleteBuffers(1, &lineVbo);
  glDeleteVertexArrays(1, &overlayVao);
  glDeleteBuffers(1, &overlayVbo);
  glDeleteProgram(program);
  glDeleteProgram(overlayProgram);
}

void Renderer::Backend::drawNow(Mesh mesh, const Transform &where, const Material &material) {
  if (!mesh || mesh.id > meshes.size() || !meshes[mesh.id - 1].vao)
    return;
  const LoadedMesh &loaded = meshes[mesh.id - 1];
  const glm::mat4 model = toGlm(where);
  glUniformMatrix4fv(uniform.model, 1, GL_FALSE, glm::value_ptr(model));
  const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));
  glUniformMatrix3fv(uniform.normalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
  const glm::vec3 colour = toGlm(material.colour);
  glUniform4f(uniform.colour, colour.r, colour.g, colour.b,
              static_cast<float>(std::min<int>(material.alpha, 31)) / 31.0f);
  glUniform1i(uniform.lit, material.shading == Material::Shading::lit ? 1 : 0);
  glUniform1f(uniform.worldSnap, worldSnap);
  glUniform1f(uniform.screenSnap, screenSnap);
  if (material.wireframe)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glBindVertexArray(loaded.vao);
  glDrawElements(GL_TRIANGLES, loaded.indexCount, GL_UNSIGNED_INT, nullptr);
  if (material.wireframe)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Renderer::Backend::drawOverlay(const Overlay &overlay) {
  if (overlay.triangles().empty())
    return;
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(overlayProgram);
  glUniform2f(overlayScreen, static_cast<float>(display.width()),
              static_cast<float>(display.height()));
  glBindVertexArray(overlayVao);
  glBindBuffer(GL_ARRAY_BUFFER, overlayVbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(overlay.triangles().size() * sizeof(Overlay::Vertex)),
               overlay.triangles().data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(overlay.triangles().size()));
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

Renderer::Renderer(Display &display) : backend_(std::make_unique<Backend>(display)) {}

Renderer::~Renderer() = default;

Mesh Renderer::load(const MeshData &mesh) {
  Backend::LoadedMesh loaded;
  glGenVertexArrays(1, &loaded.vao);
  glGenBuffers(1, &loaded.vbo);
  glGenBuffers(1, &loaded.ebo);
  glBindVertexArray(loaded.vao);
  glBindBuffer(GL_ARRAY_BUFFER, loaded.vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size_bytes()),
               mesh.vertices.data(), GL_STATIC_DRAW);
  describeVertices();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, loaded.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size_bytes()),
               mesh.indices.data(), GL_STATIC_DRAW);
  glBindVertexArray(0);
  loaded.indexCount = static_cast<GLsizei>(mesh.indices.size());
  backend_->meshes.push_back(loaded);
  return Mesh{static_cast<std::uint32_t>(backend_->meshes.size())};
}

void Renderer::unload(Mesh mesh) {
  if (!mesh || mesh.id > backend_->meshes.size())
    return;
  Backend::LoadedMesh &loaded = backend_->meshes[mesh.id - 1];
  if (!loaded.vao)
    return;
  glDeleteVertexArrays(1, &loaded.vao);
  glDeleteBuffers(1, &loaded.vbo);
  glDeleteBuffers(1, &loaded.ebo);
  loaded = {};
}

void Renderer::beginFrame(const Camera &camera, const Environment &environment) {
  Backend &b = *backend_;
  const int width = b.display.pixelWidth(), height = b.display.pixelHeight();
  glViewport(0, 0, width, height);
  const glm::vec3 background = toGlm(environment.background);
  glClearColor(background.r, background.g, background.b, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(b.program);
  b.eye = toGlm(camera.eye);
  const glm::mat4 view = glm::lookAt(b.eye, toGlm(camera.target), toGlm(camera.up));
  const glm::mat4 projection =
      glm::perspective(glm::radians(camera.fieldOfView),
                       static_cast<float>(width) / static_cast<float>(std::max(height, 1)),
                       camera.nearPlane, camera.farPlane);
  b.viewProj = projection * view;
  glUniformMatrix4fv(b.uniform.viewProj, 1, GL_FALSE, glm::value_ptr(b.viewProj));
  glUniform2f(b.uniform.viewport, static_cast<float>(width), static_cast<float>(height));
  const glm::vec3 ambient = toGlm(environment.ambient);
  glUniform3f(b.uniform.ambient, ambient.r, ambient.g, ambient.b);
  glm::vec3 directions[Environment::kMostLights], colours[Environment::kMostLights];
  for (unsigned i = 0; i < environment.lightCount; ++i) {
    directions[i] = glm::normalize(toGlm(environment.lights[i].direction));
    colours[i] = toGlm(environment.lights[i].colour);
  }
  glUniform1i(b.uniform.lightCount, static_cast<GLint>(environment.lightCount));
  glUniform3fv(b.uniform.lightDirections, Environment::kMostLights, glm::value_ptr(directions[0]));
  glUniform3fv(b.uniform.lightColours, Environment::kMostLights, glm::value_ptr(colours[0]));
  const glm::vec3 fog = toGlm(environment.fog.colour);
  glUniform3f(b.uniform.fogColour, fog.r, fog.g, fog.b);
  glUniform1f(b.uniform.fogDensity, environment.fog.density);
  b.worldSnap = b.screenSnap = 0;
  b.later.clear();
  overlay_.clear();
}

void Renderer::draw(Mesh mesh, const Transform &where, const Material &material) {
  if (material.alpha < 31) {
    const glm::vec3 position = toGlm(where.position());
    backend_->later.push_back({mesh, where, material, glm::length(position - backend_->eye)});
    return;
  }
  backend_->drawNow(mesh, where, material);
}

void Renderer::drawLines(std::span<const Vector3> ends, Colour colour) {
  if (ends.size() < 2)
    return;
  std::vector<Vertex> vertices;
  vertices.reserve(ends.size());
  for (const Vector3 &end : ends)
    vertices.push_back({end, {0, 1, 0}, Colour{}});
  Backend &b = *backend_;
  glBindVertexArray(b.lineVao);
  glBindBuffer(GL_ARRAY_BUFFER, b.lineVbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(), GL_STREAM_DRAW);
  const glm::mat4 identity(1.0f);
  const glm::mat3 normalIdentity(1.0f);
  glUniformMatrix4fv(b.uniform.model, 1, GL_FALSE, glm::value_ptr(identity));
  glUniformMatrix3fv(b.uniform.normalMatrix, 1, GL_FALSE, glm::value_ptr(normalIdentity));
  const glm::vec3 c = toGlm(colour);
  glUniform4f(b.uniform.colour, c.r, c.g, c.b, colour.a / 255.0f);
  glUniform1i(b.uniform.lit, 0);
  glUniform1f(b.uniform.worldSnap, b.worldSnap);
  glUniform1f(b.uniform.screenSnap, b.screenSnap);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 2 * 2));
}

void Renderer::endFrame() {
  Backend &b = *backend_;
  // Translucent meshes over the solid ones, farthest first, each blending
  // over what's behind it without hiding what's drawn after
  std::ranges::stable_sort(b.later, [](const Backend::Later &x, const Backend::Later &y) {
    return x.distance > y.distance;
  });
  if (!b.later.empty()) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (const Backend::Later &item : b.later)
      b.drawNow(item.mesh, item.where, item.material);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    b.later.clear();
  }
  b.drawOverlay(overlay_);
  overlay_.clear();
  glBindVertexArray(0);
  b.display.present();
}

bool Renderer::toScreen(const Vector3 &point, float &x, float &y) const {
  const glm::vec4 clip = backend_->viewProj * glm::vec4(toGlm(point), 1.0f);
  if (clip.w <= 0)
    return false;
  x = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(backend_->display.width());
  y = (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(backend_->display.height());
  return true;
}

Capabilities Renderer::capabilities() const { return {}; }

} // namespace ScratchPadd::Scene
