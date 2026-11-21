// Scene::Renderer on the DS's 3D hardware (libnds' GX), on the top screen:
// meshes as display lists of quads, strips and triangles (DisplayList.hpp),
// each drawn with its transform multiplied into the hardware's matrices,
// lit by its four directional lights, and fogged by the hardware's fog;
// translucent meshes kept to the end of the frame and drawn farthest first;
// the overlay's shapes as 3D triangles in front of everything and its text
// on the screen's 2D bitmap layer (stb_easy_font's letters). A debug build stops with a message if
// a frame overflows the DS's polygon or vertex RAM.
#include "Backend.hpp"
#include "ToonTable.hpp"

#include <ScratchPadd/Scene/OverlayText.hpp>

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
// The text layer's size
constexpr int kTextWidth = 256, kTextHeight = 192;
// Overlay points are given to the hardware as 4.12 vertices 64 points to
// the unit, so the screen (256 x 192) is 4 x 3
constexpr int kPointsPerUnit = 64;
// Translucent polygons only blend over others with a different polygon ID:
// each translucent draw takes the next of 1 to 62 (0 is the solid ones', 63
// the clear plane's)
constexpr unsigned kFirstTranslucentId = 1, kLastTranslucentId = 62;
// A polygon format bit libnds doesn't name (GBATEK's POLYGON_ATTR bit 11):
// translucent pixels set their own depth, rather than keep what's behind
// them. Fog is by the depth in the buffer, so without it a translucent
// mesh over the background was fogged as if at the far plane.
constexpr std::uint32_t kTranslucentSetsDepth = BIT(11);
// Outlines are the DS's edge marking: a line on a solid polygon's pixels
// where the one beside it has a different polygon ID (GBATEK). So each
// outlined mesh takes its own ID, 8 to 55 in turn; meshes without one keep
// 0, and so does the clear plane in a frame with outlines (otherwise every
// mesh would be outlined against the background). Edge marking's colours
// go by ID / 8: all set to the outline's.
constexpr unsigned kFirstOutlineId = 8, kLastOutlineId = 55;
constexpr unsigned kClearId = 63, kClearIdWithOutlines = 0;
// The textures (pictures, and toon shading's: DisplayList::textureFor, 128
// bytes each, 8 x 8 direct colours) live in VRAM banks B and A, mapped as
// texture slots 0 and 1: 256 KB, handed out in turn (a texture unloaded
// doesn't give its space back). Bank A was mapped by nothing; with B alone
// (128 KB) the last pictures loaded, the card backs, got none
constexpr std::uint32_t kSlotBytes = 128 * 1024;
// The banks to the CPU, to write textures (as LCD memory), and back to the
// 3D hardware as its texture slots
void texturesToCpu() {
  vramSetBankB(VRAM_B_LCD);
  vramSetBankA(VRAM_A_LCD);
}
void texturesToGpu() {
  vramSetBankB(VRAM_B_TEXTURE_SLOT0);
  vramSetBankA(VRAM_A_TEXTURE_SLOT1);
}
// Waits (a frame at most) for the lines between the screen's last and the
// 3D hardware starting its next frame (GBATEK: it renders 48 lines ahead,
// from line 214): texture memory taken from it then shows nowhere. Taken
// while it drew, the lines drawn meanwhile read nothing (the user's
// jittery scanlines over the board as a card's close-up was drawn)
void waitForTheBlank() {
  while (!(REG_VCOUNT >= 192 && REG_VCOUNT < 208)) {
  }
}
// Where texture memory at `offset` is, to the CPU (banks mapped to it):
// slot 0 in bank B, slot 1 in bank A
std::uint16_t *vramAt(std::uint32_t offset) {
  return offset < kSlotBytes ? VRAM_B + offset / 2 : VRAM_A + (offset - kSlotBytes) / 2;
}
// (a toon texture's bytes: 8 x 8 texels)
constexpr std::uint32_t kTextureBytes = DisplayList::kTexels * 2;
// TEXIMAGE_PARAM (GBATEK, "DS 3D Texture Attributes"): the texture's VRAM
// offset in 8-byte steps, its width and height (8 << n: 0), its format (7:
// direct colour), its coordinates as given (no transformation)
constexpr std::uint32_t textureParameter(std::uint32_t offset, std::uint32_t widthCode = 0,
                                         std::uint32_t heightCode = 0) {
  return (offset >> 3) | (widthCode << 20) | (heightCode << 23) | (7u << 26);
}

