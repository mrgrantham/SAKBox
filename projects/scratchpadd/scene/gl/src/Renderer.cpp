// Scene::Renderer on OpenGL 3.3 core: one shader for every material (lit or
// unlit, vertex colours times the material's colour, fog), translucent
// meshes kept to the end of the frame and drawn farthest first, and the
// overlay drawn last, in screen points.
#include "Backend.hpp"

#include <ScratchPadd/Scene/OverlayText.hpp>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <utility>

namespace ScratchPadd::Scene {

namespace {

const char *kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColour;
layout(location = 3) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat3 uNormalMatrix;
uniform mat4 uViewProj;
uniform float uWorldSnap;  // world grid vertices snap to (0: off)
uniform float uScreenSnap; // pixel grid vertices snap to (0: off)
uniform vec2 uViewport;
uniform float uExtrude;    // an outline's hull: pushed out this far along the normals (0: not one)

out vec3 vNormal;
out vec4 vColour;
out vec2 vTexCoord;
out float vDistance;

void main() {
  vec4 world = uModel * vec4(aPosition, 1.0);
  if (uExtrude > 0.0)
    world.xyz += normalize(uNormalMatrix * aNormal) * uExtrude;
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
  vTexCoord = aTexCoord;
  vDistance = clip.w;
}
)";

const char *kFragmentShader = R"(#version 330 core
in vec3 vNormal;
in vec4 vColour;
in vec2 vTexCoord;
uniform bool uTextured;      // the material's texture multiplies its colour
uniform sampler2D uTexture;
in float vDistance;

uniform vec4 uColour; // the material's: colour, and alpha 0-1
uniform bool uLit;
uniform int uToonBands;     // lit in this many bands (toon shading), or 0: smoothly
uniform bool uOutlinePass;  // an outline's hull: the colour alone
uniform float uOutlined;    // screen-space outlines: 1 for a mesh that has one (in the normals' alpha)
uniform vec3 uAmbient;
uniform int uLightCount;
uniform vec3 uLightDirections[4];
uniform vec3 uLightColours[4];
uniform vec3 uFogColour;
uniform float uFogDensity;

layout(location = 0) out vec4 fragmentColour;
// Screen-space outlines' second target: the surface's direction (0-1), and
// whether this mesh is outlined (ignored when drawing straight to the window)
layout(location = 1) out vec4 fragmentNormal;

void main() {
  fragmentNormal = vec4(normalize(vNormal) * 0.5 + 0.5, uOutlined);
  vec4 texel = uTextured && !uOutlinePass ? texture(uTexture, vTexCoord) : vec4(1.0);
  // (a texel more than half see-through isn't drawn, as the DS's images
  // have it: their alpha one bit; a card's rounded corners, an icon's ground)
  if (texel.a < 0.5)
    discard;
  vec3 colour = uOutlinePass ? uColour.rgb : vColour.rgb * uColour.rgb * texel.rgb;
  if (uLit) {
    vec3 n = normalize(vNormal);
    vec3 light = uAmbient;
    for (int i = 0; i < uLightCount; ++i) {
      float lit = max(dot(n, -uLightDirections[i]), 0.0);
      if (uToonBands > 1) // (as Types.hpp's toonStep)
        lit = clamp(floor(lit * float(uToonBands)) / float(uToonBands - 1), 0.0, 1.0);
      light += uLightColours[i] * lit;
    }
    colour *= light;
  }
  float fog = clamp(1.0 - exp(-uFogDensity * vDistance), 0.0, 1.0);
  fragmentColour = vec4(mix(colour, uFogColour, fog), vColour.a * uColour.a * texel.a);
}
)";

// Screen-space outlines: the frame (colour, normals, depth) drawn to the
// window, with the outline's colour wherever a pixel a few away (`uPixels`)
// is much nearer or farther, or faces a different way, and either is on an
// outlined mesh; the line fogged as the scene is, at the nearer distance
const char *kEdgeVertexShader = R"(#version 330 core
out vec2 vUv;
void main() {
  // One triangle covering the screen
  vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  vUv = corner;
  gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char *kEdgeFragmentShader = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uColour, uNormals, uDepth;
uniform vec2 uTexel;     // a pixel, in texture coordinates
uniform float uPixels;   // the line's thickness
uniform vec3 uLine, uFogColour;
uniform float uFogDensity, uNear, uFar;
out vec4 fragmentColour;

// How far from the eye, along the view, from the depth buffer's 0-1
float distanceAt(vec2 uv) {
  float z = texture(uDepth, uv).r * 2.0 - 1.0;
  return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear));
}

