#include "RagdollWindowPadd.hpp"

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
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace CyclonePadds {

namespace {

const glm::vec3 kSkyColor(0.88f, 0.93f, 0.98f);
const glm::vec3 kBoneColor(0.62f, 0.42f, 0.40f);
const glm::vec3 kGroundColor(0.70f, 0.72f, 0.75f);
const glm::vec3 kJointHeld(0.1f, 0.65f, 0.2f);     // within its slack
const glm::vec3 kJointStretched(0.9f, 0.1f, 0.1f); // beyond it
const glm::vec3 kLookAt(0.0f, 1.5f, 0.0f);
constexpr float kCameraDistance = 14.0f;

const char *kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uViewProj;
out vec3 vNormal;
void main() {
  vNormal = mat3(uModel) * aNormal;
  gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
}
)";

const char *kFragmentShader = R"(#version 330 core
in vec3 vNormal;
uniform vec3 uColor;
uniform int uLit;
out vec4 fragColor;
void main() {
  vec3 color = uColor;
  if (uLit == 1) {
    float light = max(dot(normalize(vNormal), normalize(vec3(0.4, 1.0, 0.3))), 0.0);
    color *= 0.45 + 0.55 * light;
  }
  fragColor = vec4(color, 1.0);
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

// Vertices are a position and a normal, 6 floats
struct Mesh {
  GLuint vao = 0, vbo = 0;
  GLsizei count = 0;
  GLenum mode = GL_TRIANGLES;
};

void upload(Mesh &mesh, const std::vector<float> &data) {
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data(),
               GL_DYNAMIC_DRAW);
  mesh.count = static_cast<GLsizei>(data.size() / 6);
}

Mesh makeMesh(const std::vector<float> &data, GLenum mode) {
  Mesh mesh;
  mesh.mode = mode;
  glGenVertexArrays(1, &mesh.vao);
  glGenBuffers(1, &mesh.vbo);
  glBindVertexArray(mesh.vao);
  upload(mesh, data);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        reinterpret_cast<void *>(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
  return mesh;
}

void pushVertex(std::vector<float> &data, glm::vec3 p, glm::vec3 n = glm::vec3(0, 1, 0)) {
  data.insert(data.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
}

// A unit cube centred on the origin, each face with its own normal
std::vector<float> cubeData() {
  std::vector<float> d;
  const float h = 0.5f;
  const glm::vec3 p[8] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
                          {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
  const int faces[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {5, 1, 2, 6},
                           {0, 4, 7, 3}, {7, 6, 2, 3}, {0, 1, 5, 4}};
  for (const auto &f : faces) {
    const glm::vec3 n = glm::normalize(glm::cross(p[f[1]] - p[f[0]], p[f[2]] - p[f[0]]));
    for (int corner : {0, 1, 2, 0, 2, 3}) {
      pushVertex(d, p[f[corner]], n);
    }
  }
  return d;
}

// Rings a metre apart on the ground, and the two axes
std::vector<float> groundData() {
  std::vector<float> d;
  for (int ring = 1; ring < 20; ++ring) {
    for (int i = 0; i < 48; ++i) {
      for (int end : {i, i + 1}) {
        const float angle = 2.0f * 3.14159265f * static_cast<float>(end) / 48.0f;
        pushVertex(d, glm::vec3(ring * std::cos(angle), 0, ring * std::sin(angle)));
      }
    }
  }
  pushVertex(d, {-20, 0, 0});
  pushVertex(d, {20, 0, 0});
  pushVertex(d, {0, 0, -20});
  pushVertex(d, {0, 0, 20});
  return d;
}

// Where a point fixed to a bone (in its own coordinates) is now
glm::vec3 pointOn(const BoneSnapshot &bone, glm::vec3 point) {
  return bone.position + bone.orientation * point;
}

} // namespace

struct RagdollRenderer {
  GLuint program = 0;
  GLint model = -1, viewProj = -1, color = -1, lit = -1;
  Mesh cube, ground, joints;

  void init() {
    program = glCreateProgram();
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    model = glGetUniformLocation(program, "uModel");
    viewProj = glGetUniformLocation(program, "uViewProj");
    color = glGetUniformLocation(program, "uColor");
    lit = glGetUniformLocation(program, "uLit");
    cube = makeMesh(cubeData(), GL_TRIANGLES);
    ground = makeMesh(groundData(), GL_LINES);
    joints = makeMesh({}, GL_LINES);
  }

  void draw(const Mesh &mesh, const glm::mat4 &m, glm::vec3 c, bool isLit) {
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(m));
    glUniform3fv(color, 1, glm::value_ptr(c));
    glUniform1i(lit, isLit ? 1 : 0);
    glBindVertexArray(mesh.vao);
    glDrawArrays(mesh.mode, 0, mesh.count);
  }

  // The bones as boxes, and each joint as a line between its two points:
  // green within its slack, red beyond it
  void drawRagdoll(const std::vector<BoneSnapshot> &bones, const std::vector<JointSnapshot> &js) {
    for (const BoneSnapshot &bone : bones) {
      const glm::mat4 m = glm::translate(glm::mat4(1.0f), bone.position) *
                          glm::mat4_cast(bone.orientation) *
                          glm::scale(glm::mat4(1.0f), 2.0f * bone.halfSize);
      draw(cube, m, kBoneColor, true);
    }
    std::vector<float> held, stretched;
    for (const JointSnapshot &joint : js) {
      if (joint.bones[0] >= static_cast<int>(bones.size()) ||
          joint.bones[1] >= static_cast<int>(bones.size())) {
        continue;
      }
      const glm::vec3 a = pointOn(bones[joint.bones[0]], joint.points[0]);
      const glm::vec3 b = pointOn(bones[joint.bones[1]], joint.points[1]);
      std::vector<float> &lines = glm::distance(a, b) > joint.slack ? stretched : held;
      pushVertex(lines, a);
      pushVertex(lines, b);
    }
    glDisable(GL_DEPTH_TEST); // joints show through the bones
    for (auto [lines, c] : {std::pair{&held, kJointHeld}, std::pair{&stretched, kJointStretched}}) {
      upload(joints, *lines);
      draw(joints, glm::mat4(1.0f), c, false);
    }
    glEnable(GL_DEPTH_TEST);
  }
};

struct RagdollWindowPadd::ControlPanels {
  std::unordered_map<std::string, std::unique_ptr<Graphics::ControlView>> views; // by padd name
};

RagdollWindowPadd::RagdollWindowPadd(ScratchPadd::System *system)
    : Base(system), renderer_(std::make_unique<RagdollRenderer>()),
      controlPanels_(std::make_unique<ControlPanels>()) {
  receive<RagdollSnapshot>([this](const RagdollSnapshot &snapshot) {
    // Draw a tick and a step behind the physics, so there's a snapshot on
    // each side to blend between
    timeline_.setDelay(kPhysicsTickSeconds + 1.0 / std::max(1, snapshot.stats.stepsPerSecond));
    timeline_.add(snapshot, now());
    ++snapshotsCounted_;
  });
  receive<ScratchPadd::MessageType::ControlSnapshot>(
      [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot,
             ScratchPadd::PaddAddress owner) { showControls(snapshot, owner); });
}

RagdollWindowPadd::~RagdollWindowPadd() = default;

double RagdollWindowPadd::now() const {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - opened_).count();
}

void RagdollWindowPadd::starting() {
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
  window_ = glfwCreateWindow(1280, 800, "Cyclone padds: ragdoll", nullptr, nullptr);
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
  ImGui::StyleColorsLight();
  ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini into the working directory
  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  renderer_->init();
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);

  // Ask every padd with controls for them, this one included (its blending
  // switch), then start drawing
  sendIncludeSender(ScratchPadd::MessageType::ControlRequest{});
  push([this] { drawFrame(); });
}

