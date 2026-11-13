// The scene layer's 3DS backend, with citro3d: see ../../DESIGN.md. Draws
// are kept during the frame and drawn at its end, once for each eye (the
// right too while the 3D slider is up): the solid meshes, lines and
// wireframes, the translucent meshes farthest first, then the overlay at
// the screen's depth. Lighting is per vertex (scene.v.pica), fog per pixel
// (the GPU's fog table). The GPU draws only triangles, so lines (and a
// wireframe's edges) are quads, two triangles each, that line.v.pica
// widens to a pixel on the screen for each eye. (A geometry shader making
// them hung a 3DS's GPU, though not Azahar's.)
#include "Backend.hpp"

#include <ScratchPadd/Scene/OverlayText.hpp>
#include <ScratchPadd/Scene/Primitives.hpp>

#include <3ds.h>

#include "line_shbin.h"
#include "overlay_shbin.h"
#include "scene_shbin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>

namespace ScratchPadd::Scene {

namespace {

static_assert(sizeof(Vertex) == 28, "the GPU reads Scene::Vertex as 3 + 3 floats and 4 bytes");
static_assert(sizeof(Renderer::Backend::LineVertex) == 32,
              "line.v.pica reads a LineVertex as 3 + 3 floats, 4 bytes and a float");
static_assert(sizeof(Overlay::Vertex) == 12, "the GPU reads Overlay::Vertex as 2 floats, 4 bytes");

// A frame's lines and overlay at most, in linear memory
constexpr std::size_t kArenaBytes = 1 << 20;

// The vertex shader's inputs from a Scene::Vertex: position, normal, colour
void describeSceneVertices(C3D_AttrInfo &attributes) {
  AttrInfo_Init(&attributes);
  AttrInfo_AddLoader(&attributes, 0, GPU_FLOAT, 3);
  AttrInfo_AddLoader(&attributes, 1, GPU_FLOAT, 3);
  AttrInfo_AddLoader(&attributes, 2, GPU_UNSIGNED_BYTE, 4);
}

// A transform as citro3d's 4 x 4 matrix
C3D_Mtx toMatrix(const Transform &t) {
  C3D_Mtx m;
  Mtx_Identity(&m);
  for (int row = 0; row < 3; ++row) {
    m.r[row].x = t.m[row][0];
    m.r[row].y = t.m[row][1];
    m.r[row].z = t.m[row][2];
    m.r[row].w = t.m[row][3];
  }
  return m;
}

// How a transform turns normals: the inverse transpose of its rotation and
// scale (a scaled box's normals stay at right angles to its faces)
C3D_Mtx normalMatrix(const Transform &t) {
  const auto &a = t.m;
  const float c00 = a[1][1] * a[2][2] - a[1][2] * a[2][1];
  const float c01 = a[1][2] * a[2][0] - a[1][0] * a[2][2];
  const float c02 = a[1][0] * a[2][1] - a[1][1] * a[2][0];
  const float determinant = a[0][0] * c00 + a[0][1] * c01 + a[0][2] * c02;
  const float d = determinant != 0 ? 1 / determinant : 0;
  C3D_Mtx n;
  Mtx_Identity(&n);
  // (the cofactors over the determinant: the inverse's transpose)
  n.r[0].x = c00 * d;
  n.r[0].y = c01 * d;
  n.r[0].z = c02 * d;
  n.r[1].x = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) * d;
  n.r[1].y = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) * d;
  n.r[1].z = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) * d;
  n.r[2].x = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) * d;
  n.r[2].y = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) * d;
  n.r[2].z = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) * d;
  return n;
}

float channel(std::uint8_t c) { return c / 255.0f; }

// A colour as citro3d clears with it (0xRRGGBBAA) and fogs with it (0xBBGGRR)
u32 clearColour(Colour c) {
  return static_cast<u32>(c.r) << 24 | static_cast<u32>(c.g) << 16 | static_cast<u32>(c.b) << 8 |
         0xFF;
}
u32 fogColour(Colour c) {
  return static_cast<u32>(c.b) << 16 | static_cast<u32>(c.g) << 8 | static_cast<u32>(c.r);
}