void main() {
  vec3 colour = texture(uColour, vUv).rgb;
  vec4 here = texture(uNormals, vUv);
  float distance = distanceAt(vUv), nearest = distance;
  bool edge = false;
  vec2 steps[4] = vec2[](vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0));
  for (int i = 0; i < 4; ++i) {
    vec2 uv = vUv + steps[i] * uTexel * uPixels;
    vec4 there = texture(uNormals, uv);
    if (max(here.a, there.a) < 0.5)
      continue; // (neither outlined)
    float thereDistance = distanceAt(uv);
    bool step = abs(thereDistance - distance) > 0.05 * min(thereDistance, distance);
    bool crease = dot(here.xyz * 2.0 - 1.0, there.xyz * 2.0 - 1.0) < 0.85; // (about 30 degrees)
    if (step || crease) {
      edge = true;
      nearest = min(nearest, thereDistance);
    }
  }
  float fog = clamp(1.0 - exp(-uFogDensity * nearest), 0.0, 1.0);
  vec3 line = mix(uLine, uFogColour, fog);
  fragmentColour = vec4(edge ? line : colour, 1.0);
}
)";

const char *kOverlayVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPoint;
layout(location = 1) in vec4 aColour;
layout(location = 2) in vec2 aAtlas;
uniform vec2 uScreen; // points
out vec4 vColour;
out vec2 vAtlas;
void main() {
  gl_Position = vec4(aPoint.x / uScreen.x * 2.0 - 1.0, 1.0 - aPoint.y / uScreen.y * 2.0, 0.0, 1.0);
  vColour = aColour;
  vAtlas = aAtlas;
}
)";

// The colour times the font atlas's alpha (its top level: the letters were
// baked near the size they're drawn)
const char *kOverlayFragmentShader = R"(#version 330 core
in vec4 vColour;
in vec2 vAtlas;
uniform sampler2D uAtlas;
out vec4 fragmentColour;
void main() { fragmentColour = vColour * textureLod(uAtlas, vAtlas, 0.0); }
)";

GLuint compileShader(GLenum type, const char *source, std::string &log) {
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char text[1024];
    glGetShaderInfoLog(shader, sizeof(text), nullptr, text);
    log += std::string(type == GL_VERTEX_SHADER ? "vertex shader: " : "fragment shader: ") + text;
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

GLuint linkProgram(const char *vertexSource, const char *fragmentSource, std::string *error) {
  std::string log;
  const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource, log);
  const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, log);
  const GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok && log.empty()) {
    char text[1024];
    glGetProgramInfoLog(program, sizeof(text), nullptr, text);
    log = std::string("linking: ") + text;
  }
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  if (!log.empty()) {
    if (error)
      *error = log;
    else
      std::fprintf(stderr, "Shader error: %s\n", log.c_str());
  }
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
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void *>(offsetof(Vertex, uv)));
  glEnableVertexAttribArray(3);
}

