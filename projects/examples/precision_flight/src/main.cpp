// Precision Flight: a prototype rail shooter in the spirit of SNES Star Fox
// where the numeric precision of the game's subsystems is a resource.
//
// Precision groups (flexmath FlexGroups, owned by a FlexController):
//   Flight - grid the ship's lateral position snaps to as physics sees it
//   Hull   - how finely collisions sample the ship's position along the rail
//   Render - how finely vertex positions resolve when drawing
//
// Powerups change a group's precision for a few seconds:
//   FOCUS (cyan)    Flight 1 -> 5 bits. Gold gates are centred between the
//                   default flight grid points, so only FOCUS can thread them.
//   PHASE (magenta) Hull 2 -> -1 bits. Collisions sample every 2 units, so
//                   thin energy walls slip between samples. Render drops to
//                   3 bits while phased: the world visibly glitches.

// GLEW must be included before GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <random>
#include <string>
#include <vector>

#include <FlexController.hpp>

using Flex::Math::FlexController;
using Flex::Math::FlexGroup;
using Flex::Math::FlexOverride;
using Flex::Math::FlexPoint;
using Flex::Math::Policy;
using Flex::Math::Precision;
using Flex::Math::Rounding;

namespace {

constexpr float kLaneHalfWidth = 6.0f;
constexpr float kLaneMinY = -3.0f;
constexpr float kLaneMaxY = 3.5f;
constexpr float kGroundY = -4.5f;
constexpr float kCeilingY = 5.5f;
constexpr float kShipHalfX = 0.55f;
constexpr float kShipHalfY = 0.25f;
constexpr float kShipHalfZ = 0.3f;
constexpr float kWallHalfZ = 0.1f;
constexpr float kGateBar = 0.3f;
constexpr double kStepSeconds = 1.0 / 60.0;
const glm::vec3 kSkyColor(0.02f, 0.03f, 0.08f);

// ---------------------------------------------------------------------------
// Rendering

const char *kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform float uWorldSnap;  // world-space grid vertices snap to (0 = off)
uniform float uScreenSnap; // pixel grid vertices snap to (0 = off)
uniform vec2 uViewport;

out vec3 vNormal;
out float vDepth;

void main() {
  vec4 world = uModel * vec4(aPos, 1.0);
  if (uWorldSnap > 0.0) {
    world.xyz = floor(world.xyz / uWorldSnap + 0.5) * uWorldSnap;
  }
  vec4 clip = uViewProj * world;
  if (uScreenSnap > 0.0 && clip.w > 0.0) {
    vec2 pixel = (clip.xy / clip.w * 0.5 + 0.5) * uViewport;
    pixel = floor(pixel / uScreenSnap + 0.5) * uScreenSnap;
    clip.xy = (pixel / uViewport * 2.0 - 1.0) * clip.w;
  }
  gl_Position = clip;
  vNormal = mat3(uModel) * aNormal;
  vDepth = clip.w;
}
)";

const char *kFragmentShader = R"(#version 330 core
in vec3 vNormal;
in float vDepth;

uniform vec3 uColor;
uniform float uAlpha;
uniform bool uLit;
uniform vec3 uFogColor;
uniform float uFogDensity;

out vec4 FragColor;

void main() {
  vec3 color = uColor;
  if (uLit) {
    vec3 n = normalize(vNormal);
    float diffuse = abs(dot(n, normalize(vec3(0.4, 0.8, 0.3))));
    color *= 0.35 + 0.65 * diffuse;
  }
  float fog = clamp(1.0 - exp(-uFogDensity * vDepth), 0.0, 1.0);
  FragColor = vec4(mix(color, uFogColor, fog), uAlpha);
}
)";

GLuint compileShader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
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

GLuint linkProgram(const char *vertexSource, const char *fragmentSource) {
  GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
  GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::fprintf(stderr, "Program link error: %s\n", log);
  }
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  return program;
}

// Interleaved position (3 floats) + normal (3 floats)
struct Mesh {
  GLuint vao = 0;
  GLuint vbo = 0;
  GLsizei count = 0;
  GLenum mode = GL_TRIANGLES;
};

void uploadMesh(Mesh &mesh, const std::vector<float> &data) {
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
  mesh.count = static_cast<GLsizei>(data.size() / 6);
}

Mesh makeMesh(const std::vector<float> &data, GLenum mode) {
  Mesh mesh;
  mesh.mode = mode;
  glGenVertexArrays(1, &mesh.vao);
  glGenBuffers(1, &mesh.vbo);
  glBindVertexArray(mesh.vao);
  uploadMesh(mesh, data);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
  return mesh;
}