// The eye's projection, turned to the top screen: in stereo, each eye
// `separation` apart, objects as far as the target at the screen's depth
C3D_Mtx projectionFor(const Camera &camera, float separation) {
  C3D_Mtx p;
  const float fov = C3D_AngleFromDegrees(camera.fieldOfView);
  if (separation == 0) {
    Mtx_PerspTilt(&p, fov, C3D_AspectRatioTop, camera.nearPlane, camera.farPlane, false);
  } else {
    const float focus = std::max<float>((camera.target - camera.eye).length(), camera.nearPlane);
    Mtx_PerspStereoTilt(&p, fov, C3D_AspectRatioTop, camera.nearPlane, camera.farPlane, separation,
                        focus, false);
  }
  return p;
}

} // namespace

void *Renderer::Backend::Arena::take(std::size_t bytes) {
  const std::size_t start = (used + 15) & ~std::size_t{15};
  if (start + bytes > size)
    return nullptr;
  used = start + bytes;
  return memory + start;
}

Renderer::Backend::Backend(Display &display) : display(display) {
  sceneShaders = DVLB_ParseFile(const_cast<u32 *>(reinterpret_cast<const u32 *>(scene_shbin)),
                                scene_shbin_size);
  overlayShaders = DVLB_ParseFile(const_cast<u32 *>(reinterpret_cast<const u32 *>(overlay_shbin)),
                                  overlay_shbin_size);
  shaderProgramInit(&sceneProgram);
  shaderProgramSetVsh(&sceneProgram, &sceneShaders->DVLE[0]);
  lineShaders =
      DVLB_ParseFile(const_cast<u32 *>(reinterpret_cast<const u32 *>(line_shbin)), line_shbin_size);
  shaderProgramInit(&lineProgram);
  shaderProgramSetVsh(&lineProgram, &lineShaders->DVLE[0]);
  lineMvp = static_cast<int>(shaderInstanceGetUniformLocation(lineProgram.vertexShader, "mvp"));
  lineMaterial =
      static_cast<int>(shaderInstanceGetUniformLocation(lineProgram.vertexShader, "material"));
  shaderProgramInit(&overlayProgram);
  shaderProgramSetVsh(&overlayProgram, &overlayShaders->DVLE[0]);

  const auto locate = [](const shaderProgram_s &program) {
    const auto at = [&](const char *name) {
      return static_cast<int>(shaderInstanceGetUniformLocation(program.vertexShader, name));
    };
    return SceneUniforms{at("mvp"),      at("normalMatrix"),    at("material"),
                         at("ambient"),  at("lightDirections"), at("lightColours"),
                         at("debugMix"), at("debugAdd")};
  };
  sceneUniforms = locate(sceneProgram);
  overlayProjection =
      static_cast<int>(shaderInstanceGetUniformLocation(overlayProgram.vertexShader, "projection"));

  describeSceneVertices(sceneAttributes);
  AttrInfo_Init(&lineAttributes);
  AttrInfo_AddLoader(&lineAttributes, 0, GPU_FLOAT, 3);         // this end
  AttrInfo_AddLoader(&lineAttributes, 1, GPU_FLOAT, 3);         // the other
  AttrInfo_AddLoader(&lineAttributes, 2, GPU_UNSIGNED_BYTE, 4); // colour
  AttrInfo_AddLoader(&lineAttributes, 3, GPU_FLOAT, 1);         // side
  AttrInfo_Init(&overlayAttributes);
  AttrInfo_AddLoader(&overlayAttributes, 0, GPU_FLOAT, 2);
  AttrInfo_AddLoader(&overlayAttributes, 1, GPU_UNSIGNED_BYTE, 4);

  arena.memory = static_cast<std::uint8_t *>(linearAlloc(kArenaBytes));
  arena.size = arena.memory ? kArenaBytes : 0;
}