void RagdollWindowPadd::finishing() {
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

void RagdollWindowPadd::showControls(const ScratchPadd::MessageType::ControlSnapshot &snapshot,
                                     ScratchPadd::PaddAddress owner) {
  auto &views = controlPanels_->views;
  auto existing = views.find(snapshot.paddName);
  if (existing != views.end()) {
    existing->second->setControls(snapshot);
    return;
  }
  auto view = ControlViewBuilder();
  view->setControls(snapshot);
  view->setOnChange([this, owner](const ScratchPadd::MessageType::ControlChange &change) {
    sendTo(owner, change);
  });
  // Top right, the physics above the window's own
  view->setInitialPlacement(ImVec2(1, snapshot.paddName == name() ? 0.45f : 0.0f));
  views[snapshot.paddName] = std::move(view);
}

void RagdollWindowPadd::handleKeys() {
  const ImGuiIO &io = ImGui::GetIO();
  const bool typing = io.WantTextInput;
  auto pressed = [&](int key, bool &held) {
    const bool down = !typing && glfwGetKey(window_, key) == GLFW_PRESS;
    const bool edge = down && !held;
    held = down;
    return edge;
  };
  if (pressed(GLFW_KEY_R, resetHeld_)) {
    send(RagdollCommand{RagdollCommand::Type::Reset});
  }
  if (pressed(GLFW_KEY_P, pauseHeld_)) {
    send(RagdollCommand{RagdollCommand::Type::TogglePause});
  }
  if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
  }

  // Dragging outside the panels turns the camera
  double x, y;
  glfwGetCursorPos(window_, &x, &y);
  const bool down = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  if (down && !dragging_ && !io.WantCaptureMouse) {
    dragging_ = true;
  } else if (!down) {
    dragging_ = false;
  }
  if (dragging_) {
    cameraTurn_ += static_cast<float>(x - dragX_) * 0.25f;
    cameraRise_ = std::clamp(cameraRise_ + static_cast<float>(y - dragY_) * 0.25f, 0.0f, 80.0f);
  }
  dragX_ = x;
  dragY_ = y;
}

