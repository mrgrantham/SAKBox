// Scene::Renderer on the DS's 3D hardware (libnds' GX), on the top screen:
// meshes as display lists (DisplayList.hpp), each drawn with its transform
// multiplied into the hardware's matrices, lit by its four directional
// lights; translucent meshes kept to the end of the frame and drawn
// farthest first; the overlay's shapes as 3D triangles in front of
// everything and its text on the screen's 2D text layer. A debug build
// stops with a message if a frame overflows the DS's polygon or vertex RAM.
#include "Backend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string_view>

namespace ScratchPadd::Scene {

namespace {

using Flex::Math::HardwareMathsLock;

// What the DS's polygon and vertex RAM hold a frame (GBATEK)
constexpr unsigned kMostPolygons = 2048, kMostVertices = 6144;
// The text layer's size, in 8-point cells
constexpr int kTextColumns = 32, kTextRows = 24, kCell = 8;
// Overlay points are given to the hardware as 4.12 vertices 64 points to
// the unit, so the screen (256 x 192) is 4 x 3
constexpr int kPointsPerUnit = 64;
// Translucent polygons only blend over others with a different polygon ID:
// each translucent draw takes the next of 1 to 62 (0 is the solid ones', 63
// the clear plane's)
constexpr unsigned kFirstTranslucentId = 1, kLastTranslucentId = 62;

// A Scalar as the DS's 20.12
int f32(Scalar value) { return DisplayList::toFixed(value, 12); }

// A unit vector's component as the DS's 1.9
v10 v10Of(Scalar value) {
  return static_cast<v10>(std::clamp<std::int32_t>(DisplayList::toFixed(value, 9), -512, 511));
}

// The console colour nearest `colour`: one of libnds' eight, bright or not
struct ConsoleColour {
  int number;
  bool bright;
};
ConsoleColour nearestConsoleColour(Colour colour) {
  static constexpr Colour kColours[16] = {
      {0, 0, 0},     {128, 0, 0},     {0, 128, 0},     {128, 128, 0},  {0, 0, 128}, {128, 0, 128},
      {0, 128, 128}, {192, 192, 192}, {128, 128, 128}, {255, 0, 0},    {0, 255, 0}, {255, 255, 0},
      {0, 0, 255},   {255, 0, 255},   {0, 255, 255},   {255, 255, 255}};
  int best = 15, bestDistance = 1 << 30;
  for (int i = 0; i < 16; ++i) {
    const int dr = colour.r - kColours[i].r, dg = colour.g - kColours[i].g,
              db = colour.b - kColours[i].b;
    const int distance = dr * dr + dg * dg + db * db;
    if (distance < bestDistance) {
      best = i;
      bestDistance = distance;
    }
  }
  return {best % 8, best >= 8};
}

bool sameText(const std::vector<Overlay::Text> &a, const std::vector<Overlay::Text> &b) {
  return std::ranges::equal(a, b, [](const Overlay::Text &x, const Overlay::Text &y) {
    return x.x == y.x && x.y == y.y && x.text == y.text && x.colour == y.colour;
  });
}

} // namespace

const std::vector<std::uint32_t> &Renderer::Backend::listFor(LoadedMesh &mesh,
                                                             const DisplayList::Variant &variant) {
  const auto found = std::ranges::find(mesh.lists, variant, [](const auto &l) { return l.first; });
  if (found != mesh.lists.end())
    return found->second;
  if (mesh.lists.size() == LoadedMesh::kMostLists)
    mesh.lists.erase(mesh.lists.begin());
  mesh.lists.emplace_back(variant, DisplayList::pack(mesh.source, variant));
  return mesh.lists.back().second;
}

void Renderer::Backend::drawNow(Mesh mesh, const Transform &where, const Material &material,
                                unsigned polygonId) {
  if (!mesh || mesh.id > meshes.size() || !meshes[mesh.id - 1].loaded)
    return;
  LoadedMesh &loaded = meshes[mesh.id - 1];
  const bool lit = material.shading == Material::Shading::lit;
  const std::vector<std::uint32_t> &list =
      listFor(loaded, {lit, material.colour, lit ? ambient : Colour{}});

  // The transform as a turn and a move, multiplied into both the position
  // and the normals' matrices, then its scale (and the mesh's own, see
  // DisplayList::Source), which the DS applies to positions only: normals
  // stay unit length, as the DS doesn't make them so
  m4x3 matrix;
  int scales[3];
  for (int column = 0; column < 3; ++column) {
    const Vector3 axis{where.m[0][column], where.m[1][column], where.m[2][column]};
    const Scalar length = axis.length();
    const Vector3 unit = length > Scalar(0) ? axis.normalised() : axis;
    // The DS's matrices take row vectors: its row is the transform's column
    matrix.m[column * 3 + 0] = f32(unit.x);
    matrix.m[column * 3 + 1] = f32(unit.y);
    matrix.m[column * 3 + 2] = f32(unit.z);
    scales[column] = f32(length) << loaded.source.scaleShift;
  }
  for (int row = 0; row < 3; ++row)
    matrix.m[9 + row] = f32(where.m[row][3]);

  glPushMatrix();
  glMultMatrix4x3(&matrix);
  glScalef32(scales[0], scales[1], scales[2]);
  const unsigned alpha = material.wireframe ? 0 : std::min<unsigned>(material.alpha, 31);
  glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(polygonId) | (lit ? lightBits : 0));
  glCallList(list.data());
  glPopMatrix(1);
}