Renderer::Backend::~Backend() {
  for (LoadedMesh &mesh : meshes) {
    linearFree(mesh.vertices);
    linearFree(mesh.indices);
    linearFree(mesh.wireframe);
  }
  linearFree(arena.memory);
  shaderProgramFree(&sceneProgram);
  shaderProgramFree(&lineProgram);
  shaderProgramFree(&overlayProgram);
  DVLB_Free(sceneShaders);
  DVLB_Free(lineShaders);
  DVLB_Free(overlayShaders);
}

// The scene's program, its vertices, the combiner passing the vertex
// colour through, and the frame's lights (unlit draws set their own: see
// setMaterial)
void Renderer::Backend::useScene() {
  if (bound != &sceneProgram) {
    C3D_BindProgram(&sceneProgram);
    bound = &sceneProgram;
  }
  C3D_SetAttrInfo(&sceneAttributes);
  C3D_TexEnv *combiner = C3D_GetTexEnv(0);
  C3D_TexEnvInit(combiner);
  C3D_TexEnvSrc(combiner, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(combiner, C3D_Both, GPU_REPLACE);
  for (unsigned i = 0; i < Environment::kMostLights; ++i) {
    const bool on = i < environment.lightCount;
    const Vector3 towards = on ? (-environment.lights[i].direction).normalised() : Vector3{};
    C3D_FVUnifSet(GPU_VERTEX_SHADER, sceneUniforms.lightDirections + static_cast<int>(i), towards.x,
                  towards.y, towards.z, 0);
    // The debug view: what the output is made of (usual, normal, light,
    // vertex colour), and overdraw's step added
    float mix[4] = {1, 0, 0, 0};
    float add = 0;
    switch (debug.settings.view) {
    case Debug::View::Normal:
      break;
    case Debug::View::Normals:
      mix[0] = 0, mix[1] = 1;
      break;
    case Debug::View::Lighting:
      mix[0] = 0, mix[2] = 1;
      break;
    case Debug::View::VertexColours:
      mix[0] = 0, mix[3] = 1;
      break;
    case Debug::View::Overdraw:
      mix[0] = 0, add = 1.0f / Debug::kOverdrawSteps;
      break;
    }
    C3D_FVUnifSet(GPU_VERTEX_SHADER, sceneUniforms.debugMix, mix[0], mix[1], mix[2], mix[3]);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, sceneUniforms.debugAdd, add, add, add, add > 0 ? 1 : 0);
  }
}

// The draw's transform and material, lit or unlit
void Renderer::Backend::setMaterial(const Material &material, const Transform &where,
                                    const C3D_Mtx &mvp) {
  const SceneUniforms &u = sceneUniforms;
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u.mvp, &mvp);
  const C3D_Mtx normals = normalMatrix(where);
  C3D_FVUnifMtx3x4(GPU_VERTEX_SHADER, u.normalMatrix, &normals);
  const float alpha = static_cast<float>(std::min<int>(material.alpha, 31)) / 31.0f;
  C3D_FVUnifSet(GPU_VERTEX_SHADER, u.material, channel(material.colour.r),
                channel(material.colour.g), channel(material.colour.b), alpha);
  const bool lit = material.shading == Material::Shading::Lit;
  if (lit) {
    C3D_FVUnifSet(GPU_VERTEX_SHADER, u.ambient, channel(environment.ambient.r),
                  channel(environment.ambient.g), channel(environment.ambient.b), 1);
  } else {
    C3D_FVUnifSet(GPU_VERTEX_SHADER, u.ambient, 1, 1, 1, 1);
  }
  for (unsigned i = 0; i < Environment::kMostLights; ++i) {
    const Colour c = i < environment.lightCount ? environment.lights[i].colour : Colour{0, 0, 0};
    C3D_FVUnifSet(GPU_VERTEX_SHADER, u.lightColours + static_cast<int>(i), lit ? channel(c.r) : 0,
                  lit ? channel(c.g) : 0, lit ? channel(c.b) : 0, 0);
  }
}

