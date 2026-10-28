#include "RagdollWindowPadd.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace CyclonePadds {

namespace Scene = ScratchPadd::Scene;

namespace {

const Scene::Colour kSky{224, 237, 250};
const Scene::Colour kBone{158, 107, 102};
const Scene::Colour kGround{179, 184, 191};
const Scene::Colour kJointHeld{26, 166, 51};      // within its slack
const Scene::Colour kJointStretched{230, 26, 26}; // beyond it
const glm::vec3 kLookAt(0.0f, 1.5f, 0.0f);
constexpr float kCameraDistance = 14.0f;

Scene::Vector3 toScene(const glm::vec3 &v) { return {v.x, v.y, v.z}; }

// A glm transform (rotation, scale and position) as the scene layer's
Scene::Transform toScene(const glm::mat4 &m) {
  Scene::Transform t;
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      t.m[row][column] = m[column][row]; // glm is column-major
  return t;
}

// Rings a metre apart on the ground, and the two axes, as line ends
std::vector<Scene::Vector3> groundLines() {
  std::vector<Scene::Vector3> ends;
  for (int ring = 1; ring < 20; ++ring) {
    for (int i = 0; i < 48; ++i) {
      for (int end : {i, i + 1}) {
        const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(end) / 48.0f;
        ends.push_back({ring * std::cos(angle), 0, ring * std::sin(angle)});
      }
    }
  }
  ends.insert(ends.end(), {{-20, 0, 0}, {20, 0, 0}, {0, 0, -20}, {0, 0, 20}});
  return ends;
}

// Where a point fixed to a bone (in its own coordinates) is now
glm::vec3 pointOn(const BoneSnapshot &bone, glm::vec3 point) {
  return bone.position + bone.orientation * point;
}

} // namespace

RagdollWindowPadd::RagdollWindowPadd(ScratchPadd::System *system)
    : Base(system),
      panels_(
          [this](ScratchPadd::PaddAddress owner,
                 const ScratchPadd::MessageType::ControlChange &change) { sendTo(owner, change); },
          // Down the right side: the physics, then this window's own, then
          // the rest (AudioPadd's volumes)
          [this](const std::string &padd, std::size_t) {
            return ImVec2(1, padd == "RagdollPhysics" ? 0.0f : padd == name() ? 0.45f : 0.7f);
          }) {
  receive<RagdollSnapshot>([this](const RagdollSnapshot &snapshot) {
    // Draw a tick and a step behind the physics, so there's a snapshot on
    // each side to blend between
    timeline_.setDelay(kPhysicsTickSeconds + 1.0 / std::max(1, snapshot.stats.stepsPerSecond));
    timeline_.add(snapshot, now());
    ++snapshotsCounted_;
  });
  receive<ScratchPadd::MessageType::ControlSnapshot>(
      [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot,
             ScratchPadd::PaddAddress owner) { panels_.show(snapshot, owner); });
}

RagdollWindowPadd::~RagdollWindowPadd() = default;

double RagdollWindowPadd::now() const {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - opened_).count();
}

void RagdollWindowPadd::starting() {
  display_ = std::make_unique<Scene::Display>(1280, 800, "Cyclone padds: ragdoll");
  if (!display_->isOpen()) {
    spdlog::error("No window: {}", display_->error());
    system_->stop();
    return;
  }
  renderer_ = std::make_unique<Scene::Renderer>(*display_);
  ui_ = std::make_unique<ScratchPadd::DebugUi::Ui>(*display_, Theme::Colors::Light);
  cube_ = renderer_->load(Scene::MeshBuilder().box({}, {0.5f, 0.5f, 0.5f}).data());

  // Ask every padd with controls for them, this one included (its blending
  // switch), then start drawing
  sendIncludeSender(ScratchPadd::MessageType::ControlRequest{});
  send(CameraTurned{cameraTurn_});
  push([this] { drawFrame(); });
}

void RagdollWindowPadd::finishing() {
  panels_.clear();
  ui_.reset();
  renderer_.reset();
  display_.reset();
}

