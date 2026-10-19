#include "FlightDisplayPadd.hpp"

// GLEW must be included before GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <ScratchPadd/Graphics/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace PrecisionFlight {

namespace {
const glm::vec3 kSkyColor(0.02f, 0.03f, 0.08f);
} // namespace

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
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)nullptr);
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

void drawGate(Renderer &r, const Obstacle &o, glm::vec3 color) {
  const float t = kGateBar, d = 2.0f * kWallHalfZ;
  float left = o.cx - o.w / 2, right = o.cx + o.w / 2, bottom = o.cy - o.h / 2,
        top = o.cy + o.h / 2;
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

struct FlightDisplayPadd::ControlPanels {
  std::unordered_map<std::string, std::unique_ptr<Graphics::ControlView>> views; // by padd name
};

FlightDisplayPadd::FlightDisplayPadd(ScratchPadd::System *system)
    : Base(system), renderer_(std::make_unique<Renderer>()),
      controlPanels_(std::make_unique<ControlPanels>()) {
  receive<WorldSnapshot>([this](const WorldSnapshot &world) { drawFrame(world); });
  receive<ScratchPadd::MessageType::ControlSnapshot>(
      [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot) {
        showControls(snapshot);
      });
  receive<GatePassed>([this](const GatePassed &e) { showEvent(e); });
  receive<Crashed>([this](const Crashed &e) { showEvent(e); });
  receive<PowerupCollected>([this](const PowerupCollected &e) { showEvent(e); });
  receive<WallPhased>([this](const WallPhased &e) { showEvent(e); });
  receive<GameOver>([this](const GameOver &e) { showEvent(e); });
}

FlightDisplayPadd::~FlightDisplayPadd() = default;

void FlightDisplayPadd::starting() {
  if (!glfwInit()) {
    std::fprintf(stderr, "Failed to initialize GLFW\n");
    system_->stop();
    return;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS
  glfwWindowHint(GLFW_SAMPLES, 4);

  window_ = glfwCreateWindow(1280, 800, "Precision Flight", nullptr, nullptr);
  if (!window_) {
    std::fprintf(stderr, "Failed to create window\n");
    system_->stop();
    return;
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    std::fprintf(stderr, "Failed to initialize GLEW\n");
    system_->stop();
    return;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini into the working directory
  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  renderer_->init();
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glLineWidth(1.0f);

  // Ask the padds with controls (the simulation's precision lab) for them,
  // then start the frame chain.
  send(ScratchPadd::MessageType::ControlRequest{});
  previousTime_ = glfwGetTime();
  send(FrameInput{});
}

void FlightDisplayPadd::finishing() {
  if (!window_) {
    return;
  }
  controlPanels_->views.clear();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window_);
  glfwTerminate();
  window_ = nullptr;
}

void FlightDisplayPadd::showEvent(const FlightEvent &event) {
  // Hit and pass flashes on the obstacle, and a fading message
  struct Style {
    unsigned color;
    int obstacleId = -1;
    Flash flash;
  };
  const Style style = std::visit(
      [](const auto &e) -> Style {
        using E = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<E, GatePassed>) {
          return {e.gold ? IM_COL32(255, 210, 60, 255) : IM_COL32(120, 255, 140, 255), e.obstacleId,
                  Flash{0.5f, glm::vec3(1.0f)}};
        } else if constexpr (std::is_same_v<E, Crashed>) {
          return {IM_COL32(255, 90, 70, 255), e.obstacleId,
                  Flash{0.6f, glm::vec3(1.0f, 0.2f, 0.1f)}};
        } else if constexpr (std::is_same_v<E, PowerupCollected>) {
          return {e.kind == Kind::FocusPickup ? IM_COL32(80, 230, 255, 255)
                                              : IM_COL32(255, 80, 230, 255)};
        } else if constexpr (std::is_same_v<E, WallPhased>) {
          return {IM_COL32(255, 80, 230, 255)};
        } else {
          return {IM_COL32(255, 255, 255, 255)};
        }
      },
      event);
  if (style.obstacleId >= 0) {
    flashes_[style.obstacleId] = style.flash;
  }
  messages_.push_back({describe(event), style.color});
  if (messages_.size() > 4) {
    messages_.pop_front();
  }
}

void FlightDisplayPadd::showControls(const ScratchPadd::MessageType::ControlSnapshot &snapshot) {
  auto &views = controlPanels_->views;
  auto existing = views.find(snapshot.paddName);
  if (existing != views.end()) {
    existing->second->setControls(snapshot);
    return;
  }
  auto view = ControlViewBuilder();
  view->setControls(snapshot);
  view->setOnChange(
      [this](const ScratchPadd::MessageType::ControlChange &change) { send(change); });
  view->setInitialPlacement(ImVec2(1, 0)); // top-right, clear of the HUD at top-left
  views[snapshot.paddName] = std::move(view);
}

glm::vec3 FlightDisplayPadd::flashed(const Obstacle &o, glm::vec3 base) const {
  auto flash = flashes_.find(o.id);
  if (flash == flashes_.end() || flash->second.seconds <= 0.0f) {
    return base;
  }
  return glm::mix(base, flash->second.color, std::min(1.0f, flash->second.seconds * 2.0f));
}

void FlightDisplayPadd::drawFrame(const WorldSnapshot &world) {
  if (!window_) {
    return;
  }
  if (glfwWindowShouldClose(window_)) {
    system_->stop(); // ends the frame chain
    return;
  }
  world_ = world;

  glfwPollEvents();
  const double now = glfwGetTime();
  const double frame = std::min(now - previousTime_, 0.25);
  previousTime_ = now;

  // One-shot keys become commands for the simulation
  const bool typing = ImGui::GetIO().WantTextInput;
  auto pressed = [&](int key, bool &held) {
    const bool down = !typing && glfwGetKey(window_, key) == GLFW_PRESS;
    const bool edge = down && !held;
    held = down;
    return edge;
  };
  if (pressed(GLFW_KEY_P, pauseHeld_)) {
    send(GameCommand{GameCommand::Type::TogglePause});
  }
  if (pressed(GLFW_KEY_R, restartHeld_)) {
    send(GameCommand{GameCommand::Type::Restart});
    smoothed_ = glm::vec2(0.0f);
    flashes_.clear();
    messages_.clear();
  }
  if (pressed(GLFW_KEY_T, autopilotHeld_)) {
    send(GameCommand{GameCommand::Type::ToggleAutopilot});
  }
  if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
  }

  if (!world_.paused) {
    const float seconds = static_cast<float>(frame);
    const float ease = 1.0f - std::exp(-25.0f * seconds);
    smoothed_ += (world_.observed - smoothed_) * ease;
    for (auto &[id, flash] : flashes_) {
      flash.seconds = std::max(0.0f, flash.seconds - seconds);
    }
    for (OnScreenMessage &m : messages_) {
      m.age += seconds;
    }
  }

  int width, height;
  glfwGetFramebufferSize(window_, &width, &height);
  glViewport(0, 0, width, height);
  glClearColor(kSkyColor.r, kSkyColor.g, kSkyColor.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  drawWorld(width, height, static_cast<float>(now));

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  drawHud();
  for (auto &[name, view] : controlPanels_->views) {
    view->draw();
  }
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  glfwSwapBuffers(window_);

  // GLFW 3.3 on macOS can render into a quarter of a high-DPI window until
  // the window moves once
  if (!nudged_) {
    int wx, wy;
    glfwGetWindowPos(window_, &wx, &wy);
    glfwSetWindowPos(window_, wx + 1, wy);
    nudged_ = true;
  }

  // Steering for the next frame (the simulation ignores it on autopilot)
  auto key = [&](int a, int b) {
    return !typing &&
           (glfwGetKey(window_, a) == GLFW_PRESS || glfwGetKey(window_, b) == GLFW_PRESS);
  };
  FrameInput next;
  next.seconds = frame;
  next.inputX = (key(GLFW_KEY_D, GLFW_KEY_RIGHT) ? 1.0f : 0.0f) -
                (key(GLFW_KEY_A, GLFW_KEY_LEFT) ? 1.0f : 0.0f);
  next.inputY =
      (key(GLFW_KEY_W, GLFW_KEY_UP) ? 1.0f : 0.0f) - (key(GLFW_KEY_S, GLFW_KEY_DOWN) ? 1.0f : 0.0f);
  send(next);
}

void FlightDisplayPadd::drawWorld(int width, int height, float time) {
  Renderer &r = *renderer_;
  const WorldSnapshot &g = world_;
  glUseProgram(r.program);
  glUniform2f(r.viewport, static_cast<float>(width), static_cast<float>(height));
  glUniform3fv(r.fogColor, 1, glm::value_ptr(kSkyColor));
  glUniform1f(r.fogDensity, 0.012f);

  const float z = static_cast<float>(g.z);
  glm::vec3 eye(smoothed_.x * 0.55f, smoothed_.y * 0.45f + 2.0f, z + 7.5f);
  glm::vec3 target(smoothed_.x * 0.8f, smoothed_.y * 0.7f + 0.6f, z - 14.0f);
  glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
  glm::mat4 proj =
      glm::perspective(glm::radians(60.0f), width / std::max(1.0f, float(height)), 0.1f, 400.0f);
  glm::mat4 viewProj = proj * view;
  glUniformMatrix4fv(r.viewProj, 1, GL_FALSE, glm::value_ptr(viewProj));

  r.setSnap(g.renderBits);

  buildLines(r, g.z);
  r.draw(r.ground, glm::mat4(1.0f), {0.08f, 0.35f, 0.5f}, false);
  r.draw(r.posts, glm::mat4(1.0f), {0.45f, 0.2f, 0.6f}, false);

  for (const Obstacle &o : g.obstacles) {
    switch (o.kind) {
    case Kind::Gate:
      drawGate(r, o, flashed(o, {0.25f, 0.85f, 0.4f}));
      break;
    case Kind::GoldGate:
      drawGate(r, o, flashed(o, {1.0f, 0.78f, 0.15f}));
      break;
    case Kind::FocusPickup:
    case Kind::PhasePickup: {
      if (o.resolved) {
        break;
      }
      glm::mat4 m = glm::translate(glm::mat4(1.0f), {o.cx, o.cy, o.z});
      m = glm::rotate(m, time * 2.0f, glm::vec3(0, 1, 0));
      m = glm::scale(m, glm::vec3(0.6f, 0.8f, 0.6f));
      glm::vec3 c =
          o.kind == Kind::FocusPickup ? glm::vec3(0.3f, 0.9f, 1.0f) : glm::vec3(1.0f, 0.3f, 0.9f);
      r.draw(r.octahedron, m, c);
      break;
    }
    case Kind::PhaseWall:
      break; // translucent, drawn below
    }
  }

  // Ship, drawn where physics sees it (eased slightly so snapping reads as
  // motion rather than teleporting)
  glm::mat4 ship = glm::translate(glm::mat4(1.0f), {smoothed_.x, smoothed_.y, z});
  ship = glm::rotate(ship, std::clamp(-g.vx * 0.06f, -0.7f, 0.7f), {0, 0, 1});
  ship = glm::rotate(ship, std::clamp(g.vy * 0.03f, -0.4f, 0.4f), {1, 0, 0});
  r.draw(r.ship, ship,
         g.phase.active() ? glm::vec3(0.85f, 0.55f, 0.95f) : glm::vec3(0.75f, 0.8f, 0.9f));
  r.draw(r.cube,
         glm::scale(glm::translate(ship, {0.0f, 0.05f, 0.7f}), glm::vec3(0.35f, 0.18f, 0.1f)),
         {1.0f, 0.55f + 0.2f * std::sin(time * 30.0f), 0.1f}, false);

  // Physics hitbox at the exact observed position, without render snapping
  if (showHitbox_) {
    r.setSnap(64);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    r.drawBox({g.observed.x, g.observed.y, g.observedZ},
              {2 * kShipHalfX, 2 * kShipHalfY, 2 * kShipHalfZ}, {1.0f, 1.0f, 0.3f}, false);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    r.setSnap(g.renderBits);
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

void FlightDisplayPadd::drawHud() {
  const WorldSnapshot &g = world_;
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
    ImGui::ProgressBar(e.active() ? e.remaining / e.duration : 0.0f, ImVec2(260, 0),
                       e.active() ? text : name);
    ImGui::PopStyleColor();
  };
  effectBar("FOCUS", g.focus, ImVec4(0.3f, 0.9f, 1.0f, 1.0f));
  effectBar("PHASE", g.phase, ImVec4(1.0f, 0.3f, 0.9f, 1.0f));

  if (ImGui::BeginTable("groups", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Group");
    ImGui::TableSetupColumn("Bits");
    ImGui::TableSetupColumn("Step");
    ImGui::TableHeadersRow();
    for (const GroupBits &group : g.groups) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(group.name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", group.fracBits);
      ImGui::TableNextColumn();
      ImGui::Text("%g", std::ldexp(1.0, -group.fracBits));
    }
    ImGui::EndTable();
  }

  ImGui::Checkbox("Show physics hitbox", &showHitbox_);
  ImGui::TextDisabled("Precision lab: see the FlightSim panel");
  ImGui::TextDisabled("WASD/arrows steer  P pause  R restart  T autopilot");
  ImGui::End();

  // Event messages, fading out near the top of the screen
  ImDrawList *draw = ImGui::GetForegroundDrawList();
  ImVec2 size = ImGui::GetIO().DisplaySize;
  float y = size.y * 0.18f;
  for (const OnScreenMessage &m : messages_) {
    float alpha = std::clamp(1.8f - m.age, 0.0f, 1.0f);
    if (alpha <= 0.0f) {
      continue;
    }
    const float fontSize = 30.0f;
    ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, m.text.c_str());
    ImU32 color =
        (m.color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha * 255) << IM_COL32_A_SHIFT);
    draw->AddText(ImGui::GetFont(), fontSize, ImVec2((size.x - textSize.x) / 2, y), color,
                  m.text.c_str());
    y += fontSize + 6.0f;
  }
  if (g.paused && !g.over) {
    const char *text = "PAUSED";
    ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(40.0f, FLT_MAX, 0.0f, text);
    draw->AddText(ImGui::GetFont(), 40.0f, ImVec2((size.x - textSize.x) / 2, size.y * 0.45f),
                  IM_COL32(255, 255, 255, 220), text);
  }
}

} // namespace PrecisionFlight