void Renderer::Backend::drawMesh(const Draw &draw, const C3D_Mtx &viewProjection) {
  LoadedMesh &mesh = meshes[draw.mesh.id - 1];
  C3D_Mtx model = toMatrix(draw.where), mvp;
  Mtx_Multiply(&mvp, &viewProjection, &model);
  if (!mayDraw())
    return;
  if (draw.material.wireframe) {
    if (debug.settings.wireframes) {
      drawWireframe(mesh, mvp, draw.material);
      sent("wireframe of mesh", draw.mesh.id);
    }
    return;
  }
  useScene();
  setMaterial(draw.material, draw.where, mvp);
  // Back faces culled (counter-clockwise is the front), unless either side
  // shows: a double-sided mesh or material
  const bool cull = !mesh.doubleSided && !draw.material.doubleSided && debug.settings.culling;
  C3D_CullFace(cull ? GPU_CULL_BACK_CCW : GPU_CULL_NONE);
  C3D_SetBufInfo(&mesh.buffers);
  if (mesh.indices)
    C3D_DrawElements(GPU_TRIANGLES, mesh.count, C3D_UNSIGNED_SHORT, mesh.indices);
  else
    C3D_DrawArrays(GPU_TRIANGLES, 0, mesh.count);
  sent("mesh", draw.mesh.id);
}

void Renderer::Backend::sent(const char *what, std::size_t index) {
  const unsigned n = debug.stepCount.load(std::memory_order_relaxed);
  debug.steps[n % kSteps] = {what, eye, static_cast<std::uint16_t>(index)};
  debug.stepCount.store(n + 1, std::memory_order_release);
  ++debug.drawsThisFrame;
}

// Lines in turn (six vertices each), placed by `mvp`, unlit in their
// colours times the material's, widened to a pixel by line.v.pica
void Renderer::Backend::drawLineVertices(const LineVertex *vertices, int count, const C3D_Mtx &mvp,
                                         const Material &material) {
  if (!vertices || count == 0)
    return;
  if (bound != &lineProgram) {
    C3D_BindProgram(&lineProgram);
    bound = &lineProgram;
  }
  C3D_SetAttrInfo(&lineAttributes);
  C3D_CullFace(GPU_CULL_NONE); // (a line's quad faces either way)
  C3D_TexEnv *combiner = C3D_GetTexEnv(0);
  C3D_TexEnvInit(combiner);
  C3D_TexEnvSrc(combiner, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
  C3D_TexEnvFunc(combiner, C3D_Both, GPU_REPLACE);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, lineMvp, &mvp);
  const float alpha = static_cast<float>(std::min<int>(material.alpha, 31)) / 31.0f;
  C3D_FVUnifSet(GPU_VERTEX_SHADER, lineMaterial, channel(material.colour.r),
                channel(material.colour.g), channel(material.colour.b), alpha);
  C3D_BufInfo buffers;
  BufInfo_Init(&buffers);
  BufInfo_Add(&buffers, vertices, sizeof(LineVertex), 4, 0x3210);
  C3D_SetBufInfo(&buffers);
  C3D_DrawArrays(GPU_TRIANGLES, 0, count);
}

// Writes the line from `a` to `b` as six LineVertex at `out` (two
// triangles: one side's corners, a and b, then the other's)
void writeLine(Renderer::Backend::LineVertex *out, const Vector3 &a, const Vector3 &b,
               Colour colourA, Colour colourB) {
  using LineVertex = Renderer::Backend::LineVertex;
  const LineVertex aPlus{a, b, colourA, 1}, aMinus{a, b, colourA, -1};
  // (seen from b, the line points the other way: its sides swap)
  const LineVertex bPlus{b, a, colourB, -1}, bMinus{b, a, colourB, 1};
  out[0] = aPlus;
  out[1] = bPlus;
  out[2] = bMinus;
  out[3] = aPlus;
  out[4] = bMinus;
  out[5] = aMinus;
}