void Renderer::Backend::drawOverlayShapes(const Overlay &overlay) {
  const std::vector<Overlay::Vertex> &triangles = overlay.triangles();
  if (triangles.empty())
    return;
  // Points on the screen, top-left 0, 0, in front of the 3D: z from 0 (the
  // nearest) to 16 units away, each triangle a step nearer than the last,
  // so later shapes are drawn over earlier ones
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  {
    HardwareMathsLock lock; // the divider
    glOrthof32(0, inttof32(display.width()) / kPointsPerUnit,
               inttof32(display.height()) / kPointsPerUnit, 0, 0, inttof32(16));
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  auto coordinate = [](float points) {
    return static_cast<v16>(
        std::clamp(static_cast<int>(points * (4096 / kPointsPerUnit)), -32767, 32767));
  };
  const int count = static_cast<int>(triangles.size() / 3);
  unsigned translucentId = kFirstTranslucentId;
  for (int t = 0; t < count; ++t) {
    const Overlay::Vertex *corner = &triangles[static_cast<std::size_t>(t) * 3];
    const unsigned alpha = corner[0].colour.a >> 3;
    if (alpha == 0)
      continue; // (alpha 0 would be a wireframe)
    unsigned id = 0;
    if (alpha < 31) {
      id = translucentId;
      translucentId = translucentId == kLastTranslucentId ? kFirstTranslucentId : translucentId + 1;
    }
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_TRIANGLES);
    const auto z = static_cast<v16>(-(count - t));
    for (int k = 0; k < 3; ++k) {
      glColor(DisplayList::rgb15(corner[k].colour));
      glVertex3v16(coordinate(corner[k].x), coordinate(corner[k].y), z);
    }
    glEnd();
  }
}

void Renderer::Backend::drawOverlayText(const Overlay &overlay) {
  if (sameText(overlay.texts(), textShown))
    return; // printing is slow: only when it changes
  PrintConsole *previous = consoleSelect(&display.textLayer());
  std::printf("\x1b[2J");
  for (const Overlay::Text &text : overlay.texts()) {
    const int column = static_cast<int>(text.x) / kCell;
    int row = static_cast<int>(text.y) / kCell;
    if (column < 0 || column >= kTextColumns - 1)
      continue;
    const ConsoleColour colour = nearestConsoleColour(text.colour);
    std::printf(colour.bright ? "\x1b[3%d;1m" : "\x1b[3%dm", colour.number);
    std::string_view rest = text.text;
    while (row < kTextRows) {
      const std::size_t end = std::min(rest.find('\n'), rest.size());
      // (Not the last column: writing it moves the cursor to the next
      // line, and at the bottom scrolls the layer)
      const int fits = std::min<int>(static_cast<int>(end), kTextColumns - 1 - column);
      if (row >= 0) // libnds' console counts rows and columns from 0
        std::printf("\x1b[%d;%dH%.*s", row, column, fits, rest.data());
      if (end == rest.size())
        break;
      rest.remove_prefix(end + 1);
      ++row;
    }
    std::printf("\x1b[39m");
  }
  consoleSelect(previous);
  textShown = overlay.texts();
}