void pushVertex(std::vector<float> &data, glm::vec3 p, glm::vec3 n) {
  data.insert(data.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
}

void pushTriangle(std::vector<float> &data, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
  glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
  pushVertex(data, a, n);
  pushVertex(data, b, n);
  pushVertex(data, c, n);
}

void pushQuad(std::vector<float> &data, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
  pushTriangle(data, a, b, c);
  pushTriangle(data, a, c, d);
}

// Unit cube centred on the origin
std::vector<float> cubeData() {
  std::vector<float> d;
  const float h = 0.5f;
  glm::vec3 p[8] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
                    {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
  pushQuad(d, p[4], p[5], p[6], p[7]); // +z
  pushQuad(d, p[1], p[0], p[3], p[2]); // -z
  pushQuad(d, p[5], p[1], p[2], p[6]); // +x
  pushQuad(d, p[0], p[4], p[7], p[3]); // -x
  pushQuad(d, p[7], p[6], p[2], p[3]); // +y
  pushQuad(d, p[0], p[1], p[5], p[4]); // -y
  return d;
}

// Low-poly fighter facing -z
std::vector<float> shipData() {
  std::vector<float> d;
  glm::vec3 nose(0.0f, 0.0f, -1.4f), top(0.0f, 0.28f, 0.3f), bottom(0.0f, -0.18f, 0.4f),
      tail(0.0f, 0.05f, 0.65f), left(-1.3f, -0.1f, 0.5f), right(1.3f, -0.1f, 0.5f),
      finLeft(-0.55f, 0.65f, 0.7f), finRight(0.55f, 0.65f, 0.7f);
  pushTriangle(d, nose, left, top);
  pushTriangle(d, nose, top, right);
  pushTriangle(d, top, left, tail);
  pushTriangle(d, top, tail, right);
  pushTriangle(d, nose, bottom, left);
  pushTriangle(d, nose, right, bottom);
  pushTriangle(d, left, bottom, tail);
  pushTriangle(d, bottom, right, tail);
  pushTriangle(d, top, tail, finLeft);
  pushTriangle(d, top, finRight, tail);
  return d;
}

std::vector<float> octahedronData() {
  std::vector<float> d;
  glm::vec3 up(0, 1, 0), down(0, -1, 0);
  glm::vec3 ring[4] = {{1, 0, 0}, {0, 0, 1}, {-1, 0, 0}, {0, 0, -1}};
  for (int i = 0; i < 4; ++i) {
    glm::vec3 a = ring[i], b = ring[(i + 1) % 4];
    pushTriangle(d, up, b, a);
    pushTriangle(d, down, a, b);
  }
  return d;
}

struct Renderer {
  GLuint program = 0;
  GLint model, viewProj, worldSnap, screenSnap, viewport, color, alpha, lit, fogColor, fogDensity;
  Mesh cube, ship, octahedron, ground, posts;

  void init() {
    program = linkProgram(kVertexShader, kFragmentShader);
    model = glGetUniformLocation(program, "uModel");
    viewProj = glGetUniformLocation(program, "uViewProj");
    worldSnap = glGetUniformLocation(program, "uWorldSnap");
    screenSnap = glGetUniformLocation(program, "uScreenSnap");
    viewport = glGetUniformLocation(program, "uViewport");
    color = glGetUniformLocation(program, "uColor");
    alpha = glGetUniformLocation(program, "uAlpha");
    lit = glGetUniformLocation(program, "uLit");
    fogColor = glGetUniformLocation(program, "uFogColor");
    fogDensity = glGetUniformLocation(program, "uFogDensity");
    cube = makeMesh(cubeData(), GL_TRIANGLES);
    ship = makeMesh(shipData(), GL_TRIANGLES);
    octahedron = makeMesh(octahedronData(), GL_TRIANGLES);
    ground = makeMesh({}, GL_LINES);
    posts = makeMesh({}, GL_LINES);
  }

  // Render precision: fewer bits means coarser vertex snapping
  void setSnap(int bits) {
    float world = static_cast<float>(std::ldexp(8.0, -bits));
    float screen = static_cast<float>(std::ldexp(64.0, -bits));
    glUniform1f(worldSnap, world < 0.02f ? 0.0f : world);
    glUniform1f(screenSnap, screen < 1.0f ? 0.0f : screen);
  }

  void draw(const Mesh &mesh, const glm::mat4 &m, glm::vec3 c, bool isLit = true, float a = 1.0f) {
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(m));
    glUniform3fv(color, 1, glm::value_ptr(c));
    glUniform1f(alpha, a);
    glUniform1i(lit, isLit ? 1 : 0);
    glBindVertexArray(mesh.vao);
    glDrawArrays(mesh.mode, 0, mesh.count);
  }

  void drawBox(glm::vec3 center, glm::vec3 size, glm::vec3 c, bool isLit = true, float a = 1.0f) {
    draw(cube, glm::scale(glm::translate(glm::mat4(1.0f), center), size), c, isLit, a);
  }
};

// ---------------------------------------------------------------------------
// Game

enum class Kind { Gate, GoldGate, PhaseWall, FocusPickup, PhasePickup };

struct Obstacle {
  Kind kind;
  float z;
  float cx = 0.0f; // hole centre for gates, position for pickups
  float cy = 0.0f;
  float w = 0.0f; // hole size for gates
  float h = 0.0f;
  bool resolved = false; // scored, crashed, collected or left behind
  bool touched = false;  // the hull sampled a position overlapping it
  float flash = 0.0f;    // seconds of highlight after an event
  glm::vec3 flashColor{1.0f};
};

struct Effect {
  float remaining = 0.0f;
  float duration = 0.0f;
  bool active() const { return remaining > 0.0f; }
};

struct Message {
  std::string text;
  ImU32 color;
  float age = 0.0f;
};

struct Rect {
  float x0, y0, x1, y1;
  bool overlaps(const Rect &o) const { return x0 < o.x1 && o.x0 < x1 && y0 < o.y1 && o.y0 < y1; }
  bool contains(const Rect &o) const { return o.x0 >= x0 && o.x1 <= x1 && o.y0 >= y0 && o.y1 <= y1; }
};

struct Game {
  // Precision lab settings
  int flightBase = 1, hullBase = 2, renderBase = 9;
  int focusFlightBits = 5, phaseHullBits = -1, phaseRenderBits = 3;
  float focusSeconds = 7.0f, phaseSeconds = 4.0f;
  Rounding rounding = Rounding::TowardZero;
  bool quantizeVelocity = false; // lossy velocity: small drag steps vanish
  bool lossyPosition = false;    // Flight policy Lossy: small moves vanish
  bool showHitbox = true;
  bool paused = false;
  bool autopilot = false; // steer toward the next gate or pickup
  float speed = 25.0f;

  FlexController precision;
  FlexGroup &flight = precision.addGroup("Flight");
  FlexGroup &hull = precision.addGroup("Hull");
  FlexGroup &render = precision.addGroup("Render");
  FlexOverride focusFlight, phaseHull, phaseRender; // held while a powerup lasts
  Effect focus, phase;

  // Lateral physics state, stored at full Q32.32 precision
  FlexPoint x, y, vx, vy;
  double z = 0.0; // rail position; the ship flies toward -z

  glm::vec2 observed{0.0f}; // lateral position as physics sees it (Flight)
  float observedZ = 0.0f;   // rail position as collisions see it (Hull)
  glm::vec2 smoothed{0.0f}; // drawn position, eased toward `observed`

  int score = 0;
  float shield = 100.0f;
  bool over = false;
  int gates = 0, goldGates = 0, phasedWalls = 0;

  std::vector<Obstacle> obstacles;
  double nextSpawnZ = 0.0;
  std::mt19937 rng;
  std::deque<Message> messages;

  void reset() {
    x = y = vx = vy = FlexPoint();
    z = 0.0;
    observed = smoothed = glm::vec2(0.0f);
    observedZ = 0.0f;
    score = gates = goldGates = phasedWalls = 0;
    shield = 100.0f;
    over = false;
    focus = phase = Effect();
    obstacles.clear();
    messages.clear();
    nextSpawnZ = -40.0;
    rng.seed(7);
    updateGroups();
  }

  void say(const std::string &text, ImU32 color) {
    std::printf("[%7.1f] %s\n", -z, text.c_str());
    std::fflush(stdout);
    messages.push_back({text, color});
    if (messages.size() > 4) {
      messages.pop_front();
    }
  }

  void updateGroups() {
    auto bits = [&](int fracBits) { return Precision{.fracBits = fracBits, .rounding = rounding}; };
    flight.setBase(bits(flightBase));
    hull.setBase(bits(hullBase));
    render.setBase(bits(renderBase));
    flight.setPolicy(lossyPosition ? Policy::Lossy : Policy::View);
    // Re-pushed every step so lab changes apply mid-effect; assigning the new
    // handle releases the previous override
    focusFlight = focus.active() ? flight.push(bits(focusFlightBits)) : FlexOverride();
    phaseHull = phase.active() ? hull.push(bits(phaseHullBits)) : FlexOverride();
    phaseRender = phase.active() ? render.push(bits(phaseRenderBits)) : FlexOverride();
  }

  float random(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
  float randomOnGrid(float lo, float hi, float grid) { return std::round(random(lo, hi) / grid) * grid; }

  void spawnSegment() {
    const float z0 = static_cast<float>(nextSpawnZ);
    const float roll = random(0.0f, 1.0f);
    auto gate = [&](float gz) {
      Obstacle o{Kind::Gate, gz};
      o.w = 3.4f;
      o.h = 2.6f;
      o.cx = randomOnGrid(-kLaneHalfWidth + 2.0f, kLaneHalfWidth - 2.0f, 0.5f);
      o.cy = randomOnGrid(kLaneMinY + 1.0f, kLaneMaxY - 1.0f, 0.5f);
      obstacles.push_back(o);
    };
    if (roll < 0.35f) {
      gate(z0);
      nextSpawnZ -= 32.0;
    } else if (roll < 0.55f) {
      gate(z0);
      gate(z0 - 16.0f);
      nextSpawnZ -= 44.0;
    } else if (roll < 0.78f) {
      Obstacle pickup{Kind::FocusPickup, z0};
      pickup.cx = randomOnGrid(-4.0f, 4.0f, 0.5f);
      pickup.cy = randomOnGrid(-2.0f, 2.5f, 0.5f);
      obstacles.push_back(pickup);
      // Opening centred a quarter unit off the default 0.5 flight grid, with
      // 0.2 of slack: unreachable until FOCUS refines the grid.
      Obstacle gold{Kind::GoldGate, z0 - 45.0f};
      gold.cx = randomOnGrid(-4.0f, 4.0f, 0.5f) + 0.25f;
      gold.cy = randomOnGrid(-1.5f, 2.0f, 0.5f);
      gold.w = 2.0f * (kShipHalfX + 0.2f);
      gold.h = 2.0f * (kShipHalfY + 0.9f);
      obstacles.push_back(gold);
      nextSpawnZ -= 70.0;
    } else {
      Obstacle pickup{Kind::PhasePickup, z0};
      pickup.cx = randomOnGrid(-4.0f, 4.0f, 0.5f);
      pickup.cy = randomOnGrid(-2.0f, 2.5f, 0.5f);
      obstacles.push_back(pickup);
      // Walls sit on odd z so 2-unit hull sampling always steps over them
      float wz = z0 - 40.0f;
      float k = std::round((-wz - 1.0f) / 2.0f);
      obstacles.push_back(Obstacle{Kind::PhaseWall, -(2.0f * k + 1.0f)});
      nextSpawnZ -= 70.0;
    }
  }

  void integrateAxis(FlexPoint &p, FlexPoint &v, float input, float lo, float hi) {
    static const FlexPoint dt = kStepSeconds;
    static const FlexPoint drag = 4.0;
    static const FlexPoint maxSpeed = 14.0;
    v += FlexPoint(70.0 * input) * dt;
    v -= v * drag * dt;
    if (quantizeVelocity) {
      v = flight.view(v);
    }
    v = std::clamp(v, -maxSpeed, maxSpeed);
    p += v * dt;
    p = flight.commit(p); // snaps only under the Lossy policy
    if (p < FlexPoint(lo)) {
      p = lo;
      v = FlexPoint();
    } else if (p > FlexPoint(hi)) {
      p = hi;
      v = FlexPoint();
    }
  }

  void crash(Obstacle &o, float damage, const char *what) {
    shield = std::max(0.0f, shield - damage);
    o.flash = 0.6f;
    o.flashColor = glm::vec3(1.0f, 0.2f, 0.1f);
    char text[64];
    std::snprintf(text, sizeof(text), "%s  -%.0f shield", what, damage);
    say(text, IM_COL32(255, 90, 70, 255));
    if (shield <= 0.0f) {
      over = true;
      say("GAME OVER - press R", IM_COL32(255, 255, 255, 255));
    }
  }

  void checkCollisions() {
    const Rect ship{observed.x - kShipHalfX, observed.y - kShipHalfY, observed.x + kShipHalfX,
                    observed.y + kShipHalfY};
    const float window = kWallHalfZ + kShipHalfZ;
    for (Obstacle &o : obstacles) {
      if (o.resolved) {
        continue;
      }
      if (o.kind == Kind::FocusPickup || o.kind == Kind::PhasePickup) {
        float lateral = glm::length(observed - glm::vec2(o.cx, o.cy));
        if (std::abs(observedZ - o.z) < 0.9f && lateral < 1.1f) {
          o.resolved = true;
          if (o.kind == Kind::FocusPickup) {
            focus = {focusSeconds, focusSeconds};
            say("FOCUS  flight precision up", IM_COL32(80, 230, 255, 255));
          } else {
            phase = {phaseSeconds, phaseSeconds};
            say("PHASE  hull precision down", IM_COL32(255, 80, 230, 255));
          }
        } else if (z < o.z - 3.0) {
          o.resolved = true;
        }
        continue;
      }

      if (std::abs(observedZ - o.z) <= window) {
        o.touched = true;
        o.resolved = true;
        if (o.kind == Kind::PhaseWall) {
          crash(o, 34.0f, "WALL HIT");
          continue;
        }
        Rect hole{o.cx - o.w / 2, o.cy - o.h / 2, o.cx + o.w / 2, o.cy + o.h / 2};
        Rect frame{hole.x0 - kGateBar, hole.y0 - kGateBar, hole.x1 + kGateBar, hole.y1 + kGateBar};
        if (hole.contains(ship)) {
          bool gold = o.kind == Kind::GoldGate;
          score += gold ? 300 : 100;
          gold ? ++goldGates : ++gates;
          o.flash = 0.5f;
          o.flashColor = glm::vec3(1.0f);
          say(gold ? "GOLD GATE  +300" : "GATE  +100",
              gold ? IM_COL32(255, 210, 60, 255) : IM_COL32(120, 255, 140, 255));
        } else if (frame.overlaps(ship)) {
          crash(o, 20.0f, "CLIPPED GATE");
        }
      } else if (!o.touched && z < o.z - window - 0.5) {
        // Passed without the hull ever sampling a position inside it
        o.resolved = true;
        if (o.kind == Kind::PhaseWall) {
          score += 150;
          ++phasedWalls;
          say("PHASED  +150", IM_COL32(255, 80, 230, 255));
        }
      }
    }
  }

  // Aim for the nearest gate opening or pickup ahead. Walls are left to the
  // PHASE pickup that precedes them.
  void autopilotInput(float &inputX, float &inputY) {
    const Obstacle *next = nullptr;
    for (const Obstacle &o : obstacles) {
      if (!o.resolved && o.kind != Kind::PhaseWall && o.z < z && (!next || o.z > next->z)) {
        next = &o;
      }
    }
    if (!next) {
      return;
    }
    auto steer = [](float error, float velocity) { return std::clamp(error * 2.0f - velocity * 0.25f, -1.0f, 1.0f); };
    inputX = steer(next->cx - static_cast<float>(x.value()), static_cast<float>(vx.value()));
    inputY = steer(next->cy - static_cast<float>(y.value()), static_cast<float>(vy.value()));
  }

  void step(GLFWwindow *window) {
    if (over || paused) {
      return;
    }
    const float dt = static_cast<float>(kStepSeconds);
    focus.remaining = std::max(0.0f, focus.remaining - dt);
    phase.remaining = std::max(0.0f, phase.remaining - dt);
    updateGroups();

    auto key = [&](int a, int b) { return glfwGetKey(window, a) == GLFW_PRESS || glfwGetKey(window, b) == GLFW_PRESS; };
    float inputX = (key(GLFW_KEY_D, GLFW_KEY_RIGHT) ? 1.0f : 0.0f) - (key(GLFW_KEY_A, GLFW_KEY_LEFT) ? 1.0f : 0.0f);
    float inputY = (key(GLFW_KEY_W, GLFW_KEY_UP) ? 1.0f : 0.0f) - (key(GLFW_KEY_S, GLFW_KEY_DOWN) ? 1.0f : 0.0f);
    if (autopilot) {
      autopilotInput(inputX, inputY);
    }
    integrateAxis(x, vx, inputX, -kLaneHalfWidth, kLaneHalfWidth);
    integrateAxis(y, vy, inputY, kLaneMinY, kLaneMaxY);
    z -= speed * kStepSeconds;

    observed = glm::vec2(flight.view(x).value(), flight.view(y).value());
    observedZ = static_cast<float>(hull.view(FlexPoint(z)).value());

    checkCollisions();

    while (nextSpawnZ > z - 260.0) {
      spawnSegment();
    }
    obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(),
                                   [&](const Obstacle &o) { return o.z > z + 20.0; }),
                    obstacles.end());
  }
};

