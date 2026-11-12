// How many triangles a 3DS's GPU (the PICA200) draws a frame, with citro3d,
// for the scene layer's 3DS budgets (see ../README.md). Each case draws
// objects of 128 triangles, each with its own transform (a draw and its
// uniforms, as a scene draws meshes), doubling how many until a frame takes
// over 40 ms, and works out from the GPU's and the CPU's times how many
// triangles fit in a frame at 60 and at 30 frames a second.
//
// The cases: unlit (vertex colours) or lit (the GPU's fragment lighting:
// ambient and 4 directional lights, the scene layer's most), small
// triangles (about 4 pixels: vertex-bound) or large (about 450: fill-bound),
// mono or stereo (the top screen drawn for each eye), and on a New 3DS at
// 268 MHz and with its 804 MHz mode. The results show on the bottom screen
// and are written to sdmc:/scene-3ds-benchmark.txt, each case's every
// measurement too. START quits.
#include <3ds.h>
#include <citro3d.h>

#include "lit_shbin.h"
#include "unlit_shbin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr int kGrid = 8;                          // an object: 8 x 8 quads
constexpr int kTrianglesEach = kGrid * kGrid * 2; // 128
constexpr int kVerticesEach = kTrianglesEach * 3; // (not indexed)
constexpr int kMostObjects = 4096;                // 524,288 triangles
constexpr float kSlowestFrame = 40;               // ms: doubling stops past it
constexpr int kWarmUpFrames = 3, kMeasuredFrames = 12;
constexpr std::array<float, 2> kBudgets = {1000.0f / 60, 1000.0f / 30};
constexpr const char *kResultsPath = "sdmc:/scene-3ds-benchmark.txt";

constexpr std::uint32_t kDisplayTransferFlags =
    GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

struct Vertex {
  float position[3];
  float normal[3];
  std::uint8_t colour[4];
};

struct Case {
  bool lit, large, stereo, fast;
  std::string name() const {
    char text[40];
    std::snprintf(text, sizeof text, "%-5s %-5s %-6s %3d", lit ? "lit" : "unlit",
                  large ? "large" : "small", stereo ? "stereo" : "mono", fast ? 804 : 268);
    return text;
  }
};

// One measurement: `objects` drawn each frame, and the frame's GPU and CPU
// time (ms, averaged)
struct Sample {
  int objects = 0;
  float gpu = 0, cpu = 0;
  float slowest() const { return std::max(gpu, cpu); }
};

// A small, repeatable random number generator
struct Random {
  std::uint32_t state = 12345;
  float next() { // 0 to 1
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
  }
};

// A program (one of the vertex shaders) and its uniforms' locations
struct Program {
  DVLB_s *dvlb = nullptr;
  shaderProgram_s program;
  int projection = -1, modelView = -1;

  void load(const std::uint8_t *shbin, std::uint32_t size) {
    dvlb = DVLB_ParseFile(const_cast<u32 *>(reinterpret_cast<const u32 *>(shbin)), size);
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &dvlb->DVLE[0]);
    projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
    modelView = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
  }
};

class Benchmark {
public:
  Benchmark() {
    C3D_Init(0x200000); // room for kMostObjects draws, twice (stereo)
    left_ = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    right_ = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(left_, GFX_TOP, GFX_LEFT, kDisplayTransferFlags);
    C3D_RenderTargetSetOutput(right_, GFX_TOP, GFX_RIGHT, kDisplayTransferFlags);
    unlit_.load(unlit_shbin, unlit_shbin_size);
    lit_.load(lit_shbin, lit_shbin_size);
    makeObject();
    placeObjects();
    setUpLights();
    // Pixels in a 400 x 240 screen, and depths from 0 to 1 away
    Mtx_OrthoTilt(&projection_, 0, 400, 0, 240, 0, 1, false);
  }