Renderer::Backend::Backend(Display &display) : display(display) {
  program = linkProgram(kVertexShader, kFragmentShader);
  auto at = [this](const char *name) { return glGetUniformLocation(program, name); };
  uniform = {at("uModel"),       at("uNormalMatrix"), at("uViewProj"),        at("uWorldSnap"),
             at("uScreenSnap"),  at("uViewport"),     at("uColour"),          at("uLit"),
             at("uAmbient"),     at("uLightCount"),   at("uLightDirections"), at("uLightColours"),
             at("uFogColour"),   at("uFogDensity"),   at("uExtrude"),         at("uToonBands"),
             at("uOutlinePass"), at("uOutlined"),     at("uTextured"),        at("uTexture")};
  edgeProgram = linkProgram(kEdgeVertexShader, kEdgeFragmentShader);
  glGenVertexArrays(1, &edgeVao); // (the edge pass's triangle needs no vertices, but a VAO)

  glGenVertexArrays(1, &lineVao);
  glGenBuffers(1, &lineVbo);
  glBindVertexArray(lineVao);
  glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
  describeVertices();

  overlayProgram = linkProgram(kOverlayVertexShader, kOverlayFragmentShader);
  overlayScreen = glGetUniformLocation(overlayProgram, "uScreen");
  overlayAtlas = glGetUniformLocation(overlayProgram, "uAtlas");
  glGenVertexArrays(1, &overlayVao);
  glGenBuffers(1, &overlayVbo);
  glBindVertexArray(overlayVao);
  glBindBuffer(GL_ARRAY_BUFFER, overlayVbo);
  const auto stride = static_cast<GLsizei>(sizeof(TexturedVertex));
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void *>(offsetof(TexturedVertex, x)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                        reinterpret_cast<void *>(offsetof(TexturedVertex, colour)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void *>(offsetof(TexturedVertex, u)));
  glEnableVertexAttribArray(2);
  glBindVertexArray(0);
  makeFontAtlas();
}

// The overlay font's atlas as a texture (white, its alpha), read smoothly
void Renderer::Backend::makeFontAtlas() {
  const FontAtlas atlas = Font::platform().atlas();
  const std::vector<Colour> pixels = atlasPixels(atlas);
  glGenTextures(1, &fontAtlas);
  glBindTexture(GL_TEXTURE_2D, fontAtlas);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlas.width, atlas.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
}

Renderer::Backend::~Backend() {
  for (GLuint texture : textures)
    if (texture)
      glDeleteTextures(1, &texture);
  for (const LoadedMesh &mesh : meshes) {
    if (mesh.vao) {
      glDeleteVertexArrays(1, &mesh.vao);
      glDeleteBuffers(1, &mesh.vbo);
      glDeleteBuffers(1, &mesh.ebo);
      glDeleteVertexArrays(1, &mesh.hullVao);
      glDeleteBuffers(1, &mesh.hullVbo);
    }
  }
  glDeleteVertexArrays(1, &lineVao);
  glDeleteBuffers(1, &lineVbo);
  glDeleteVertexArrays(1, &overlayVao);
  glDeleteBuffers(1, &overlayVbo);
  glDeleteTextures(1, &fontAtlas);
  glDeleteProgram(program);
  glDeleteProgram(overlayProgram);
  glDeleteProgram(edgeProgram);
  glDeleteVertexArrays(1, &edgeVao);
  if (edgeFramebuffer) {
    glDeleteFramebuffers(1, &edgeFramebuffer);
    glDeleteTextures(3, edgeTextures);
  }
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
  glUniform1i(uniform.lit, material.shading == Material::Shading::Unlit ? 0 : 1);
  glUniform1f(uniform.outlined, material.outline ? 1.0f : 0.0f);
  // Its texture, on unit 0 (the edge pass binds its own afterwards)
  const bool textured = material.texture && material.texture.id <= textures.size() &&
                        textures[material.texture.id - 1];
  glUniform1i(uniform.textured, textured ? 1 : 0);
  if (textured) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[material.texture.id - 1]);
    glUniform1i(uniform.texture, 0);
  }
  glUniform1i(uniform.toonBands, material.shading == Material::Shading::Toon
                                     ? toonBandsFor(material, environment)
                                     : 0);
  glUniform1f(uniform.worldSnap, worldSnap);
  glUniform1f(uniform.screenSnap, screenSnap);
  if (material.wireframe)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  // Back faces culled (counter-clockwise is the front), unless either side
  // shows: a double-sided mesh or material, or a wireframe
  if (!loaded.doubleSided && !material.doubleSided && !material.wireframe)
    glEnable(GL_CULL_FACE);
  glBindVertexArray(loaded.vao);
  glDrawElements(GL_TRIANGLES, loaded.indexCount, GL_UNSIGNED_INT, nullptr);
  glDisable(GL_CULL_FACE);
  counting += loaded.cost;
  ++counting.draws;
  if (material.wireframe)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  else if (material.outline && environment.outline.style == Outline::Style::Hull)
    drawOutline(loaded, material);
}

// The inverted hull: the mesh again, pushed out along its shared normals,
// its front faces culled so only the rim around it shows, in one colour
void Renderer::Backend::drawOutline(const LoadedMesh &loaded, const Material &material) {
  const glm::vec3 colour = toGlm(material.outlineColour.value_or(environment.outline.colour));
  glUniform4f(uniform.colour, colour.r, colour.g, colour.b,
              static_cast<float>(std::min<int>(material.alpha, 31)) / 31.0f);
  glUniform1i(uniform.lit, 0);
  glUniform1i(uniform.outlinePass, 1);
  glUniform1f(uniform.extrude, environment.outline.width);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glBindVertexArray(loaded.hullVao);
  glDrawElements(GL_TRIANGLES, loaded.indexCount, GL_UNSIGNED_INT, nullptr);
  glCullFace(GL_BACK);
  glDisable(GL_CULL_FACE);
  glUniform1i(uniform.outlinePass, 0);
  glUniform1f(uniform.extrude, 0);
  counting += loaded.cost;
  ++counting.draws;
}