// A side the DS's textures can have (8 to 1024, a power of two) as its
// TEXIMAGE_PARAM code (8 << code), or -1
int sideCode(int side) {
  for (int code = 0; code <= 7; ++code)
    if (side == (8 << code))
      return code;
  return -1;
}

// A Scalar as the DS's 20.12
int f32(Scalar value) { return DisplayList::toFixed(value, 12); }

// A unit vector's component as the DS's 1.9
v10 v10Of(Scalar value) {
  return static_cast<v10>(std::clamp<std::int32_t>(DisplayList::toFixed(value, 9), -512, 511));
}

// Whether two overlay triangles are one shape's: the same colour, and two
// corners in common (an edge between them)
bool sameShape(const Overlay::Vertex *a, const Overlay::Vertex *b) {
  if (!(a[0].colour == b[0].colour))
    return false;
  int shared = 0;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      shared += a[i].x == b[j].x && a[i].y == b[j].y;
  return shared >= 2;
}

bool sameText(const std::vector<Overlay::Text> &a, const std::vector<Overlay::Text> &b) {
  return std::ranges::equal(a, b, [](const Overlay::Text &x, const Overlay::Text &y) {
    return x.x == y.x && x.y == y.y && x.text == y.text && x.colour == y.colour &&
           x.scale == y.scale && x.clip == y.clip;
  });
}

} // namespace

