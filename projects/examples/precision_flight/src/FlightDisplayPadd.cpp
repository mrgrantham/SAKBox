#include "FlightDisplayPadd.hpp"

#include <ScratchPadd/Scene/GL/Desktop.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace PrecisionFlight {

namespace Scene = ScratchPadd::Scene;

namespace {
const glm::vec3 kSkyColor(0.02f, 0.03f, 0.08f);

Scene::Vector3 toScene(glm::vec3 v) { return {v.x, v.y, v.z}; }
Scene::Colour toColour(glm::vec3 c, float alpha = 1.0f) {
  return Scene::Colour::fromUnit(c.r, c.g, c.b, alpha);
}

// A glm transform (rotation, scale and position) as the scene layer's
Scene::Transform toScene(const glm::mat4 &m) {
  Scene::Transform t;
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      t.m[row][column] = m[column][row]; // glm is column-major
  return t;
}

Scene::Transform box(glm::vec3 center, glm::vec3 size) {
  return toScene(glm::scale(glm::translate(glm::mat4(1.0f), center), size));
}

// Low-poly fighter facing -z
Scene::MeshBuilder shipMesh() {
  const Scene::Vector3 nose{0.0f, 0.0f, -1.4f}, top{0.0f, 0.28f, 0.3f}, bottom{0.0f, -0.18f, 0.4f},
      tail{0.0f, 0.05f, 0.65f}, left{-1.3f, -0.1f, 0.5f}, right{1.3f, -0.1f, 0.5f},
      finLeft{-0.55f, 0.65f, 0.7f}, finRight{0.55f, 0.65f, 0.7f};
  Scene::MeshBuilder ship;
  ship.triangle(nose, left, top)
      .triangle(nose, top, right)
      .triangle(top, left, tail)
      .triangle(top, tail, right)
      .triangle(nose, bottom, left)
      .triangle(nose, right, bottom)
      .triangle(left, bottom, tail)
      .triangle(bottom, right, tail)
      .triangle(top, tail, finLeft)
      .triangle(top, finRight, tail);
  return ship;
}

Scene::MeshBuilder octahedronMesh() {
  const Scene::Vector3 up{0, 1, 0}, down{0, -1, 0};
  const Scene::Vector3 ring[4] = {{1, 0, 0}, {0, 0, 1}, {-1, 0, 0}, {0, 0, -1}};
  Scene::MeshBuilder octahedron;
  for (int i = 0; i < 4; ++i) {
    const Scene::Vector3 a = ring[i], b = ring[(i + 1) % 4];
    octahedron.triangle(up, b, a).triangle(down, a, b);
  }
  return octahedron;
}

// Render precision: fewer bits means coarser vertex snapping
void setSnap(Scene::Renderer &renderer, int bits) {
  const auto world = static_cast<float>(std::ldexp(8.0, -bits));
  const auto screen = static_cast<float>(std::ldexp(64.0, -bits));
  Scene::Desktop::setVertexSnap(renderer, world < 0.02f ? 0.0f : world,
                                screen < 1.0f ? 0.0f : screen);
}

// The ground's grid and the posts beside the lane, around `z`, as line ends
void groundLines(double z, std::vector<Scene::Vector3> &ground,
                 std::vector<Scene::Vector3> &posts) {
  ground.clear();
  posts.clear();
  const float zNear = static_cast<float>(z) + 12.0f, zFar = static_cast<float>(z) - 260.0f;
  for (float gz = std::floor(zNear / 4.0f) * 4.0f; gz > zFar; gz -= 4.0f) {
    ground.push_back({-60.0f, kGroundY, gz});
    ground.push_back({60.0f, kGroundY, gz});
  }
  for (float gx = -60.0f; gx <= 60.0f; gx += 4.0f) {
    ground.push_back({gx, kGroundY, zNear});
    ground.push_back({gx, kGroundY, zFar});
  }
  for (float pz = std::floor(zNear / 8.0f) * 8.0f; pz > zFar; pz -= 8.0f) {
    for (float side : {-1.0f, 1.0f}) {
      const float px = side * (kLaneHalfWidth + 1.2f);
      posts.push_back({px, kGroundY, pz});
      posts.push_back({px, kCeilingY - 1.0f, pz});
    }
  }
}

void drawGate(Scene::Renderer &r, Scene::Mesh cube, const Obstacle &o, glm::vec3 color) {
  const float t = kGateBar, d = 2.0f * kWallHalfZ;
  const float left = o.cx - o.w / 2, right = o.cx + o.w / 2, bottom = o.cy - o.h / 2,
              top = o.cy + o.h / 2;
  const Scene::Material lit{.colour = toColour(color)};
  r.draw(cube, box({left - t / 2, o.cy, o.z}, {t, o.h + 2 * t, d}), lit);
  r.draw(cube, box({right + t / 2, o.cy, o.z}, {t, o.h + 2 * t, d}), lit);
  r.draw(cube, box({o.cx, top + t / 2, o.z}, {o.w, t, d}), lit);
  r.draw(cube, box({o.cx, bottom - t / 2, o.z}, {o.w, t, d}), lit);
}

} // namespace