// The offscreen target for screen-space outlines, made (or remade) at the
// window's size, bound for drawing to both colour targets
void Renderer::Backend::useEdgeTarget(int width, int height) {
  if (!edgeFramebuffer || width != edgeWidth || height != edgeHeight) {
    if (edgeFramebuffer) {
      glDeleteFramebuffers(1, &edgeFramebuffer);
      glDeleteTextures(3, edgeTextures);
    }
    glGenFramebuffers(1, &edgeFramebuffer);
    glGenTextures(3, edgeTextures);
    const GLenum formats[3][3] = {{GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},
                                  {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},
                                  {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT}};
    glBindFramebuffer(GL_FRAMEBUFFER, edgeFramebuffer);
    for (int i = 0; i < 3; ++i) {
      glBindTexture(GL_TEXTURE_2D, edgeTextures[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(formats[i][0]), width, height, 0,
                   formats[i][1], formats[i][2], nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glFramebufferTexture2D(GL_FRAMEBUFFER, i < 2 ? GL_COLOR_ATTACHMENT0 + i : GL_DEPTH_ATTACHMENT,
                             GL_TEXTURE_2D, edgeTextures[i], 0);
    }
    edgeWidth = width;
    edgeHeight = height;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, edgeFramebuffer);
  const GLenum both[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, both);
}

// The edge pass: the offscreen frame to the window, with its outlines
void Renderer::Backend::drawEdges() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_DEPTH_TEST);
  glUseProgram(edgeProgram);
  const char *samplers[3] = {"uColour", "uNormals", "uDepth"};
  for (int i = 0; i < 3; ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, edgeTextures[i]);
    glUniform1i(glGetUniformLocation(edgeProgram, samplers[i]), i);
  }
  glActiveTexture(GL_TEXTURE0);
  const auto at = [this](const char *name) { return glGetUniformLocation(edgeProgram, name); };
  glUniform2f(at("uTexel"), 1.0f / static_cast<float>(edgeWidth),
              1.0f / static_cast<float>(edgeHeight));
  glUniform1f(at("uPixels"), static_cast<float>(std::max<int>(environment.outline.pixels, 1)));
  const glm::vec3 line = toGlm(environment.outline.colour), fog = toGlm(environment.fog.colour);
  glUniform3f(at("uLine"), line.r, line.g, line.b);
  glUniform3f(at("uFogColour"), fog.r, fog.g, fog.b);
  glUniform1f(at("uFogDensity"), environment.fog.density);
  glUniform1f(at("uNear"), camera.nearPlane);
  glUniform1f(at("uFar"), camera.farPlane);
  glBindVertexArray(edgeVao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glUseProgram(program);
  glEnable(GL_DEPTH_TEST);
}

void Renderer::Backend::drawOverlay(const Overlay &overlay) {
  // The shapes, then the text over them
  overlayVertices(overlay, overlayTriangles);
  if (overlayTriangles.empty())
    return;
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(overlayProgram);
  glUniform2f(overlayScreen, static_cast<float>(display.width()),
              static_cast<float>(display.height()));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fontAtlas);
  glUniform1i(overlayAtlas, 0);
  glBindVertexArray(overlayVao);
  glBindBuffer(GL_ARRAY_BUFFER, overlayVbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(overlayTriangles.size() * sizeof(TexturedVertex)),
               overlayTriangles.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(overlayTriangles.size()));
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

Renderer::Renderer(Display &display) : backend_(std::make_unique<Backend>(display)) {}

Renderer::~Renderer() = default;