const std::vector<DisplayList::Packed> &
Renderer::Backend::listsFor(LoadedMesh &mesh, const DisplayList::Variant &variant) {
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
  ++draws;
  const bool lit = material.shading != Material::Shading::Unlit;
  const bool toon = material.shading == Material::Shading::Toon;
  // Toon: its colours from its texture (the DS draws an untextured toon
  // mesh in the table's colours: ToonTable.hpp), lit as white so the light
  // picks the table's band
  // An image texture instead, when the material has one: its colours from
  // the image, each corner where its uv puts it
  const LoadedImage *image = material.texture && material.texture.id <= images.size() &&
                                     images[material.texture.id - 1].parameter
                                 ? &images[material.texture.id - 1]
                                 : nullptr;
  const std::uint32_t texture =
      image ? image->parameter : (toon ? textureFor(loaded, material.colour) : 0);
  GFX_TEX_FORMAT = texture;
  const std::vector<DisplayList::Packed> &lists =
      listsFor(loaded, {.lit = lit,
                        .colour = material.colour,
                        .ambient = lit ? ambient : Colour{},
                        .textured = !image && texture != 0,
                        .imageWidth = image ? image->width : 0,
                        .imageHeight = image ? image->height : 0});

  // The transform as a turn and a move, multiplied into both the position
  // and the normals' matrices, then its scale (and the mesh's own, see
  // DisplayList::Source), which the DS applies to positions only: normals
  // stay unit length, as the DS doesn't make them so
  m4x3 matrix;
  int scales[3];
  for (int column = 0; column < 3; ++column) {
    const Vector3 axis{where.m[0][column], where.m[1][column], where.m[2][column]};
    // (its length once, then each component divided by it: not normalised(),
    // which would work the length out again)
    // (length 1, as a move's or a turn's are: no divides at all)
    const Scalar length = axis.length();
    const Vector3 unit = length > Scalar(0) && length != Scalar(1)
                             ? Vector3{axis.x / length, axis.y / length, axis.z / length}
                             : axis;
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
  // Each list's polygons with its alpha times the material's (a polygon's
  // format is taken when its list begins them)
  const unsigned materialAlpha = std::min<unsigned>(material.alpha, 31);
  // Back faces culled (counter-clockwise is the front), unless either side
  // shows: a double-sided mesh or material, or a wireframe. A culled
  // polygon takes none of the DS's 2048.
  const bool cull = !loaded.doubleSided && !material.doubleSided && !material.wireframe;
  const std::uint32_t format = (cull ? POLY_CULL_BACK : POLY_CULL_NONE) | POLY_ID(polygonId) |
                               (lit ? lightBits : 0) | (toon ? POLY_TOON_HIGHLIGHT : 0) | fogBit;
  for (const DisplayList::Packed &packed : lists) {
    unsigned alpha = 0; // a wireframe
    if (!material.wireframe) {
      alpha = (packed.alpha * materialAlpha + 15) / 31;
      if (alpha == 0)
        continue; // wholly see-through (alpha 0 would draw a wireframe)
    }
    const bool translucent = alpha > 0 && alpha < 31; // (0: a wireframe, drawn solid)
    glPolyFmt(POLY_ALPHA(alpha) | format | (translucent ? kTranslucentSetsDepth : 0));
    glCallList(packed.words.data());
  }
  glPopMatrix(1);
}

std::uint32_t Renderer::Backend::textureFor(LoadedMesh &mesh, Colour colour) {
  for (const auto &[made, parameter] : mesh.textures)
    if (made == colour)
      return parameter;
  const std::optional<std::uint32_t> at = textures.take(kTextureBytes);
  if (!at)
    return 0; // (the banks are full: drawn in the toon table's colours)
  const std::vector<std::uint16_t> texels = DisplayList::textureFor(mesh.source, colour);
  // Written with the bank mapped to the CPU, then mapped back, between frames
  waitForTheBlank();
  texturesToCpu();
  std::copy(texels.begin(), texels.end(), vramAt(*at));
  texturesToGpu();
  const std::uint32_t parameter = textureParameter(*at);
  mesh.textures.emplace_back(colour, parameter);
  return parameter;
}

void Renderer::Backend::setFog(const Fog &fog) {
  // The background (the rear plane) is fogged too, as at the far plane: a
  // translucent pixel keeps fog only if what's under it had it (GBATEK), so
  // without, translucent meshes over the background weren't fogged. Its
  // flag is bit 15 of libnds' clear colour, which glClearColor keeps.
  constexpr std::uint32_t kRearPlaneFog = BIT(15);
  if (!(fog.density > Scalar(0))) {
    glDisable(GL_FOG);
    fogBit = 0;
    GFX_CLEAR_COLOR = glCurClearColor &= ~kRearPlaneFog;
    return;
  }
  glEnable(GL_FOG);
  fogBit = POLY_FOG;
  GFX_CLEAR_COLOR = glCurClearColor |= kRearPlaneFog;
  if (fog.colour == fogSet.colour && fog.density == fogSet.density && camera.nearPlane == fogNear &&
      camera.farPlane == fogFar)
    return;
  fogSet = fog;
  fogNear = camera.nearPlane;
  fogFar = camera.farPlane;
  glFogColor(fog.colour.r >> 3, fog.colour.g >> 3, fog.colour.b >> 3, 31);

  // The DS fogs by depth, as its depth buffer holds it: 0 to 0x7FFF from
  // the near plane to the far one, z / w of the projection, so most of it
  // near the eye. Its 32 densities are at depths FOG_OFFSET + step x (n +
  // 1), step = 0x400 >> FOG_SHIFT, linear between them (GBATEK, "DS 3D
  // Alpha-Blending and Fog"). They're placed from where the fog starts to
  // show (1/128) to where it's whole (127/128) or the far plane, as finely
  // as the shift allows, each the desktops' 1 - exp(-density x distance) at
  // its depth's distance along the view. (Desktops fog by distance from
  // the eye, so a little more at the screen's edges.) In float: only when
  // the fog or the camera's planes change.
  const float near = static_cast<float>(camera.nearPlane),
              far = static_cast<float>(camera.farPlane), density = static_cast<float>(fog.density);
  auto depthAt = [&](float distance) {
    const float z = (far + near) / (far - near) - 2 * far * near / ((far - near) * distance);
    return std::clamp(static_cast<int>((z + 1) / 2 * 0x7FFF), 0, 0x7FFF);
  };
  auto distanceAt = [&](int depth) {
    const float z = static_cast<float>(depth) / 0x7FFF * 2 - 1;
    return 2 * far * near / ((far + near) - z * (far - near));
  };
  const int from = depthAt(std::max(near, std::log(128.0f / 127) / density)),
            to = depthAt(std::min(far, std::log(128.0f) / density));
  int shift = 10; // the finest: a step of 1
  while (shift > 0 && from + 32 * (0x400 >> shift) < to)
    --shift;
  const int step = 0x400 >> shift, offset = std::max(0, from - step);
  glFogShift(shift);
  glFogOffset(offset);
  for (int n = 0; n < 32; ++n) {
    const int depth = std::min(offset + step * (n + 1), 0x7FFF);
    const float fogged = 1 - std::exp(-density * distanceAt(depth));
    glFogDensity(n, std::clamp(static_cast<int>(std::lround(fogged * 128)), 0, 127));
  }
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
               inttof32(display.height()) / kPointsPerUnit, 0, 0,
               inttof32(DisplayList::kOverlayFar));
  }
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  auto coordinate = [](float points) {
    return static_cast<v16>(
        std::clamp(static_cast<int>(points * (4096 / kPointsPerUnit)), -32767, 32767));
  };
  const int count = static_cast<int>(triangles.size() / 3);
  GFX_TEX_FORMAT = 0;                          // (untextured)
  unsigned &translucentId = nextTranslucentId; // (after the frame's translucent meshes')
  unsigned lastId = 0;
  for (int t = 0; t < count; ++t) {
    const Overlay::Vertex *corner = &triangles[static_cast<std::size_t>(t) * 3];
    const unsigned alpha = corner[0].colour.a >> 3;
    if (alpha == 0)
      continue; // (alpha 0 would be a wireframe)
    unsigned id = 0;
    if (alpha < 31) {
      // A see-through shape's triangles share one ID (a rectangle's two, a
      // hexagon's six: the same colour, touching the last along an edge),
      // so where they meet isn't blended twice: the DS blends a translucent
      // polygon only over others with a different ID (a band's
      // diagonal showed as a dark line)
      if (t > 0 && lastId != 0 && sameShape(corner, corner - 3)) {
        id = lastId;
      } else {
        id = translucentId;
        translucentId =
            translucentId == kLastTranslucentId ? kFirstTranslucentId : translucentId + 1;
      }
    }
    lastId = id;
    glPolyFmt(POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id));
    glBegin(GL_TRIANGLES);
    // (each a depth step nearer than the last: DisplayList::overlayDepth)
    const auto z = static_cast<v16>(DisplayList::overlayDepth(t, count));
    for (int k = 0; k < 3; ++k) {
      glColor(DisplayList::rgb15(corner[k].colour));
      glVertex3v16(coordinate(corner[k].x), coordinate(corner[k].y), z);
    }
    glEnd();
  }
}