// A mesh's triangles' edges as lines, each once, in its vertex colours
// times the material's (unlit); made the first time
void Renderer::Backend::drawWireframe(LoadedMesh &mesh, const C3D_Mtx &mvp,
                                      const Material &material) {
  if (!mesh.wireframe) {
    // Each triangle's corners (its indices, or the vertices in turn), and
    // each place an id, so edges in the same place are one
    const auto corner = [&](std::size_t i) {
      return mesh.triangles.empty() ? static_cast<std::uint32_t>(i) : mesh.triangles[i];
    };
    const std::size_t corners =
        mesh.triangles.empty() ? static_cast<std::size_t>(mesh.count) : mesh.triangles.size();
    std::map<std::array<float, 3>, std::uint32_t> places;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::pair<std::uint32_t, std::uint32_t>>
        edges; // (by places: the vertices)
    const auto placeOf = [&](std::uint32_t v) {
      const Vector3 &p = mesh.vertices[v].position;
      return places.try_emplace({p.x, p.y, p.z}, static_cast<std::uint32_t>(places.size()))
          .first->second;
    };
    for (std::size_t t = 0; t + 2 < corners; t += 3) {
      for (std::size_t side = 0; side < 3; ++side) {
        const std::uint32_t a = corner(t + side), b = corner(t + (side + 1) % 3);
        const std::uint32_t pa = placeOf(a), pb = placeOf(b);
        edges.try_emplace({std::min(pa, pb), std::max(pa, pb)}, std::make_pair(a, b));
      }
    }
    mesh.wireframeVertices = static_cast<int>(edges.size() * 6);
    mesh.wireframe = static_cast<LineVertex *>(linearAlloc(edges.size() * 6 * sizeof(LineVertex)));
    if (!mesh.wireframe)
      return;
    LineVertex *out = mesh.wireframe;
    for (const auto &[where, ends] : edges) {
      const Vertex &a = mesh.vertices[ends.first], &b = mesh.vertices[ends.second];
      writeLine(out, a.position, b.position, a.colour, b.colour);
      out += 6;
    }
    GSPGPU_FlushDataCache(mesh.wireframe, edges.size() * 6 * sizeof(LineVertex));
    mesh.triangles = {}; // (no longer needed)
  }
  drawLineVertices(mesh.wireframe, mesh.wireframeVertices, mvp, material);
}

void Renderer::Backend::drawLineBatches(const C3D_Mtx &viewProjection) {
  if (!lineVertices)
    return;
  for (const Lines &batch : lines) {
    if (!mayDraw())
      return;
    Material material;
    material.colour = batch.colour;
    drawLineVertices(lineVertices + batch.first / 2 * 6, static_cast<int>(batch.count / 2 * 6),
                     viewProjection, material);
    sent("lines", static_cast<std::size_t>(&batch - lines.data()));
  }
}

void Renderer::Backend::drawOverlay() {
  if (!overlayVertices || !mayDraw())
    return;
  C3D_BindProgram(&overlayProgram);
  bound = &overlayProgram;
  C3D_SetAttrInfo(&overlayAttributes);
  C3D_CullFace(GPU_CULL_NONE);
  C3D_BufInfo buffers;
  BufInfo_Init(&buffers);
  BufInfo_Add(&buffers, overlayVertices, sizeof(Overlay::Vertex), 2, 0x10);
  C3D_SetBufInfo(&buffers);
  // Points, from the top-left, across the turned screen
  C3D_Mtx projection;
  Mtx_OrthoTilt(&projection, 0, static_cast<float>(display.width()),
                static_cast<float>(display.height()), 0, 0, 1, false);
  C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, overlayProjection, &projection);
  C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
  C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                 GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
  C3D_DrawArrays(GPU_TRIANGLES, 0, static_cast<int>(overlayTriangles.size()));
  sent("overlay", 0);
}