void RagdollWindowPadd::drawStats() {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::Begin("Cyclone in padds", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::TextUnformatted("RagdollPhysics steps the ragdoll on its own thread and sends\n"
                         "a snapshot each tick; this window draws them at the screen's rate.");
  ImGui::Separator();
  if (const RagdollSnapshot *newest = timeline_.newest()) {
    const PhysicsStats &stats = newest->stats;
    ImGui::Text("Physics: %d steps a second, %d sub-step%s each", stats.stepsPerSecond,
                stats.subSteps, stats.subSteps == 1 ? "" : "s");
    ImGui::Text("Physics thread busy: %.1f%%", 100 * stats.busy);
    ImGui::Text("Joint stretch now: %.2f cm, worst since reset: %.2f cm", 100 * stats.stretchNow,
                100 * stats.worstStretch);
  } else {
    ImGui::TextUnformatted("Waiting for the physics...");
  }
  ImGui::Text("Snapshots: %.0f a second; frames: %.0f a second", snapshotRate_, frameRate_);
  ImGui::Text("Drawing %.0f ms behind the physics%s", 1000 * timeline_.delay(),
              control<ScratchPadd::ControlType::Boolean>("Blend between snapshots").value
                  ? ", blended"
                  : " (not blending: the newest as it is)");
  ImGui::Separator();
  ImGui::TextUnformatted("R: reset   P: pause   Drag: turn the camera   Esc: quit");
  ImGui::End();
}

void RagdollWindowPadd::drawFrame() {
  if (!window_) {
    return;
  }
  if (glfwWindowShouldClose(window_)) {
    system_->stop(); // ends the frame chain
    return;
  }
  glfwPollEvents();
  handleKeys();

  const double time = now();
  ++framesCounted_;
  if (time - countingSince_ >= 1.0) {
    snapshotRate_ = snapshotsCounted_ / (time - countingSince_);
    frameRate_ = framesCounted_ / (time - countingSince_);
    snapshotsCounted_ = framesCounted_ = 0;
    countingSince_ = time;
  }

  int width, height;
  glfwGetFramebufferSize(window_, &width, &height);
  glViewport(0, 0, width, height);
  glClearColor(kSkyColor.r, kSkyColor.g, kSkyColor.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const float turn = glm::radians(cameraTurn_), rise = glm::radians(cameraRise_);
  const glm::vec3 eye =
      kLookAt + kCameraDistance * glm::vec3(std::cos(rise) * std::cos(turn), std::sin(rise),
                                            std::cos(rise) * std::sin(turn));
  const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
  const glm::mat4 viewProj = glm::perspective(glm::radians(50.0f), aspect, 0.1f, 200.0f) *
                             glm::lookAt(eye, kLookAt, glm::vec3(0, 1, 0));
  RagdollRenderer &r = *renderer_;
  glUseProgram(r.program);
  glUniformMatrix4fv(r.viewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
  r.draw(r.ground, glm::mat4(1.0f), kGroundColor, false);
  if (const RagdollSnapshot *newest = timeline_.newest()) {
    const bool blend = control<ScratchPadd::ControlType::Boolean>("Blend between snapshots").value;
    r.drawRagdoll(timeline_.bonesAt(time, blend), newest->joints);
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  drawStats();
  for (auto &[padd, view] : controlPanels_->views) {
    view->draw();
  }
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window_); // waits for the screen's refresh

  // GLFW 3.3 on macOS can render into a quarter of a high-DPI window until
  // the window moves once
  if (!nudged_) {
    int wx, wy;
    glfwGetWindowPos(window_, &wx, &wy);
    glfwSetWindowPos(window_, wx + 1, wy);
    nudged_ = true;
  }

  push([this] { drawFrame(); });
}

} // namespace CyclonePadds