void Renderer::Backend::drawOverlayText(const Overlay &overlay) {
  if (sameText(overlay.texts(), textShown))
    return; // only when it changes
  // Each colour its own palette entry (0 is see-through), up to 255
  std::vector<std::uint16_t> &colours = textColours;
  colours.clear();
  // Only the rows the last text covered need clearing: the rest are clear
  // already (not the whole 48 KB each time the text changes, as
  // it does every frame of a moving banner)
  if (textRowsTo > textRowsFrom)
    std::fill(textPixels.begin() + static_cast<std::ptrdiff_t>(textRowsFrom) * kTextWidth,
              textPixels.begin() + static_cast<std::ptrdiff_t>(textRowsTo) * kTextWidth, 0);
  textRowsFrom = kTextHeight;
  textRowsTo = 0;
  for (const Overlay::Text &text : overlay.texts()) {
    const std::uint16_t colour = DisplayList::rgb15(text.colour);
    auto found = std::ranges::find(colours, colour);
    if (found == colours.end()) {
      if (colours.size() == 255)
        continue;
      colours.push_back(colour);
      found = colours.end() - 1;
    }
    const auto entry = static_cast<std::uint8_t>(found - colours.begin() + 1);
    // stb_easy_font's letters (OverlayText.hpp, as the 3DS and desktops
    // draw them), each rectangle filled to whole pixels, at least
    // one each way. Placed in 16.16 fixed point, three floats a text rather
    // than a dozen a stroke (no floating point hardware here:
    // the text took up to 40 ms a frame)
    fontRectangles(text.text, textStrokes);
    constexpr int kOne = 1 << 16;
    const auto fixed = [](float value) { return static_cast<int>(std::lround(value * kOne)); };
    const int left = fixed(text.x), top = fixed(text.y), scale = fixed(text.scale);
    const auto pixel = [](int at) { return (at + kOne / 2) >> 16; }; // (rounded)
    for (const FontRectangle &r : textStrokes) {
      const int x0 = pixel(left + r.x * scale), y0 = pixel(top + r.y * scale);
      const int x1 = std::max(x0 + 1, pixel(left + (r.x + r.width) * scale));
      const int y1 = std::max(y0 + 1, pixel(top + (r.y + r.height) * scale));
      // (cut to the text's clip, and the layer)
      const int fromY = std::max({0, y0, static_cast<int>(std::ceil(text.clip.top))});
      const int toY = std::min({y1, kTextHeight, static_cast<int>(std::floor(text.clip.bottom))});
      const int fromX = std::max({0, x0, static_cast<int>(std::ceil(text.clip.left))});
      const int toX = std::min({x1, kTextWidth, static_cast<int>(std::floor(text.clip.right))});
      if (toY <= fromY || toX <= fromX)
        continue;
      textRowsFrom = std::min(textRowsFrom, fromY);
      textRowsTo = std::max(textRowsTo, toY);
      for (int y = fromY; y < toY; ++y) {
        std::uint8_t *line = &textPixels[static_cast<std::size_t>(y) * kTextWidth];
        for (int x = fromX; x < toX; ++x)
          line[x] = entry;
      }
    }
  }
  textShown = overlay.texts();
  textWaiting = true;
}