// The frame's lines and overlay triangles written into linear memory, once
// for both eyes (nothing if they don't fit)
void Renderer::Backend::copyFrameVertices() {
  arena.used = 0; // (the last frame's drawing is done)
  overlayVertices = nullptr;
  lineVertices = nullptr;
  if (lineEnds.size() >= 2) {
    const std::size_t count = lineEnds.size() / 2;
    lineVertices = static_cast<LineVertex *>(arena.take(count * 6 * sizeof(LineVertex)));
    if (lineVertices) {
      for (std::size_t l = 0; l < count; ++l)
        writeLine(lineVertices + l * 6, lineEnds[l * 2], lineEnds[l * 2 + 1], Colour{}, Colour{});
      GSPGPU_FlushDataCache(lineVertices, count * 6 * sizeof(LineVertex));
    }
  }
  if (!overlayTriangles.empty()) {
    const std::size_t bytes = overlayTriangles.size() * sizeof(Overlay::Vertex);
    overlayVertices = static_cast<Overlay::Vertex *>(arena.take(bytes));
    if (overlayVertices) {
      std::memcpy(overlayVertices, overlayTriangles.data(), bytes);
      GSPGPU_FlushDataCache(overlayVertices, bytes);
    }
  }
}

void Renderer::Backend::drawEye(C3D_RenderTarget *target, const C3D_Mtx &projection) {
  C3D_RenderTargetClear(target, C3D_CLEAR_ALL,
                        clearColour(environment.backgroundSeen(camera.farPlane)), 0);
  C3D_FrameDrawOn(target);
  C3D_Mtx viewProjection;
  Mtx_Multiply(&viewProjection, &projection, &view);

  // Fog, by each pixel's depth (exp(-density x distance), as on desktops)
  const Debug::Settings &s = debug.settings;
  if (environment.fog.density > Scalar(0) && s.fog && s.view == Debug::View::Normal) {
    FogLut_Exp(&fog, environment.fog.density, 1, camera.nearPlane, camera.farPlane);
    C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
    C3D_FogColor(fogColour(environment.fog.colour));
    C3D_FogLutBind(&fog);
  } else {
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
  }
  C3D_CullFace(GPU_CULL_NONE);

  // Overdraw (a debug view): every mesh's every pixel adds a step, with no
  // depth test, so the brighter, the more often it was drawn
  const bool overdraw = s.view == Debug::View::Overdraw;
  const auto additive = [] {
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ONE, GPU_ONE);
  };

  // Solid meshes and lines
  if (overdraw) {
    additive();
  } else {
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
  }
  if (s.meshes)
    for (const Draw &draw : solid)
      drawMesh(draw, viewProjection);
  if (s.lines && !overdraw)
    drawLineBatches(viewProjection);

  // Translucent ones, farthest first, blending over what's behind them
  // without hiding what's drawn after
  if (!overdraw) {
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
  }
  if (s.translucent)
    for (const Draw &draw : later)
      drawMesh(draw, viewProjection);

  if (s.overlay)
    drawOverlay();
}

Renderer::Renderer(Display &display) : backend_(std::make_unique<Backend>(display)) {}

Renderer::~Renderer() = default;