  // Measures `which`, drawing on the top screen; false if START (or the
  // system) stopped it
  bool measure(const Case &which, std::vector<Sample> &samples) {
    use(which);
    for (int objects = 1; objects <= kMostObjects; objects *= 2) {
      Sample sample{.objects = objects};
      for (int frame = 0; frame < kWarmUpFrames + kMeasuredFrames; ++frame) {
        if (!aptMainLoop())
          return false;
        hidScanInput();
        if (hidKeysDown() & KEY_START)
          return false;
        drawFrame(which, objects);
        if (frame >= kWarmUpFrames) {
          sample.gpu += C3D_GetDrawingTime() / kMeasuredFrames;
          sample.cpu += C3D_GetProcessingTime() / kMeasuredFrames;
        }
      }
      samples.push_back(sample);
      if (sample.slowest() > kSlowestFrame)
        break;
    }
    return true;
  }

private:
  // The object every draw draws: an 8 x 8 grid of quads, 1 unit across,
  // facing +z, its normals tilted a little each way and its colours varied
  void makeObject() {
    vertices_ = static_cast<Vertex *>(linearAlloc(sizeof(Vertex) * kVerticesEach));
    Random random;
    Vertex *out = vertices_;
    const auto corner = [&](int x, int y) {
      Vertex v{};
      v.position[0] = static_cast<float>(x) / kGrid - 0.5f;
      v.position[1] = static_cast<float>(y) / kGrid - 0.5f;
      v.normal[0] = random.next() * 0.6f - 0.3f;
      v.normal[1] = random.next() * 0.6f - 0.3f;
      v.normal[2] = 1;
      v.colour[0] = static_cast<std::uint8_t>(128 + x * 15);
      v.colour[1] = static_cast<std::uint8_t>(128 + y * 15);
      v.colour[2] = 200;
      v.colour[3] = 255;
      return v;
    };
    for (int y = 0; y < kGrid; ++y) {
      for (int x = 0; x < kGrid; ++x) {
        const Vertex a = corner(x, y), b = corner(x + 1, y), c = corner(x + 1, y + 1),
                     d = corner(x, y + 1);
        for (const Vertex &v : {a, b, c, a, c, d})
          *out++ = v;
      }
    }
    C3D_BufInfo *buffers = C3D_GetBufInfo();
    BufInfo_Init(buffers);
    BufInfo_Add(buffers, vertices_, sizeof(Vertex), 3, 0x210);
    C3D_AttrInfo *attributes = C3D_GetAttrInfo();
    AttrInfo_Init(attributes);
    AttrInfo_AddLoader(attributes, 0, GPU_FLOAT, 3);         // v0: position
    AttrInfo_AddLoader(attributes, 1, GPU_FLOAT, 3);         // v1: normal
    AttrInfo_AddLoader(attributes, 2, GPU_UNSIGNED_BYTE, 4); // v2: colour
  }

  // Where each object goes: anywhere on the screen, turned any way, at its
  // own depth; scaled when drawn (by the case's size)
  void placeObjects() {
    Random random;
    places_.resize(kMostObjects);
    for (Place &place : places_) {
      place.x = random.next() * 400;
      place.y = random.next() * 240;
      place.depth = 0.05f + random.next() * 0.9f;
      place.turn = random.next() * 6.2831853f;
    }
  }

  // Ambient light and 4 directional lights (the scene layer's most), for
  // the lit case, with the vertex colours as the surface's
  void setUpLights() {
    static const C3D_Material material = {
        {0.25f, 0.25f, 0.25f}, // ambient
        {0.75f, 0.75f, 0.75f}, // diffuse
        {0, 0, 0},             // specular0
        {0, 0, 0},             // specular1
        {0, 0, 0},             // emission
    };
    C3D_LightEnvInit(&lightEnv_);
    C3D_LightEnvMaterial(&lightEnv_, &material);
    const float directions[4][3] = {{0, 0, 1}, {1, 1, 1}, {-1, 0.5f, 1}, {0, -1, 1}};
    for (int i = 0; i < 4; ++i) {
      C3D_LightInit(&lights_[i], &lightEnv_);
      C3D_FVec direction = FVec4_New(directions[i][0], directions[i][1], directions[i][2], 0);
      C3D_LightPosition(&lights_[i], &direction); // (w 0: directional)
      C3D_LightColor(&lights_[i], 0.5f, 0.5f, 0.5f);
    }
  }

  // The case's program, lighting and texture combiner, 3D and CPU speed
  void use(const Case &which) {
    program_ = which.lit ? &lit_ : &unlit_;
    C3D_BindProgram(&program_->program);
    C3D_LightEnvBind(which.lit ? &lightEnv_ : nullptr);
    C3D_TexEnv *combiner = C3D_GetTexEnv(0);
    C3D_TexEnvInit(combiner);
    if (which.lit) {
      // The vertex colour times the lighting
      C3D_TexEnvSrc(combiner, C3D_Both, GPU_PRIMARY_COLOR, GPU_FRAGMENT_PRIMARY_COLOR,
                    GPU_PRIMARY_COLOR);
      C3D_TexEnvFunc(combiner, C3D_Both, GPU_MODULATE);
    } else {
      C3D_TexEnvSrc(combiner, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
      C3D_TexEnvFunc(combiner, C3D_Both, GPU_REPLACE);
    }
    C3D_CullFace(GPU_CULL_NONE);
    // Depth tested and written, as a scene's are (cleared to 0; nearer is
    // greater)
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    gfxSet3D(which.stereo);
    osSetSpeedupEnable(which.fast);
  }

  // A frame: `objects` objects on each eye's target
  void drawFrame(const Case &which, int objects) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    drawOn(left_, which, objects);
    if (which.stereo)
      drawOn(right_, which, objects);
    C3D_FrameEnd(0);
  }