Renderer::Renderer(Display &display) : backend_(std::make_unique<Backend>(display)) {
  glInit();
  glEnable(GL_ANTIALIAS);
  glEnable(GL_BLEND);
  glClearPolyID(63);
  glClearDepth(GL_MAX_DEPTH);
  glViewport(0, 0, 255, 191);
}

Renderer::~Renderer() = default;

Mesh Renderer::load(const MeshData &mesh) {
  Backend::LoadedMesh loaded;
  loaded.loaded = true;
  loaded.source = DisplayList::prepare(mesh);
  backend_->meshes.push_back(std::move(loaded));
  return Mesh{static_cast<std::uint32_t>(backend_->meshes.size())};
}

void Renderer::unload(Mesh mesh) {
  if (mesh && mesh.id <= backend_->meshes.size())
    backend_->meshes[mesh.id - 1] = {};
}

void Renderer::beginFrame(const Camera &camera, const Environment &environment) {
  Backend &b = *backend_;
  b.camera = camera;
  b.ambient = environment.ambient;
  b.later.clear();
  overlay_.clear();
  const Colour &background = environment.background;
  glClearColor(background.r >> 3, background.g >> 3, background.b >> 3, 31);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  {
    HardwareMathsLock lock; // these use the divider and square root unit
    glMatrixMode(GL_PROJECTION);
    // The field of view in libnds' angles (32768 a turn)
    const int angle = static_cast<int>(static_cast<std::int64_t>(f32(camera.fieldOfView)) *
                                       DEGREES_IN_CIRCLE / (360 * 4096));
    gluPerspectivef32(angle, inttof32(b.display.width()) / b.display.height(),
                      f32(camera.nearPlane), f32(camera.farPlane));
    glMatrixMode(GL_MODELVIEW);
    gluLookAtf32(f32(camera.eye.x), f32(camera.eye.y), f32(camera.eye.z), f32(camera.target.x),
                 f32(camera.target.y), f32(camera.target.z), f32(camera.up.x), f32(camera.up.y),
                 f32(camera.up.z));
  }

  // The lights, set after the view: the DS turns a light's direction by the
  // view as it's set, so they're given in the world
  b.lightBits = 0;
  for (unsigned i = 0; i < environment.lightCount; ++i) {
    const Light &light = environment.lights[i];
    const Vector3 direction = light.direction.normalised();
    glLight(static_cast<int>(i), DisplayList::rgb15(light.colour), v10Of(direction.x),
            v10Of(direction.y), v10Of(direction.z));
    b.lightBits |= POLY_FORMAT_LIGHT0 << i;
  }
}

void Renderer::draw(Mesh mesh, const Transform &where, const Material &material) {
  Backend &b = *backend_;
  if (material.alpha < 31 && !material.wireframe) {
    b.later.push_back({mesh, where, material, (where.position() - b.camera.eye).length()});
    return;
  }
  b.drawNow(mesh, where, material, 0);
}

void Renderer::drawLines(std::span<const Vector3> ends, Colour colour) {
  if (ends.size() < 2)
    return;
  // In the world, scaled down by a power of two to fit 4.12, as meshes are
  std::int32_t furthest = 0;
  for (const Vector3 &end : ends)
    for (Scalar c : {end.x, end.y, end.z})
      furthest = std::max(furthest, std::abs(DisplayList::toFixed(c, 12)));
  int shift = 0;
  while ((furthest >> shift) > 0x7FFF)
    ++shift;
  auto coordinate = [shift](Scalar c) {
    return static_cast<v16>(DisplayList::toFixed(c, 12 - shift));
  };
  glPushMatrix();
  glScalef32(inttof32(1) << shift, inttof32(1) << shift, inttof32(1) << shift);
  glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
  glBegin(GL_TRIANGLES);
  glColor(DisplayList::rgb15(colour));
  // Each a triangle with two corners at the same place, which the DS draws
  // as a line
  for (std::size_t i = 0; i + 1 < ends.size(); i += 2) {
    const Vector3 &a = ends[i], &b = ends[i + 1];
    glVertex3v16(coordinate(a.x), coordinate(a.y), coordinate(a.z));
    glVertex3v16(coordinate(b.x), coordinate(b.y), coordinate(b.z));
    glVertex3v16(coordinate(b.x), coordinate(b.y), coordinate(b.z));
  }
  glEnd();
  glPopMatrix(1);
}