void RagdollWindowPadd::handleKeys() {
  if (display_->keyPressed(Scene::keyFor('R'))) {
    send(RagdollCommand{RagdollCommand::Type::Reset});
  }
  if (display_->keyPressed(Scene::keyFor('P'))) {
    send(RagdollCommand{RagdollCommand::Type::TogglePause});
  }
  if (display_->keyDown(Scene::Key::Escape)) {
    display_->close();
  }

  // Dragging outside the panels turns the camera
  const float x = display_->cursorX(), y = display_->cursorY();
  const bool down = display_->mouseDown(Scene::MouseButton::Left);
  if (down && !dragging_) {
    dragging_ = true;
  } else if (!down) {
    dragging_ = false;
  }
  if (dragging_ && x != dragX_) {
    cameraTurn_ += (x - dragX_) * 0.25f;
    send(CameraTurned{cameraTurn_});
  }
  if (dragging_) {
    cameraRise_ = std::clamp(cameraRise_ + (y - dragY_) * 0.25f, 0.0f, 80.0f);
  }
  dragX_ = x;
  dragY_ = y;
}

void RagdollWindowPadd::drawStats() {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  Theme::beginWindow("Cyclone in padds", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
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
    if (stats.audio) {
      ImGui::Text("Knocks sent to AudioPadd: %u", static_cast<unsigned>(stats.knocks));
    } else {
      ImGui::TextUnformatted("No AudioPadd: no sound");
    }
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

void RagdollWindowPadd::drawRagdoll(const std::vector<BoneSnapshot> &bones,
                                    const std::vector<JointSnapshot> &joints) {
  for (const BoneSnapshot &bone : bones) {
    const glm::mat4 m = glm::translate(glm::mat4(1.0f), bone.position) *
                        glm::mat4_cast(bone.orientation) *
                        glm::scale(glm::mat4(1.0f), 2.0f * bone.halfSize);
    renderer_->draw(cube_, toScene(m), {.colour = kBone});
  }
  // The joints over the bones (drawn on the overlay, so they show through)
  for (const JointSnapshot &joint : joints) {
    if (joint.bones[0] >= static_cast<int>(bones.size()) ||
        joint.bones[1] >= static_cast<int>(bones.size())) {
      continue;
    }
    const glm::vec3 a = pointOn(bones[joint.bones[0]], joint.points[0]);
    const glm::vec3 b = pointOn(bones[joint.bones[1]], joint.points[1]);
    float ax, ay, bx, by;
    if (renderer_->toScreen(toScene(a), ax, ay) && renderer_->toScreen(toScene(b), bx, by)) {
      renderer_->overlay().line(
          ax, ay, bx, by, glm::distance(a, b) > joint.slack ? kJointStretched : kJointHeld, 2);
    }
  }
}

void RagdollWindowPadd::drawFrame() {
  if (!display_->isOpen()) {
    system_->stop(); // ends the frame chain
    return;
  }
  display_->pollEvents();
  ui_->newFrame();
  handleKeys();

  const double time = now();
  ++framesCounted_;
  if (time - countingSince_ >= 1.0) {
    snapshotRate_ = snapshotsCounted_ / (time - countingSince_);
    frameRate_ = framesCounted_ / (time - countingSince_);
    snapshotsCounted_ = framesCounted_ = 0;
    countingSince_ = time;
  }

  const float turn = glm::radians(cameraTurn_), rise = glm::radians(cameraRise_);
  const glm::vec3 eye =
      kLookAt + kCameraDistance * glm::vec3(std::cos(rise) * std::cos(turn), std::sin(rise),
                                            std::cos(rise) * std::sin(turn));
  const Scene::Camera camera{
      .eye = toScene(eye), .target = toScene(kLookAt), .fieldOfView = 50, .farPlane = 200};
  // Lit from above and to the side, never darker than 45%
  Scene::Environment environment{.background = kSky, .ambient = {115, 115, 115}};
  environment.addLight({.direction = {-0.4f, -1.0f, -0.3f}, .colour = {140, 140, 140}});
  renderer_->beginFrame(camera, environment);
  static const std::vector<Scene::Vector3> ground = groundLines();
  renderer_->drawLines(ground, kGround);
  if (const RagdollSnapshot *newest = timeline_.newest()) {
    const bool blend = control<ScratchPadd::ControlType::Boolean>("Blend between snapshots").value;
    drawRagdoll(timeline_.bonesAt(time, blend), newest->joints);
  }
  drawStats();
  panels_.draw();
  renderer_->endFrame(); // waits for the screen's refresh
  push([this] { drawFrame(); });
}

} // namespace CyclonePadds