FlightDisplayPadd::FlightDisplayPadd(ScratchPadd::System *system)
    : Base(system),
      panels_(
          [this](ScratchPadd::PaddAddress owner,
                 const ScratchPadd::MessageType::ControlChange &change) { sendTo(owner, change); },
          // The precision lab top-right, clear of the HUD at top-left; any
          // other padd's panel (AudioPadd's volumes) bottom-right, clear of
          // the lab
          [](const std::string &padd, std::size_t) {
            return padd == "FlightSim" ? ImVec2(1, 0) : ImVec2(1, 1);
          }) {
  receive<WorldSnapshot>([this](const WorldSnapshot &world) { drawFrame(world); });
  receive<ScratchPadd::MessageType::ControlSnapshot>(
      [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot,
             ScratchPadd::PaddAddress owner) { panels_.show(snapshot, owner); });
  receive<GatePassed>([this](const GatePassed &e) { showEvent(e); });
  receive<Crashed>([this](const Crashed &e) { showEvent(e); });
  receive<PowerupCollected>([this](const PowerupCollected &e) { showEvent(e); });
  receive<WallPhased>([this](const WallPhased &e) { showEvent(e); });
  receive<GameOver>([this](const GameOver &e) { showEvent(e); });
}

FlightDisplayPadd::~FlightDisplayPadd() = default;

void FlightDisplayPadd::starting() {
  display_ = std::make_unique<Scene::Display>(1280, 800, "Precision Flight");
  if (!display_->isOpen()) {
    std::fprintf(stderr, "No window: %s\n", display_->error().c_str());
    system_->stop();
    return;
  }
  renderer_ = std::make_unique<Scene::Renderer>(*display_);
  ui_ = std::make_unique<ScratchPadd::DebugUi::Ui>(*display_);
  meshes_.cube = renderer_->load(Scene::MeshBuilder().box({}, {0.5f, 0.5f, 0.5f}).data());
  meshes_.ship = renderer_->load(shipMesh().data());
  meshes_.octahedron = renderer_->load(octahedronMesh().data());

  // Ask the padds with controls (the simulation's precision lab) for them,
  // then start the frame chain.
  send(ScratchPadd::MessageType::ControlRequest{});
  previousTime_ = display_->seconds();
  send(FrameInput{});
}