void Renderer::Backend::showText() {
  if (!textWaiting)
    return;
  textWaiting = false;
  std::uint16_t *palette = display.textPalette();
  for (std::size_t i = 0; i < textColours.size(); ++i)
    palette[i + 1] = textColours[i];
  // Only the rows the text covers now or did before: a few lines, so the
  // copy is done in the vertical blank rather than torn across the picture
  const int from = std::min(textRowsFrom, shownRowsFrom);
  const int to = std::max(textRowsTo, shownRowsTo);
  shownRowsFrom = textRowsFrom;
  shownRowsTo = textRowsTo;
  if (from >= to)
    return;
  const std::size_t start = static_cast<std::size_t>(from) * kTextWidth;
  const std::size_t bytes = static_cast<std::size_t>(to - from) * kTextWidth;
  DC_FlushRange(textPixels.data() + start, bytes);
  dmaCopy(textPixels.data() + start, display.textPixels() + start / 2, bytes);
}

Renderer::Renderer(Display &display) : backend_(std::make_unique<Backend>(display)) {
  backend_->textPixels.assign(kTextWidth * kTextHeight, 0);
  glInit();
  glEnable(GL_ANTIALIAS);
  glEnable(GL_BLEND);
  glClearPolyID(kClearId);
  // The textures' banks (B and A), and texture mapping on (a draw without
  // one sets none)
  texturesToGpu();
  glEnable(GL_TEXTURE_2D);
  glClearDepth(GL_MAX_DEPTH);
  glViewport(0, 0, 255, 191);
}

Renderer::~Renderer() = default;