void Renderer::endFrame() {
  Backend &b = *backend_;
  // Translucent meshes, farthest first (drawn in this order: the frame is
  // flushed with manual sorting), each with its own polygon ID so they
  // blend over each other
  std::ranges::stable_sort(b.later, [](const Backend::Later &x, const Backend::Later &y) {
    return x.distance > y.distance;
  });
  unsigned id = kFirstTranslucentId;
  for (const Backend::Later &item : b.later) {
    b.drawNow(item.mesh, item.where, item.material, id);
    id = id == kLastTranslucentId ? kFirstTranslucentId : id + 1;
  }
  b.later.clear();
  b.drawOverlayShapes(overlay_);
  b.drawOverlayText(overlay_);
  overlay_.clear();

  // What the frame used of the polygon and vertex RAM, once the geometry
  // engine has taken in all of it
  while (GFX_BUSY) {
  }
  b.last = {static_cast<unsigned>(GFX_POLYGON_RAM_USAGE),
            static_cast<unsigned>(GFX_VERTEX_RAM_USAGE)};
  const bool overflowed = (GFX_CONTROL & BIT(13)) != 0;
  if (overflowed)
    GFX_CONTROL |= BIT(13); // acknowledged
  sassert(!overflowed,
          // (Lines within the bottom screen's 32 columns)
          "Over the DS's 3D limits:\n%u of %u polygons,\n%u of %u vertices.\n"
          "What's past them isn't drawn.\ntools/assets' scene rule checks\n"
          "a scene when it's built.",
          b.last.polygons, kMostPolygons, b.last.vertices, kMostVertices);

  glFlush(GL_TRANS_MANUALSORT);
  swiWaitForVBlank();
}

bool Renderer::toScreen(const Vector3 &point, float &x, float &y) const {
  // In float (software floating point on the DS: slow, but it's rarely
  // needed): the camera's view, then its projection
  const Camera &camera = backend_->camera;
  auto toFloat = [](const Vector3 &v) {
    return std::array<float, 3>{static_cast<float>(v.x), static_cast<float>(v.y),
                                static_cast<float>(v.z)};
  };
  auto normalise = [](std::array<float, 3> v) {
    const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return length > 0 ? std::array<float, 3>{v[0] / length, v[1] / length, v[2] / length} : v;
  };
  auto cross = [](const std::array<float, 3> &a, const std::array<float, 3> &b) {
    return std::array<float, 3>{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                                a[0] * b[1] - a[1] * b[0]};
  };
  auto dot = [](const std::array<float, 3> &a, const std::array<float, 3> &b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  };
  const auto eye = toFloat(camera.eye), target = toFloat(camera.target), p = toFloat(point);
  const auto forward = normalise({target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]});
  const auto side = normalise(cross(forward, toFloat(camera.up)));
  const auto up = cross(side, forward);
  const std::array<float, 3> relative{p[0] - eye[0], p[1] - eye[1], p[2] - eye[2]};
  const float viewX = dot(side, relative), viewY = dot(up, relative),
              depth = dot(forward, relative); // in front of the eye: positive
  if (depth <= 0)
    return false;
  const float width = static_cast<float>(backend_->display.width()),
              height = static_cast<float>(backend_->display.height());
  const float focal = 1 / std::tan(static_cast<float>(camera.fieldOfView) * 3.14159265f / 360);
  const float ndcX = focal * height / width * viewX / depth, ndcY = focal * viewY / depth;
  x = (ndcX * 0.5f + 0.5f) * width;
  y = (0.5f - ndcY * 0.5f) * height;
  return true;
}

Capabilities Renderer::capabilities() const {
  return {.lights = Environment::kMostLights, .polygonsPerFrame = kMostPolygons, .fog = false};
}

FrameCounts Renderer::lastFrame() const { return backend_->last; }

// The overlay's text on the DS: the text layer's 8-point cells, one size
float Overlay::textWidth(std::string_view text, float) {
  std::size_t widest = 0;
  while (true) {
    const std::size_t end = std::min(text.find('\n'), text.size());
    widest = std::max(widest, end);
    if (end == text.size())
      break;
    text.remove_prefix(end + 1);
  }
  return static_cast<float>(widest * kCell);
}

float Overlay::lineHeight(float) { return kCell; }

} // namespace ScratchPadd::Scene