void FlightDisplayPadd::finishing() {
  panels_.clear();
  ui_.reset();
  renderer_.reset();
  display_.reset();
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

glm::vec3 FlightDisplayPadd::flashed(const Obstacle &o, glm::vec3 base) const {
  auto flash = flashes_.find(o.id);
  if (flash == flashes_.end() || flash->second.seconds <= 0.0f) {
    return base;
  }
  return glm::mix(base, flash->second.color, std::min(1.0f, flash->second.seconds * 2.0f));
}

void FlightDisplayPadd::drawFrame(const WorldSnapshot &world) {
  if (!display_ || !display_->isOpen()) {
    if (display_)
      system_->stop(); // ends the frame chain
    return;
  }
  world_ = world;

  display_->pollEvents();
  ui_->newFrame();
  const double now = display_->seconds();
  const double frame = std::min(now - previousTime_, 0.25);
  previousTime_ = now;

  // One-shot keys become commands for the simulation (none while typing in
  // a panel: the display gives keys to ImGui then)
  if (display_->keyPressed(Scene::keyFor('P'))) {
    send(GameCommand{GameCommand::Type::TogglePause});
  }
  if (display_->keyPressed(Scene::keyFor('R'))) {
    send(GameCommand{GameCommand::Type::Restart});
    smoothed_ = glm::vec2(0.0f);
    flashes_.clear();
    messages_.clear();
  }
  if (display_->keyPressed(Scene::keyFor('T'))) {
    send(GameCommand{GameCommand::Type::ToggleAutopilot});
  }
  if (display_->keyDown(Scene::Key::Escape)) {
    display_->close();
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

  drawWorld(static_cast<float>(now));
  drawHud();
  panels_.draw();
  renderer_->endFrame(); // waits for the screen's refresh

  // Steering for the next frame (the simulation ignores it on autopilot)
  auto key = [&](Scene::Key a, Scene::Key b) {
    return display_->keyDown(a) || display_->keyDown(b);
  };
  FrameInput next;
  next.seconds = frame;
  next.inputX = (key(Scene::keyFor('D'), Scene::Key::Right) ? 1.0f : 0.0f) -
                (key(Scene::keyFor('A'), Scene::Key::Left) ? 1.0f : 0.0f);
  next.inputY = (key(Scene::keyFor('W'), Scene::Key::Up) ? 1.0f : 0.0f) -
                (key(Scene::keyFor('S'), Scene::Key::Down) ? 1.0f : 0.0f);
  send(next);
}

void FlightDisplayPadd::drawWorld(float time) {
  Scene::Renderer &r = *renderer_;
  const WorldSnapshot &g = world_;
  const float z = static_cast<float>(g.z);
  const Scene::Camera camera{
      .eye = toScene({smoothed_.x * 0.55f, smoothed_.y * 0.45f + 2.0f, z + 7.5f}),
      .target = toScene({smoothed_.x * 0.8f, smoothed_.y * 0.7f + 0.6f, z - 14.0f}),
      .fieldOfView = 60,
      .farPlane = 400};
  // Lit from one direction both ways, so faces facing away are as bright as
  // faces facing it (0.35 + 0.65 |n . light|), fogged into the night sky
  const Scene::Vector3 light = Scene::Vector3{0.4f, 0.8f, 0.3f}.normalised();
  Scene::Environment environment{.background = toColour(kSkyColor),
                                 .ambient = Scene::Colour::fromUnit(0.35f, 0.35f, 0.35f),
                                 .fog = {.colour = toColour(kSkyColor), .density = 0.012f}};
  environment
      .addLight({.direction = -light, .colour = Scene::Colour::fromUnit(0.65f, 0.65f, 0.65f)})
      .addLight({.direction = light, .colour = Scene::Colour::fromUnit(0.65f, 0.65f, 0.65f)});
  r.beginFrame(camera, environment);
  setSnap(r, g.renderBits);

  static std::vector<Scene::Vector3> ground, posts;
  groundLines(g.z, ground, posts);
  r.drawLines(ground, toColour({0.08f, 0.35f, 0.5f}));
  r.drawLines(posts, toColour({0.45f, 0.2f, 0.6f}));

  const auto unlit = [](glm::vec3 colour, float alpha = 1.0f) {
    return Scene::Material{.shading = Scene::Material::Shading::unlit,
                           .colour = toColour(colour),
                           .alpha = static_cast<std::uint8_t>(std::lround(alpha * 31))};
  };
  for (const Obstacle &o : g.obstacles) {
    switch (o.kind) {
    case Kind::Gate:
      drawGate(r, meshes_.cube, o, flashed(o, {0.25f, 0.85f, 0.4f}));
      break;
    case Kind::GoldGate:
      drawGate(r, meshes_.cube, o, flashed(o, {1.0f, 0.78f, 0.15f}));
      break;
    case Kind::FocusPickup:
    case Kind::PhasePickup: {
      if (o.resolved) {
        break;
      }
      glm::mat4 m = glm::translate(glm::mat4(1.0f), {o.cx, o.cy, o.z});
      m = glm::rotate(m, time * 2.0f, glm::vec3(0, 1, 0));
      m = glm::scale(m, glm::vec3(0.6f, 0.8f, 0.6f));
      const glm::vec3 c =
          o.kind == Kind::FocusPickup ? glm::vec3(0.3f, 0.9f, 1.0f) : glm::vec3(1.0f, 0.3f, 0.9f);
      r.draw(meshes_.octahedron, toScene(m), {.colour = toColour(c)});
      break;
    }
    case Kind::PhaseWall: {
      // Translucent energy walls: the renderer draws them after everything
      // solid, farthest first
      const float pulse = 0.3f + 0.1f * std::sin(time * 6.0f + o.z);
      r.draw(meshes_.cube,
             box({0.0f, (kGroundY + kCeilingY) / 2, o.z},
                 {2 * kLaneHalfWidth + 3.0f, kCeilingY - kGroundY, 2 * kWallHalfZ}),
             unlit(flashed(o, {1.0f, 0.2f, 0.85f}), pulse));
      break;
    }
    }
  }

  // Ship, drawn where physics sees it (eased slightly so snapping reads as
  // motion rather than teleporting)
  glm::mat4 ship = glm::translate(glm::mat4(1.0f), {smoothed_.x, smoothed_.y, z});
  ship = glm::rotate(ship, std::clamp(-g.vx * 0.06f, -0.7f, 0.7f), {0, 0, 1});
  ship = glm::rotate(ship, std::clamp(g.vy * 0.03f, -0.4f, 0.4f), {1, 0, 0});
  r.draw(meshes_.ship, toScene(ship),
         {.colour = toColour(g.phase.active() ? glm::vec3(0.85f, 0.55f, 0.95f)
                                              : glm::vec3(0.75f, 0.8f, 0.9f))});
  r.draw(
      meshes_.cube,
      toScene(glm::scale(glm::translate(ship, {0.0f, 0.05f, 0.7f}), glm::vec3(0.35f, 0.18f, 0.1f))),
      unlit({1.0f, 0.55f + 0.2f * std::sin(time * 30.0f), 0.1f}));

  // Physics hitbox at the exact observed position, without render snapping
  if (showHitbox_) {
    Scene::Desktop::setVertexSnap(r, 0, 0);
    Scene::Material wire = unlit({1.0f, 1.0f, 0.3f});
    wire.wireframe = true;
    r.draw(meshes_.cube,
           box({g.observed.x, g.observed.y, g.observedZ},
               {2 * kShipHalfX, 2 * kShipHalfY, 2 * kShipHalfZ}),
           wire);
    setSnap(r, g.renderBits);
  }
}

void FlightDisplayPadd::drawHud() {
  const WorldSnapshot &g = world_;
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.75f);
  Theme::beginWindow("Precision Flight", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

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