Mesh Renderer::load(const MeshData &mesh) {
  Backend::LoadedMesh loaded;
  loaded.loaded = true;
  loaded.translucent = hasTranslucentVertices(mesh);
  loaded.source = DisplayList::prepare(mesh);
  loaded.bounds = boundsOf(mesh);
  loaded.doubleSided = mesh.doubleSided;
  backend_->meshes.push_back(std::move(loaded));
  return Mesh{static_cast<std::uint32_t>(backend_->meshes.size())};
}

Texture Renderer::load(const TextureImage &image) {
  const int widthCode = sideCode(image.width), heightCode = sideCode(image.height);
  const bool blank = image.pixels.empty();
  if (widthCode < 0 || heightCode < 0 ||
      (!blank && image.pixels.size() < static_cast<std::size_t>(image.width * image.height)))
    return {};
  const std::optional<std::uint32_t> at = backend_->textures.takeImage(image.width, image.height);
  if (!at)
    return {}; // (the banks are full)
  // Direct colour: 15 bits, the top bit opaque (more than half alpha); a
  // blank one see-through
  texturesToCpu();
  std::uint16_t *to = vramAt(*at);
  for (std::size_t i = 0; i < static_cast<std::size_t>(image.width * image.height); ++i) {
    if (blank) {
      to[i] = 0;
      continue;
    }
    const Colour c = image.pixels[i];
    to[i] = static_cast<std::uint16_t>(DisplayList::rgb15(c) | (c.a >= 128 ? 0x8000 : 0));
  }
  texturesToGpu();
  backend_->images.push_back({textureParameter(*at, static_cast<std::uint32_t>(widthCode),
                                               static_cast<std::uint32_t>(heightCode)),
                              image.width, image.height});
  return {static_cast<std::uint32_t>(backend_->images.size())};
}

bool Renderer::replace(Texture texture, const TextureImage &image, int firstRow) {
  if (!texture || texture.id > backend_->images.size())
    return false;
  const auto &loaded = backend_->images[texture.id - 1];
  if (loaded.parameter == 0 || loaded.width != image.width || firstRow < 0 ||
      firstRow + image.height > loaded.height ||
      image.pixels.size() < static_cast<std::size_t>(image.width * image.height))
    return false;
  // (where it is in the bank: the parameter's offset, in 8-byte steps; the
  // band from its first row, two bytes a pixel)
  const std::uint32_t offset =
      (loaded.parameter & 0xFFFF) * 8 + static_cast<std::uint32_t>(firstRow * image.width * 2);
  // (converted first, then copied in between frames: the 3D hardware is
  // without its textures only for the copy)
  std::vector<std::uint16_t> texels(static_cast<std::size_t>(image.width * image.height));
  for (std::size_t i = 0; i < texels.size(); ++i) {
    const Colour c = image.pixels[i];
    texels[i] = static_cast<std::uint16_t>(DisplayList::rgb15(c) | (c.a >= 128 ? 0x8000 : 0));
  }
  waitForTheBlank();
  texturesToCpu();
  std::copy(texels.begin(), texels.end(), vramAt(offset));
  texturesToGpu();
  return true;
}

// (Its bank space isn't given back: the bank is handed out in turn)
void Renderer::unload(Texture texture) {
  if (texture && texture.id <= backend_->images.size())
    backend_->images[texture.id - 1].parameter = 0;
}

void Renderer::unload(Mesh mesh) {
  if (mesh && mesh.id <= backend_->meshes.size())
    backend_->meshes[mesh.id - 1] = {};
}

void Renderer::beginFrame(const Camera &camera, const Environment &environment) {
  Backend &b = *backend_;
  b.began = std::chrono::steady_clock::now();
  b.ambient = environment.ambient;
  b.lights = environment.lights;
  b.lightCount = environment.lightCount;
  b.nextTranslucentId = kFirstTranslucentId;
  b.later.clear();
  overlay_.clear();
  const Colour &background = environment.background;
  glClearColor(background.r >> 3, background.g >> 3, background.b >> 3, 31);

  b.useCamera(camera, {0, 0, b.display.width(), b.display.height()});
  b.setFog(environment.fog);

  // Toon shading's table, and how it's used (ToonTable.hpp): one for the frame
  b.toonTable = environment.dsToonTable;
  if (b.toonTable == DsToonTable::Highlights)
    glEnable(GL_TOON_HIGHLIGHT);
  else
    glDisable(GL_TOON_HIGHLIGHT);
  const auto table = DisplayList::toonTable(environment.dsToonTable, environment.toonBands);
  if (table != b.toonTableSet) {
    glSetToonTable(table.data());
    b.toonTableSet = table;
  }
  // Outlines' colour (edge marking's eight), and none yet this frame
  for (int i = 0; i < 8; ++i)
    glSetOutlineColor(i, DisplayList::rgb15(environment.outline.colour));
  b.outlining = false;
  b.nextOutlineId = kFirstOutlineId;
}