Mesh Renderer::load(const MeshData &mesh) {
  Backend::LoadedMesh loaded;
  const std::vector<std::uint32_t> indices = triangleList(mesh);
  glGenVertexArrays(1, &loaded.vao);
  glGenBuffers(1, &loaded.vbo);
  glGenBuffers(1, &loaded.ebo);
  glBindVertexArray(loaded.vao);
  glBindBuffer(GL_ARRAY_BUFFER, loaded.vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size_bytes()),
               mesh.vertices.data(), GL_STATIC_DRAW);
  describeVertices();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, loaded.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)), indices.data(),
               GL_STATIC_DRAW);
  // Its outline's hull: the same indices, the normals shared (OutlineHull.hpp)
  const std::vector<Vertex> hull = outlineHullVertices(mesh.vertices);
  glGenVertexArrays(1, &loaded.hullVao);
  glGenBuffers(1, &loaded.hullVbo);
  glBindVertexArray(loaded.hullVao);
  glBindBuffer(GL_ARRAY_BUFFER, loaded.hullVbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(hull.size() * sizeof(Vertex)), hull.data(),
               GL_STATIC_DRAW);
  describeVertices();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, loaded.ebo);
  glBindVertexArray(0);
  loaded.indexCount = static_cast<GLsizei>(indices.size());
  loaded.translucent = hasTranslucentVertices(mesh);
  loaded.cost = costOf(mesh);
  loaded.bounds = boundsOf(mesh);
  loaded.doubleSided = mesh.doubleSided;
  backend_->meshes.push_back(loaded);
  return Mesh{static_cast<std::uint32_t>(backend_->meshes.size())};
}

Texture Renderer::load(const TextureImage &image) {
  // (held to a console's texture memory: past it, no texture, as there)
  if (backend_->textureBudget && image.width > 0 && image.height > 0 &&
      !backend_->textureBudget->takeImage(image.width, image.height))
    return {};
  const bool blank = image.pixels.empty(); // (made see-through, filled by replace)
  if (image.width <= 0 || image.height <= 0 ||
      (!blank && image.pixels.size() < static_cast<std::size_t>(image.width) *
                                           static_cast<std::size_t>(image.height)))
    return {};
  const std::vector<Colour> clear(
      blank ? static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) : 0,
      Colour{0, 0, 0, 0});
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  // Rows from the top, as the image has them: texture coordinate v 0 is
  // its first row
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               blank ? clear.data() : image.pixels.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  backend_->textures.push_back(texture);
  return {static_cast<std::uint32_t>(backend_->textures.size())};
}

bool Renderer::replace(Texture texture, const TextureImage &image, int firstRow) {
  if (!texture || texture.id > backend_->textures.size() || !backend_->textures[texture.id - 1] ||
      firstRow < 0 ||
      image.pixels.size() <
          static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height))
    return false;
  glBindTexture(GL_TEXTURE_2D, backend_->textures[texture.id - 1]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  // (its rows, from firstRow: the whole picture, or a band of it)
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, firstRow, image.width, image.height, GL_RGBA,
                  GL_UNSIGNED_BYTE, image.pixels.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  return true;
}

void Renderer::unload(Texture texture) {
  if (!texture || texture.id > backend_->textures.size())
    return;
  GLuint &loaded = backend_->textures[texture.id - 1];
  if (loaded)
    glDeleteTextures(1, &loaded);
  loaded = 0;
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
  glDeleteVertexArrays(1, &loaded.hullVao);
  glDeleteBuffers(1, &loaded.hullVbo);
  loaded = {};
}