// ---------------------------------------------------------------------------
// Drawing

void drawGate(Renderer &r, const Obstacle &o, glm::vec3 color) {
  const float t = kGateBar, d = 2.0f * kWallHalfZ;
  float left = o.cx - o.w / 2, right = o.cx + o.w / 2, bottom = o.cy - o.h / 2, top = o.cy + o.h / 2;
  r.drawBox({left - t / 2, o.cy, o.z}, {t, o.h + 2 * t, d}, color);
  r.drawBox({right + t / 2, o.cy, o.z}, {t, o.h + 2 * t, d}, color);
  r.drawBox({o.cx, top + t / 2, o.z}, {o.w, t, d}, color);
  r.drawBox({o.cx, bottom - t / 2, o.z}, {o.w, t, d}, color);
}

void buildLines(Renderer &r, double z) {
  std::vector<float> ground, posts;
  const glm::vec3 up(0, 1, 0);
  const float zNear = static_cast<float>(z) + 12.0f, zFar = static_cast<float>(z) - 260.0f;
  for (float gz = std::floor(zNear / 4.0f) * 4.0f; gz > zFar; gz -= 4.0f) {
    pushVertex(ground, {-60.0f, kGroundY, gz}, up);
    pushVertex(ground, {60.0f, kGroundY, gz}, up);
  }
  for (float gx = -60.0f; gx <= 60.0f; gx += 4.0f) {
    pushVertex(ground, {gx, kGroundY, zNear}, up);
    pushVertex(ground, {gx, kGroundY, zFar}, up);
  }
  for (float pz = std::floor(zNear / 8.0f) * 8.0f; pz > zFar; pz -= 8.0f) {
    for (float side : {-1.0f, 1.0f}) {
      float px = side * (kLaneHalfWidth + 1.2f);
      pushVertex(posts, {px, kGroundY, pz}, up);
      pushVertex(posts, {px, kCeilingY - 1.0f, pz}, up);
    }
  }
  uploadMesh(r.ground, ground);
  uploadMesh(r.posts, posts);
}