void Renderer::draw(Mesh mesh, const Transform &where, const Material &material) {
  Backend &b = *backend_;
  // Wholly out of view: not drawn, its vertices not sent (Culling.hpp)
  if (mesh && mesh.id <= b.meshes.size() && b.frustum &&
      b.frustum->outside(b.meshes[mesh.id - 1].bounds, where)) {
    ++b.culled;
    return;
  }
  const bool translucentMesh =
      mesh && mesh.id <= b.meshes.size() && b.meshes[mesh.id - 1].translucent;
  if ((material.alpha < 31 || translucentMesh) && !material.wireframe) {
    b.later.push_back({mesh, where, material, (where.position() - b.camera.eye).length()});
    return;
  }
  // An outlined mesh takes its own polygon ID, so edge marking rings it
  unsigned id = 0;
  if (material.outline && !material.wireframe) {
    id = b.nextOutlineId;
    b.nextOutlineId = id == kLastOutlineId ? kFirstOutlineId : id + 1;
    b.outlining = true;
  }
  b.drawNow(mesh, where, material, id);
}

void Renderer::drawLines(std::span<const Vector3> ends, Colour colour) {
  if (ends.size() < 2)
    return;
  ++backend_->draws;
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
  GFX_TEX_FORMAT = 0; // (untextured)
  glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0) | backend_->fogBit);
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

void Renderer::Backend::drawLater() {
  // Translucent meshes, farthest first (drawn in this order: the frame is
  // flushed with manual sorting), each with its own polygon ID so they
  // blend over each other
  std::ranges::stable_sort(later,
                           [](const Later &x, const Later &y) { return x.distance > y.distance; });
  // (one count for the frame, meshes then the overlay's shapes: a
  // translucent polygon isn't drawn over a translucent one with the same
  // ID, so the overlay's mustn't reuse the meshes' IDs, the roll's
  // hexagon missing two triangles over the dice's glass)
  for (const Later &item : later) {
    unsigned &id = nextTranslucentId;
    drawNow(item.mesh, item.where, item.material, id);
    id = id == kLastTranslucentId ? kFirstTranslucentId : id + 1;
  }
  later.clear();
}

void Renderer::Backend::useCamera(const Camera &seen, PixelRect rect) {
  camera = seen;
  view = rect;
  frustum.emplace(seen, Scalar(rect.width) / Scalar(rect.height > 0 ? rect.height : 1));
  // (the hardware's viewport: corners inclusive, y up from the bottom)
  const int bottom = display.height() - 1;
  glViewport(rect.x, bottom - (rect.y + rect.height - 1), rect.x + rect.width - 1, bottom - rect.y);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  {
    HardwareMathsLock lock; // these use the divider and square root unit
    glMatrixMode(GL_PROJECTION);
    // The field of view in libnds' angles (32768 a turn)
    const int angle = static_cast<int>(static_cast<std::int64_t>(f32(seen.fieldOfView)) *
                                       DEGREES_IN_CIRCLE / (360 * 4096));
    gluPerspectivef32(angle, inttof32(rect.width) / (rect.height > 0 ? rect.height : 1),
                      f32(seen.nearPlane), f32(seen.farPlane));
    glMatrixMode(GL_MODELVIEW);
    gluLookAtf32(f32(seen.eye.x), f32(seen.eye.y), f32(seen.eye.z), f32(seen.target.x),
                 f32(seen.target.y), f32(seen.target.z), f32(seen.up.x), f32(seen.up.y),
                 f32(seen.up.z));
  }
  setLights();
}