  void drawOn(C3D_RenderTarget *target, const Case &which, int objects) {
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x202830FF, 0);
    C3D_FrameDrawOn(target);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, program_->projection, &projection_);
    // About 4 pixels a triangle small (24 pixels across an object), about
    // 450 large (240 across)
    const float size = which.large ? 240 : 24;
    for (int i = 0; i < objects; ++i) {
      const Place &place = places_[static_cast<std::size_t>(i)];
      C3D_Mtx modelView;
      Mtx_Identity(&modelView);
      Mtx_Translate(&modelView, place.x, place.y, -place.depth, true);
      Mtx_RotateZ(&modelView, place.turn, true);
      Mtx_Scale(&modelView, size, size, 1);
      C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, program_->modelView, &modelView);
      C3D_DrawArrays(GPU_TRIANGLES, 0, kVerticesEach);
    }
  }

  struct Place {
    float x, y, depth, turn;
  };

  C3D_RenderTarget *left_ = nullptr, *right_ = nullptr;
  Program unlit_, lit_;
  Program *program_ = &unlit_;
  Vertex *vertices_ = nullptr;
  std::vector<Place> places_;
  C3D_Mtx projection_;
  C3D_LightEnv lightEnv_;
  std::array<C3D_Light, 4> lights_;
};

// The triangles a frame that fit in `budget` ms, between the samples on
// either side of it (each the slower of the GPU and CPU); -1 if even one
// object is slower, 0 if the most objects are faster
int trianglesWithin(const std::vector<Sample> &samples, float budget, const char *&limit) {
  limit = "";
  for (std::size_t i = 0; i < samples.size(); ++i) {
    if (samples[i].slowest() <= budget)
      continue;
    limit = samples[i].gpu >= samples[i].cpu ? "gpu" : "cpu";
    if (i == 0)
      return -1;
    const Sample &below = samples[i - 1], &above = samples[i];
    const float fraction = (budget - below.slowest()) / (above.slowest() - below.slowest());
    return static_cast<int>(
        (below.objects + fraction * static_cast<float>(above.objects - below.objects)) *
        kTrianglesEach);
  }
  return 0;
}

// A number of triangles, as the results show it (in thousands)
std::string thousands(int triangles) {
  char text[16];
  if (triangles < 0)
    return "<0.1k";
  if (triangles == 0)
    std::snprintf(text, sizeof text, ">%dk", kMostObjects * kTrianglesEach / 1000);
  else
    std::snprintf(text, sizeof text, "%.1fk", triangles / 1000.0f);
  return text;
}

} // namespace

int main() {
  gfxInitDefault();
  PrintConsole bottom;
  consoleInit(GFX_BOTTOM, &bottom);
  bool newThreeDs = false;
  APT_CheckNew3DS(&newThreeDs);

  std::vector<Case> cases;
  for (const bool fast : {false, true}) {
    if (fast && !newThreeDs)
      continue; // (only a New 3DS has the 804 MHz mode)
    for (const bool lit : {false, true})
      for (const bool large : {false, true})
        for (const bool stereo : {false, true})
          cases.push_back({.lit = lit, .large = large, .stereo = stereo, .fast = fast});
  }

  std::printf("\x1b[1mScene 3DS benchmark\x1b[0m (%s 3DS)\n", newThreeDs ? "New" : "Old");
  std::printf("Triangles a frame at 60 and 30 fps\n\n");
  std::printf("case               MHz   60fps 30fps\n");
  std::FILE *results = std::fopen(kResultsPath, "w");
  if (results)
    std::fprintf(results, "Scene 3DS benchmark, on a%s 3DS: %d triangles an object\n\n",
                 newThreeDs ? " New" : "n Old", kTrianglesEach);

  Benchmark benchmark;
  bool finished = true;
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const Case &which = cases[index];
    std::printf("\x1b[30;1H%-38s", (which.name() + " ...").c_str());
    std::vector<Sample> samples;
    if (!benchmark.measure(which, samples)) {
      finished = false;
      break;
    }
    const char *limit60 = "", *limit30 = "";
    const int at60 = trianglesWithin(samples, kBudgets[0], limit60);
    const int at30 = trianglesWithin(samples, kBudgets[1], limit30);
    std::printf("\x1b[30;1H\x1b[K"); // (the progress line cleared)
    std::printf("\x1b[%d;1H%s %6s %5s %s", static_cast<int>(5 + index), which.name().c_str(),
                thousands(at60).c_str(), thousands(at30).c_str(), limit60);
    if (results) {
      std::fprintf(results, "%s: 60 fps %s, 30 fps %s (limited by the %s)\n", which.name().c_str(),
                   thousands(at60).c_str(), thousands(at30).c_str(), limit60);
      for (const Sample &sample : samples)
        std::fprintf(results, "  %7d triangles: GPU %6.2f ms, CPU %6.2f ms\n",
                     sample.objects * kTrianglesEach, sample.gpu, sample.cpu);
      std::fflush(results);
    }
  }
  if (results)
    std::fclose(results);
  osSetSpeedupEnable(false);
  std::printf("\x1b[29;1H%s",
              finished ? "Done: results on SD card. START quits" : "Stopped. START quits.");
  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START)
      break;
    gspWaitForVBlank();
  }
  C3D_Fini();
  gfxExit();
  return 0;
}