void drawWorld(Renderer &r, Game &g, int width, int height, float time) {
  glUseProgram(r.program);
  glUniform2f(r.viewport, static_cast<float>(width), static_cast<float>(height));
  glUniform3fv(r.fogColor, 1, glm::value_ptr(kSkyColor));
  glUniform1f(r.fogDensity, 0.012f);

  const float z = static_cast<float>(g.z);
  glm::vec3 eye(g.smoothed.x * 0.55f, g.smoothed.y * 0.45f + 2.0f, z + 7.5f);
  glm::vec3 target(g.smoothed.x * 0.8f, g.smoothed.y * 0.7f + 0.6f, z - 14.0f);
  glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
  glm::mat4 proj = glm::perspective(glm::radians(60.0f), width / std::max(1.0f, float(height)), 0.1f, 400.0f);
  glm::mat4 viewProj = proj * view;
  glUniformMatrix4fv(r.viewProj, 1, GL_FALSE, glm::value_ptr(viewProj));

  r.setSnap(g.render.precision().fracBits);

  buildLines(r, g.z);
  r.draw(r.ground, glm::mat4(1.0f), {0.08f, 0.35f, 0.5f}, false);
  r.draw(r.posts, glm::mat4(1.0f), {0.45f, 0.2f, 0.6f}, false);

  auto flashed = [](const Obstacle &o, glm::vec3 base) {
    return o.flash > 0.0f ? glm::mix(base, o.flashColor, std::min(1.0f, o.flash * 2.0f)) : base;
  };

  for (Obstacle &o : g.obstacles) {
    switch (o.kind) {
    case Kind::Gate:
      drawGate(r, o, flashed(o, {0.25f, 0.85f, 0.4f}));
      break;
    case Kind::GoldGate:
      drawGate(r, o, flashed(o, {1.0f, 0.78f, 0.15f}));
      break;
    case Kind::FocusPickup:
    case Kind::PhasePickup: {
      if (o.resolved && o.flash <= 0.0f) {
        break;
      }
      glm::mat4 m = glm::translate(glm::mat4(1.0f), {o.cx, o.cy, o.z});
      m = glm::rotate(m, time * 2.0f, glm::vec3(0, 1, 0));
      m = glm::scale(m, glm::vec3(0.6f, 0.8f, 0.6f));
      glm::vec3 c = o.kind == Kind::FocusPickup ? glm::vec3(0.3f, 0.9f, 1.0f) : glm::vec3(1.0f, 0.3f, 0.9f);
      if (!o.resolved) {
        r.draw(r.octahedron, m, c);
      }
      break;
    }
    case Kind::PhaseWall:
      break; // translucent, drawn below
    }
  }

  // Ship, drawn where physics sees it (eased slightly so snapping reads as
  // motion rather than teleporting)
  glm::mat4 ship = glm::translate(glm::mat4(1.0f), {g.smoothed.x, g.smoothed.y, z});
  ship = glm::rotate(ship, std::clamp(static_cast<float>(-g.vx.value()) * 0.06f, -0.7f, 0.7f), {0, 0, 1});
  ship = glm::rotate(ship, std::clamp(static_cast<float>(g.vy.value()) * 0.03f, -0.4f, 0.4f), {1, 0, 0});
  r.draw(r.ship, ship, g.phase.active() ? glm::vec3(0.85f, 0.55f, 0.95f) : glm::vec3(0.75f, 0.8f, 0.9f));
  r.draw(r.cube, glm::scale(glm::translate(ship, {0.0f, 0.05f, 0.7f}), glm::vec3(0.35f, 0.18f, 0.1f)),
         {1.0f, 0.55f + 0.2f * std::sin(time * 30.0f), 0.1f}, false);

  // Physics hitbox at the exact observed position, without render snapping
  if (g.showHitbox) {
    r.setSnap(64);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    r.drawBox({g.observed.x, g.observed.y, g.observedZ}, {2 * kShipHalfX, 2 * kShipHalfY, 2 * kShipHalfZ},
              {1.0f, 1.0f, 0.3f}, false);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    r.setSnap(g.render.precision().fracBits);
  }

  // Translucent energy walls, far to near
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  for (auto it = g.obstacles.rbegin(); it != g.obstacles.rend(); ++it) {
    if (it->kind != Kind::PhaseWall) {
      continue;
    }
    float pulse = 0.3f + 0.1f * std::sin(time * 6.0f + it->z);
    r.drawBox({0.0f, (kGroundY + kCeilingY) / 2, it->z},
              {2 * kLaneHalfWidth + 3.0f, kCeilingY - kGroundY, 2 * kWallHalfZ},
              flashed(*it, {1.0f, 0.2f, 0.85f}), false, pulse);
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void drawHud(Game &g) {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.75f);
  ImGui::Begin("Precision Flight", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("Score %d   Distance %.0f", g.score, -g.z);
  ImGui::Text("Gates %d   Gold %d   Phased walls %d", g.gates, g.goldGates, g.phasedWalls);
  char label[32];
  std::snprintf(label, sizeof(label), "Shield %.0f", g.shield);
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
  ImGui::ProgressBar(g.shield / 100.0f, ImVec2(260, 0), label);
  ImGui::PopStyleColor();

  auto effectBar = [](const char *name, const Effect &e, ImVec4 color) {
    char text[32];
    std::snprintf(text, sizeof(text), "%s %.1fs", name, e.remaining);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(e.active() ? e.remaining / e.duration : 0.0f, ImVec2(260, 0), e.active() ? text : name);
    ImGui::PopStyleColor();
  };
  effectBar("FOCUS", g.focus, ImVec4(0.3f, 0.9f, 1.0f, 1.0f));
  effectBar("PHASE", g.phase, ImVec4(1.0f, 0.3f, 0.9f, 1.0f));

  if (ImGui::BeginTable("groups", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Group");
    ImGui::TableSetupColumn("Bits");
    ImGui::TableSetupColumn("Step");
    ImGui::TableHeadersRow();
    for (const FlexGroup *group : {&g.flight, &g.hull, &g.render}) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(group->name().c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", group->precision().fracBits);
      ImGui::TableNextColumn();
      ImGui::Text("%g", group->precision().step());
    }
    ImGui::EndTable();
  }

  if (ImGui::CollapsingHeader("Precision lab")) {
    ImGui::SliderInt("Flight bits", &g.flightBase, -2, 12);
    ImGui::SliderInt("Hull bits", &g.hullBase, -2, 12);
    ImGui::SliderInt("Render bits", &g.renderBase, 1, 12);
    ImGui::SliderInt("FOCUS flight bits", &g.focusFlightBits, -2, 12);
    ImGui::SliderInt("PHASE hull bits", &g.phaseHullBits, -2, 12);
    ImGui::SliderInt("PHASE render bits", &g.phaseRenderBits, 1, 12);
    int rounding = g.rounding == Rounding::Nearest ? 1 : 0;
    ImGui::RadioButton("Truncate", &rounding, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Round to nearest", &rounding, 1);
    g.rounding = rounding == 1 ? Rounding::Nearest : Rounding::TowardZero;
    ImGui::Checkbox("Lossy velocity (drag underflows)", &g.quantizeVelocity);
    ImGui::Checkbox("Lossy position (small moves vanish)", &g.lossyPosition);
    ImGui::Checkbox("Show physics hitbox", &g.showHitbox);
    ImGui::Checkbox("Autopilot (T)", &g.autopilot);
    ImGui::SliderFloat("Rail speed", &g.speed, 5.0f, 60.0f);
  }
  ImGui::TextDisabled("WASD/arrows steer  P pause  R restart  T autopilot");
  ImGui::End();

  // Event messages, fading out near the top of the screen
  ImDrawList *draw = ImGui::GetForegroundDrawList();
  ImVec2 size = ImGui::GetIO().DisplaySize;
  float y = size.y * 0.18f;
  for (const Message &m : g.messages) {
    float alpha = std::clamp(1.8f - m.age, 0.0f, 1.0f);
    if (alpha <= 0.0f) {
      continue;
    }
    const float fontSize = 30.0f;
    ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, m.text.c_str());
    ImU32 color = (m.color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha * 255) << IM_COL32_A_SHIFT);
    draw->AddText(ImGui::GetFont(), fontSize, ImVec2((size.x - textSize.x) / 2, y), color, m.text.c_str());
    y += fontSize + 6.0f;
  }
  if (g.paused && !g.over) {
    const char *text = "PAUSED";
    ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(40.0f, FLT_MAX, 0.0f, text);
    draw->AddText(ImGui::GetFont(), 40.0f, ImVec2((size.x - textSize.x) / 2, size.y * 0.45f),
                  IM_COL32(255, 255, 255, 220), text);
  }
}

} // namespace

int main(int argc, char **argv) {
  if (!glfwInit()) {
    std::fprintf(stderr, "Failed to initialize GLFW\n");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS
  glfwWindowHint(GLFW_SAMPLES, 4);

  GLFWwindow *window = glfwCreateWindow(1280, 800, "Precision Flight", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "Failed to create window\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    std::fprintf(stderr, "Failed to initialize GLEW\n");
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini into the working directory
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  Renderer renderer;
  renderer.init();
  Game game;
  game.reset();
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--autopilot") {
      game.autopilot = true;
    }
  }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glLineWidth(1.0f);

  double previous = glfwGetTime(), accumulator = 0.0;
  bool pauseHeld = false, restartHeld = false, autopilotHeld = false, nudged = false;
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    double now = glfwGetTime();
    double frame = std::min(now - previous, 0.25);
    previous = now;

    bool typing = ImGui::GetIO().WantTextInput;
    bool pauseDown = !typing && glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    bool restartDown = !typing && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (pauseDown && !pauseHeld) {
      game.paused = !game.paused;
    }
    if (restartDown && !restartHeld) {
      game.reset();
    }
    bool autopilotDown = !typing && glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    if (autopilotDown && !autopilotHeld) {
      game.autopilot = !game.autopilot;
    }
    autopilotHeld = autopilotDown;
    pauseHeld = pauseDown;
    restartHeld = restartDown;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    accumulator += frame;
    while (accumulator >= kStepSeconds) {
      game.step(window);
      accumulator -= kStepSeconds;
    }
    if (!game.paused) {
      float ease = 1.0f - std::exp(-25.0f * static_cast<float>(frame));
      game.smoothed += (game.observed - game.smoothed) * ease;
      for (Obstacle &o : game.obstacles) {
        o.flash = std::max(0.0f, o.flash - static_cast<float>(frame));
      }
      for (Message &m : game.messages) {
        m.age += static_cast<float>(frame);
      }
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(kSkyColor.r, kSkyColor.g, kSkyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawWorld(renderer, game, width, height, static_cast<float>(now));

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    drawHud(game);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);

    // GLFW 3.3 on macOS can render into a quarter of a high-DPI window until
    // the window moves once
    if (!nudged) {
      int wx, wy;
      glfwGetWindowPos(window, &wx, &wy);
      glfwSetWindowPos(window, wx + 1, wy);
      nudged = true;
    }
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