void Renderer::Backend::setLights() {
  // Set after the view: the DS turns a light's direction by the view as
  // it's set, so they're given in the world
  lightBits = 0;
  for (unsigned i = 0; i < lightCount; ++i) {
    const Light &light = lights[i];
    const Vector3 direction = light.direction.normalised();
    glLight(static_cast<int>(i), DisplayList::rgb15(light.colour), v10Of(direction.x),
            v10Of(direction.y), v10Of(direction.z));
    lightBits |= POLY_FORMAT_LIGHT0 << i;
  }
}

void Renderer::beginView(const Camera &camera, const Viewport &viewport) {
  Backend &b = *backend_;
  b.drawLater();
  b.useCamera(camera, pixelsOf(viewport, b.display.width(), b.display.height()));
}

void Renderer::endFrame() {
  Backend &b = *backend_;
  b.drawLater();
  glViewport(0, 0, 255, 191); // (the overlay: the whole screen again)
  using Clock = std::chrono::steady_clock;
  const auto milliseconds = [](Clock::duration d) {
    return std::chrono::duration<float, std::milli>(d).count();
  };
  const Clock::time_point overlayFrom = Clock::now();
  b.drawOverlayShapes(overlay_);
  b.drawOverlayText(overlay_);
  overlay_.clear();
  const Clock::time_point overlayTo = Clock::now();
  // Edge marking only in frames with outlines (it also misbehaves with
  // translucent polygons that set depth, as ours do: GBATEK), and then
  // without anti-aliasing, which would draw the outlines half see-through
  if (b.outlining) {
    glClearPolyID(kClearIdWithOutlines);
    glEnable(GL_OUTLINE);
    glDisable(GL_ANTIALIAS);
  } else {
    glClearPolyID(kClearId);
    glDisable(GL_OUTLINE);
    glEnable(GL_ANTIALIAS);
  }

  // What the frame used of the polygon and vertex RAM, once the geometry
  // engine has taken in all of it
  while (GFX_BUSY) {
  }
  b.last = {static_cast<unsigned>(GFX_POLYGON_RAM_USAGE),
            static_cast<unsigned>(GFX_VERTEX_RAM_USAGE), std::exchange(b.draws, 0),
            std::exchange(b.culled, 0)};
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
  const Clock::time_point flushed = Clock::now();
  swiWaitForVBlank();
  // (the frame's 3D shows from this refresh: its text with it)
  const Clock::time_point refreshed = Clock::now();
  b.showText();
  const Clock::duration shown = Clock::now() - refreshed;
  b.times = {.gpu = 0,
             .cpu = milliseconds(flushed - b.began + shown),
             .overlay = milliseconds(overlayTo - overlayFrom + shown)};
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
  // (in the current view)
  const PixelRect &view = backend_->view;
  const float width = static_cast<float>(view.width), height = static_cast<float>(view.height);
  const float focal = 1 / std::tan(static_cast<float>(camera.fieldOfView) * 3.14159265f / 360);
  const float ndcX = focal * height / width * viewX / depth, ndcY = focal * viewY / depth;
  x = static_cast<float>(view.x) + (ndcX * 0.5f + 0.5f) * width;
  y = static_cast<float>(view.y) + (0.5f - ndcY * 0.5f) * height;
  return true;
}

Capabilities Renderer::capabilities() const {
  return {.lights = Environment::kMostLights, .polygonsPerFrame = kMostPolygons};
}

std::optional<TextureMemory> Renderer::textureMemory() const { return backend_->textures; }

void Renderer::setTextureBudget(const TextureMemory &) {} // (the DS has its own)

FrameCounts Renderer::lastFrame() const { return backend_->last; }

FrameTimes Renderer::lastFrameTimes() const { return backend_->times; }

// (Overlay::textWidth and lineHeight: stb_easy_font's, in OverlayText.cpp)

} // namespace ScratchPadd::Scene