Mesh Renderer::load(const MeshData &mesh) {
  Backend::LoadedMesh loaded;
  std::vector<std::uint32_t> indices = triangleList(mesh);
  const std::size_t vertexCount = mesh.vertices.size();
  if (vertexCount <= 65536) {
    loaded.vertices = static_cast<Vertex *>(linearAlloc(mesh.vertices.size_bytes()));
    std::memcpy(loaded.vertices, mesh.vertices.data(), mesh.vertices.size_bytes());
    GSPGPU_FlushDataCache(loaded.vertices, mesh.vertices.size_bytes());
    loaded.indices =
        static_cast<std::uint16_t *>(linearAlloc(indices.size() * sizeof(std::uint16_t)));
    for (std::size_t i = 0; i < indices.size(); ++i)
      loaded.indices[i] = static_cast<std::uint16_t>(indices[i]);
    GSPGPU_FlushDataCache(loaded.indices, indices.size() * sizeof(std::uint16_t));
    loaded.triangles = std::move(indices);
    loaded.count = static_cast<int>(loaded.triangles.size());
  } else {
    // Too many for 16-bit indices: each triangle's vertices in turn
    loaded.vertices = static_cast<Vertex *>(linearAlloc(indices.size() * sizeof(Vertex)));
    for (std::size_t i = 0; i < indices.size(); ++i)
      loaded.vertices[i] = mesh.vertices[indices[i]];
    GSPGPU_FlushDataCache(loaded.vertices, indices.size() * sizeof(Vertex));
    loaded.count = static_cast<int>(indices.size());
  }
  BufInfo_Init(&loaded.buffers);
  BufInfo_Add(&loaded.buffers, loaded.vertices, sizeof(Vertex), 3, 0x210);
  loaded.translucent = hasTranslucentVertices(mesh);
  loaded.bounds = boundsOf(mesh);
  loaded.doubleSided = mesh.doubleSided;
  const unsigned triangles = static_cast<unsigned>(loaded.count / 3);
  loaded.cost = {triangles, triangles * 3, 0};
  backend_->meshes.push_back(std::move(loaded));
  return Mesh{static_cast<std::uint32_t>(backend_->meshes.size())};
}

void Renderer::unload(Mesh mesh) {
  if (!mesh || mesh.id > backend_->meshes.size())
    return;
  Backend::LoadedMesh &loaded = backend_->meshes[mesh.id - 1];
  linearFree(loaded.vertices);
  linearFree(loaded.indices);
  linearFree(loaded.wireframe);
  loaded = {};
}

void Renderer::beginFrame(const Camera &camera, const Environment &environment) {
  Backend &b = *backend_;
  b.camera = camera;
  b.environment = environment;
  Mtx_LookAt(&b.view, FVec3_New(camera.eye.x, camera.eye.y, camera.eye.z),
             FVec3_New(camera.target.x, camera.target.y, camera.target.z),
             FVec3_New(camera.up.x, camera.up.y, camera.up.z), false);
  // What the camera sees, its sides moved out a little: in stereo each eye
  // sees a little further to its side
  b.frustum.emplace(camera, static_cast<float>(b.display.width()) / b.display.height(), 1.15f);
  b.solid.clear();
  b.later.clear();
  b.lineEnds.clear();
  b.lines.clear();
  b.counting = {};
  overlay_.clear();
}

void Renderer::draw(Mesh mesh, const Transform &where, const Material &material) {
  Backend &b = *backend_;
  if (!mesh || mesh.id > b.meshes.size() || !b.meshes[mesh.id - 1].vertices)
    return;
  const Backend::LoadedMesh &loaded = b.meshes[mesh.id - 1];
  // Wholly out of view: not drawn (Culling.hpp)
  if (b.debug.settings.culling && b.frustum->outside(loaded.bounds, where)) {
    ++b.counting.culled;
    return;
  }
  const float distance = (where.position() - b.camera.eye).length();
  if (material.alpha < 31 || loaded.translucent)
    b.later.push_back({mesh, where, material, distance});
  else
    b.solid.push_back({mesh, where, material, distance});
  b.counting += loaded.cost;
  ++b.counting.draws;
}

void Renderer::drawLines(std::span<const Vector3> ends, Colour colour) {
  if (ends.size() < 2)
    return;
  Backend &b = *backend_;
  const std::size_t count = ends.size() / 2 * 2;
  b.lines.push_back({b.lineEnds.size(), count, colour});
  b.lineEnds.insert(b.lineEnds.end(), ends.begin(),
                    ends.begin() + static_cast<std::ptrdiff_t>(count));
  // (each line: two triangles)
  b.counting.polygons += static_cast<unsigned>(count);
  b.counting.vertices += static_cast<unsigned>(count * 3);
  ++b.counting.draws;
}