void Renderer::beginFrame(const Camera &camera, const Environment &environment) {
  Backend &b = *backend_;
  const int width = b.display.pixelWidth(), height = b.display.pixelHeight();
  glViewport(0, 0, width, height);
  const glm::vec3 background = toGlm(environment.backgroundSeen(camera.farPlane));
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glDepthMask(GL_TRUE);
  // Screen-space outlines: drawn into the offscreen target (colour,
  // normals, depth), the edges found in endFrame
  b.screenEdges = environment.outline.style == Outline::Style::ScreenSpace;
  if (b.screenEdges) {
    b.useEdgeTarget(width, height);
    const GLfloat colour[] = {background.r, background.g, background.b, 1.0f};
    const GLfloat noSurface[] = {0.5f, 0.5f, 0.5f, 0.0f}; // (no direction, not outlined)
    glClearBufferfv(GL_COLOR, 0, colour);
    glClearBufferfv(GL_COLOR, 1, noSurface);
    glClear(GL_DEPTH_BUFFER_BIT);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(background.r, background.g, background.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  glUseProgram(b.program);
  b.useCamera(camera, {0, 0, width, height});
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
  b.environment = environment;
  b.worldSnap = b.screenSnap = 0;
  b.later.clear();
  b.counting = {};
  overlay_.clear();
}

void Renderer::Backend::drawLater() {
  // Translucent meshes over the solid ones, farthest first, each blending
  // over what's behind it without hiding what's drawn after
  if (later.empty())
    return;
  std::ranges::stable_sort(later,
                           [](const Later &x, const Later &y) { return x.distance > y.distance; });
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  if (screenEdges)
    glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE,
                 GL_FALSE); // (see-through: no surface of its own)
  for (const Later &item : later)
    drawNow(item.mesh, item.where, item.material);
  glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  later.clear();
}

void Renderer::Backend::useCamera(const Camera &seen, PixelRect rect) {
  camera = seen;
  view = rect;
  const float aspect =
      static_cast<float>(rect.width) / static_cast<float>(std::max(rect.height, 1));
  eye = toGlm(seen.eye);
  const glm::mat4 look = glm::lookAt(eye, toGlm(seen.target), toGlm(seen.up));
  const glm::mat4 projection =
      glm::perspective(glm::radians(seen.fieldOfView), aspect, seen.nearPlane, seen.farPlane);
  viewProj = projection * look;
  frustum.emplace(seen, aspect);
  // (GL counts up from the bottom)
  const int fromBottom = display.pixelHeight() - rect.y - rect.height;
  glViewport(rect.x, fromBottom, rect.width, rect.height);
  glEnable(GL_SCISSOR_TEST);
  glScissor(rect.x, fromBottom, rect.width, rect.height);
  glUseProgram(program);
  glUniformMatrix4fv(uniform.viewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
  glUniform2f(uniform.viewport, static_cast<float>(rect.width), static_cast<float>(rect.height));
}

void Renderer::beginView(const Camera &camera, const Viewport &viewport) {
  Backend &b = *backend_;
  b.drawLater();
  b.useCamera(camera, pixelsOf(viewport, b.display.pixelWidth(), b.display.pixelHeight()));
}

void Renderer::draw(Mesh mesh, const Transform &where, const Material &material) {
  const auto &meshes = backend_->meshes;
  // Wholly out of view: not drawn (Culling.hpp)
  if (mesh && mesh.id <= meshes.size() && backend_->frustum &&
      backend_->frustum->outside(meshes[mesh.id - 1].bounds, where)) {
    ++backend_->counting.culled;
    return;
  }
  if (material.alpha < 31 ||
      (mesh && mesh.id <= meshes.size() && meshes[mesh.id - 1].translucent)) {
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
  glUniform1i(b.uniform.toonBands, 0);
  glUniform1f(b.uniform.worldSnap, b.worldSnap);
  glUniform1f(b.uniform.screenSnap, b.screenSnap);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 2 * 2));
  b.counting.polygons += static_cast<unsigned>(vertices.size() / 2);
  b.counting.vertices += static_cast<unsigned>(vertices.size() / 2 * 3);
  ++b.counting.draws;
}

void Renderer::endFrame() {
  Backend &b = *backend_;
  b.drawLater();
  // (the outlines and the overlay: the whole screen again)
  glDisable(GL_SCISSOR_TEST);
  glViewport(0, 0, b.display.pixelWidth(), b.display.pixelHeight());
  if (b.screenEdges)
    b.drawEdges();
  b.drawOverlay(overlay_);
  overlay_.clear();
  glBindVertexArray(0);
  b.last = std::exchange(b.counting, {});
  b.display.present();
}

bool Renderer::toScreen(const Vector3 &point, float &x, float &y) const {
  const Backend &b = *backend_;
  const glm::vec4 clip = b.viewProj * glm::vec4(toGlm(point), 1.0f);
  if (clip.w <= 0)
    return false;
  // (in the view, then from pixels to the overlay's points)
  const float points = static_cast<float>(b.display.width()) /
                       static_cast<float>(std::max(b.display.pixelWidth(), 1));
  x = (static_cast<float>(b.view.x) +
       (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(b.view.width)) *
      points;
  y = (static_cast<float>(b.view.y) +
       (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(b.view.height)) *
      points;
  return true;
}

Capabilities Renderer::capabilities() const { return {}; }

std::optional<TextureMemory> Renderer::textureMemory() const { return backend_->textureBudget; }

void Renderer::setTextureBudget(const TextureMemory &budget) { backend_->textureBudget = budget; }

FrameCounts Renderer::lastFrame() const { return backend_->last; }

FrameTimes Renderer::lastFrameTimes() const { return {}; } // (not measured here)

} // namespace ScratchPadd::Scene