bool Renderer::toScreen(const Vector3 &point, float &x, float &y) const {
  const Backend &b = *backend_;
  // The mono projection, not turned to the screen: x across, y up
  C3D_Mtx projection, viewProjection;
  Mtx_Persp(&projection, C3D_AngleFromDegrees(b.camera.fieldOfView),
            static_cast<float>(b.display.width()) / static_cast<float>(b.display.height()),
            b.camera.nearPlane, b.camera.farPlane, false);
  Mtx_Multiply(&viewProjection, &projection, &b.view);
  const C3D_FVec clip = Mtx_MultiplyFVec4(&viewProjection, FVec4_New(point.x, point.y, point.z, 1));
  if (clip.w <= 0)
    return false;
  x = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(b.display.width());
  y = (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(b.display.height());
  return true;
}

void Renderer::endFrame() {
  Backend &b = *backend_;
  std::ranges::stable_sort(b.later, [](const Backend::Draw &x, const Backend::Draw &y) {
    return x.distance > y.distance;
  });
  const Debug::Settings &debug = b.debug.settings;
  b.debug.drawingSince.store(osGetTime(), std::memory_order_relaxed);
  b.debug.drawing.store(true, std::memory_order_release);
  // Finding a hang: one more draw a frame every half second (30 frames),
  // back to one when it's turned on
  if (debug.findHang) {
    if (++b.debug.framesAtLimit >= 30 && b.debug.drawLimit <= b.debug.drawsLastFrame) {
      ++b.debug.drawLimit;
      b.debug.framesAtLimit = 0;
    }
  } else {
    b.debug.drawLimit = 1;
    b.debug.framesAtLimit = 0;
  }
  b.debug.drawsThisFrame = 0;
  // The probe's cross (a debug setting), over everything
  if (debug.probe) {
    const auto x = static_cast<float>(debug.probeX), y = static_cast<float>(debug.probeY);
    overlay_.rectangle(x - 6, y, 4, 1, {255, 0, 255});
    overlay_.rectangle(x + 3, y, 4, 1, {255, 0, 255});
    overlay_.rectangle(x, y - 6, 1, 4, {255, 0, 255});
    overlay_.rectangle(x, y + 3, 1, 4, {255, 0, 255});
  }
  // The overlay's shapes, then its text over them, as triangles
  b.overlayTriangles.assign(overlay_.triangles().begin(), overlay_.triangles().end());
  for (const Overlay::Text &text : overlay_.texts())
    appendTextTriangles(text, b.overlayTriangles);

  // Each eye apart by the slider (a third of it, as devkitPro's examples
  // have it), or one with it down
  const float separation = debug.stereo ? osGet3DSliderState() / 3 : 0;
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
  b.copyFrameVertices();
  b.bound = nullptr;
  b.eye = 1;
  b.drawEye(b.display.left(), projectionFor(b.camera, -separation));
  if (separation > 0) {
    b.eye = 2;
    b.drawEye(b.display.right(), projectionFor(b.camera, separation));
  }
  C3D_FrameEnd(0);

  // The probe: the pixel under the cross, from the frame the screen shows
  // now or the one before (the top screen's framebuffer is turned: 240
  // pixels a column, from the bottom, 3 bytes a pixel, blue first)
  if (debug.probe) {
    u16 height = 0, width = 0; // (as the framebuffer is laid out: 240 x 400)
    const u8 *pixels = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &height, &width);
    const int x = std::clamp(debug.probeX, 0, 399), y = std::clamp(debug.probeY, 0, 239);
    const u8 *pixel = pixels + (static_cast<std::size_t>(x) * 240 + (239 - y)) * 3;
    b.debug.probed = {pixel[2], pixel[1], pixel[0]};
  }

  b.times = {C3D_GetDrawingTime(), C3D_GetProcessingTime()};
  b.last = std::exchange(b.counting, {});
  b.debug.drawsLastFrame = b.debug.drawsThisFrame;
  overlay_.clear();
  b.debug.drawing.store(false, std::memory_order_release);
}

Capabilities Renderer::capabilities() const { return {}; }

FrameCounts Renderer::lastFrame() const { return backend_->last; }

FrameTimes Renderer::lastFrameTimes() const { return backend_->times; }

} // namespace ScratchPadd::Scene
