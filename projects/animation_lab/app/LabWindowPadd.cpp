#include "LabWindowPadd.hpp"

#include <ScratchPadd/Storage/Folders.hpp>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace AnimationLab {

namespace Scene = ScratchPadd::Scene;

namespace {

// The window's columns and the plots' height (points)
constexpr float kHistoryWidth = 260, kMakingWidth = 380, kPlotsHeight = 330;
// How long settings stay still before they're a run in the history
constexpr double kSecondsToNote = 0.8;
// The pause at the end of each loop
constexpr double kSecondsBetween = 0.6;
constexpr std::size_t kMostRuns = 300;

const Scene::Colour kNowColour{235, 235, 240}, kOtherColour{255, 170, 90};
const Scene::Colour kNowPath{110, 200, 255}, kOtherPath{255, 150, 60}, kFloor{60, 66, 80};

Scene::Vector3 vectorOf(const Point &p) { return {p[0], p[1], p[2]}; }

// A rotation (w, x, y, z) as the transform turning by it, at `at`
Scene::Transform turned(const ScratchPadd::Motion::BakedMotion::Turn &q, const Scene::Vector3 &at) {
  const float w = q[0], x = q[1], y = q[2], z = q[3];
  return Scene::Transform::fromAxes(
      {1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y)},
      {2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)},
      {2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)}, at);
}

// Where a run is at `t` seconds, and how it's turned: the bake, or the
// physics as recorded
struct Pose {
  Point at{};
  ScratchPadd::Motion::BakedMotion::Turn turn{1, 0, 0, 0};
};
Pose poseAt(const Made &made, double t, bool physics) {
  const double seconds = made.recording.seconds();
  t = std::clamp(t, 0.0, seconds);
  if (!physics || made.recording.positions.empty())
    return {made.baked.at(static_cast<float>(t)), made.baked.turnAt(static_cast<float>(t))};
  const auto i = std::min(static_cast<std::size_t>(std::lround(t / made.recording.step)),
                          made.recording.positions.size() - 1);
  Pose pose{made.recording.positions[i]};
  if (i < made.recording.turns.size())
    pose.turn = made.recording.turns[i];
  return pose;
}

// A point's three numbers as one row of drag fields (z kept 0 when flat)
bool dragPoint(const char *label, Point &p, bool flat) {
  const bool changed = ImGui::DragFloat3(label, p.data(), 0.05f);
  if (flat)
    p[2] = 0;
  return changed;
}
bool dragNumber(const char *label, double &v, float speed) {
  return ImGui::DragScalar(label, ImGuiDataType_Double, &v, speed);
}

} // namespace

void LabWindowPadd::starting() {
  display_ = std::make_unique<Scene::Display>(1500, 920, "Animation lab");
  if (!display_->isOpen()) {
    logger().error("No window: {}", display_->error());
    system_->stop();
    return;
  }
  renderer_ = std::make_unique<Scene::Renderer>(*display_);
  ui_ = std::make_unique<ScratchPadd::DebugUi::Ui>(*display_);
  folder_ = ScratchPadd::Storage::Folders::forApp().files;
  shapes_ = labShapes();
  for (const LabShape &shape : shapes_) {
    meshes_.push_back(renderer_->load(shape.mesh()));
    if (!shape.picture.empty())
      cardPicture_ = renderer_->load(
          Scene::TextureImage{shape.pictureWidth, shape.pictureHeight, shape.picture});
  }
  readHistory();
  if (!history_.empty()) {
    chosen_ = static_cast<int>(history_.size()) - 1;
    settings_ = history_.back().settings;
  }
  remake();
  unnoted_ = false;
  push([this] { drawFrame(); });
}

void LabWindowPadd::finishing() {
  if (unnoted_)
    noteRun();
  ui_.reset();
  renderer_.reset();
  display_.reset();
}

LabWindowPadd::Playing LabWindowPadd::play(const Settings &settings) const {
  Playing playing{.settings = settings};
  for (const LabShape &shape : shapes_)
    if (shape.name == settings.shape)
      playing.shape = &shape;
  if (!playing.shape && !shapes_.empty())
    playing.shape = &shapes_.front();
  playing.made =
      make(settings, playing.shape ? playing.shape->corners(settings.size) : std::vector<Point>{});
  return playing;
}

void LabWindowPadd::remake() {
  now_ = play(settings_);
  unnoted_ = true;
  changedAt_ = Clock::now();
  playFrom_ = Clock::now();
}

void LabWindowPadd::noteRun() {
  unnoted_ = false;
  const std::string text = writeSettings(settings_);
  if (chosen_ >= 0 && chosen_ < static_cast<int>(history_.size()) &&
      writeSettings(history_[static_cast<std::size_t>(chosen_)].settings) == text)
    return; // (as it was)
  if (!history_.empty() && writeSettings(history_.back().settings) == text) {
    chosen_ = static_cast<int>(history_.size()) - 1;
    return;
  }
  history_.push_back({.number = nextNumber_++, .settings = settings_});
  if (history_.size() > kMostRuns)
    history_.erase(history_.begin());
  chosen_ = static_cast<int>(history_.size()) - 1;
  writeHistory();
}

// history.txt: each run as "run NUMBER NAME", its settings, then "end"
void LabWindowPadd::readHistory() {
  std::ifstream in(folder_ + "history.txt");
  std::string line, block;
  Run run;
  bool inRun = false;
  while (std::getline(in, line)) {
    if (line.rfind("run ", 0) == 0) {
      std::istringstream words(line.substr(4));
      run = {};
      words >> run.number;
      std::getline(words >> std::ws, run.name);
      block.clear();
      inRun = true;
    } else if (line == "end" && inRun) {
      if (const auto settings = readSettings(block)) {
        run.settings = *settings;
        history_.push_back(run);
        nextNumber_ = std::max(nextNumber_, run.number + 1);
      }
      inRun = false;
    } else if (inRun) {
      block += line + "\n";
    }
  }
}

void LabWindowPadd::writeHistory() const {
  ScratchPadd::Storage::makeFolder(folder_);
  std::ofstream out(folder_ + "history.txt");
  for (const Run &run : history_)
    out << "run " << run.number << " " << run.name << "\n"
        << writeSettings(run.settings) << "end\n";
}

void LabWindowPadd::save() {
  const std::string folder = folder_ + "motions/";
  ScratchPadd::Storage::makeFolder(folder);
  const std::string path = folder + name_ + ".txt";
  std::ofstream out(path);
  out << writeText(now_.made.baked);
  saved_ = out ? "saved to " + path : "couldn't write " + path;
}

double LabWindowPadd::playTime() const {
  double seconds = now_.made.recording.seconds();
  if (other_)
    seconds = std::max(seconds, other_->made.recording.seconds());
  const double since = std::chrono::duration<double>(Clock::now() - playFrom_).count();
  return seconds > 0 ? std::fmod(since, seconds + kSecondsBetween) : 0;
}

void LabWindowPadd::drawFrame() {
  if (!display_->isOpen()) {
    system_->stop();
    return;
  }
  display_->pollEvents();
  ui_->newFrame();
  // (settings still a moment, and nothing held: a run)
  if (unnoted_ && !ImGui::IsAnyItemActive() &&
      std::chrono::duration<double>(Clock::now() - changedAt_).count() > kSecondsToNote)
    noteRun();
  const double t = playTime();
  drawView(t);
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  constexpr ImGuiWindowFlags kFixed =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(kHistoryWidth, size.y));
  ImGui::Begin("History", nullptr, kFixed);
  drawHistory();
  ImGui::End();
  ImGui::SetNextWindowPos(ImVec2(kHistoryWidth, 0));
  ImGui::SetNextWindowSize(ImVec2(kMakingWidth, size.y));
  ImGui::Begin("Making", nullptr, kFixed);
  drawMaking();
  ImGui::End();
  ImGui::SetNextWindowPos(ImVec2(kHistoryWidth + kMakingWidth, size.y - kPlotsHeight));
  ImGui::SetNextWindowSize(ImVec2(size.x - kHistoryWidth - kMakingWidth, kPlotsHeight));
  ImGui::Begin("Plots", nullptr, kFixed);
  drawPlots(t);
  ImGui::End();
  renderer_->endFrame();
  push([this] { drawFrame(); });
}

void LabWindowPadd::drawHistory() {
  ImGui::TextWrapped("Each run, once its settings have been still a moment. Click to go back to "
                     "one; vs plays it beside this one.");
  if (other_) {
    ImGui::Text("Comparing with #%d", otherNumber_);
    ImGui::SameLine();
    if (ImGui::SmallButton("stop")) {
      other_.reset();
      otherNumber_ = 0;
    }
    ImGui::Checkbox("side by side (else on top)", &sideBySide_);
  }
  ImGui::Separator();
  ImGui::BeginChild("runs", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 3));
  for (int i = static_cast<int>(history_.size()) - 1; i >= 0; --i) {
    Run &run = history_[static_cast<std::size_t>(i)];
    ImGui::PushID(run.number);
    const bool comparing = other_ && otherNumber_ == run.number;
    if (ImGui::SmallButton(comparing ? "vs*" : "vs")) {
      other_ = play(run.settings);
      otherNumber_ = run.number;
      playFrom_ = Clock::now();
    }
    ImGui::SameLine();
    const std::string label = "#" + std::to_string(run.number) + " " +
                              (run.name.empty() ? summary(run.settings) : run.name);
    if (ImGui::Selectable(label.c_str(), chosen_ == i)) {
      if (unnoted_)
        noteRun();
      chosen_ = i;
      settings_ = run.settings;
      remake();
      unnoted_ = false;
    }
    if (ImGui::IsItemHovered() && !run.name.empty())
      ImGui::SetTooltip("%s", summary(run.settings).c_str());
    ImGui::PopID();
  }
  ImGui::EndChild();
  if (chosen_ >= 0 && chosen_ < static_cast<int>(history_.size())) {
    Run &run = history_[static_cast<std::size_t>(chosen_)];
    if (ImGui::InputTextWithHint("##name", "name this run", &run.name,
                                 ImGuiInputTextFlags_EnterReturnsTrue) ||
        ImGui::IsItemDeactivatedAfterEdit())
      writeHistory();
    if (ImGui::Button("Delete this run")) {
      if (otherNumber_ == run.number) {
        other_.reset();
        otherNumber_ = 0;
      }
      history_.erase(history_.begin() + chosen_);
      chosen_ = -1;
      writeHistory();
    }
  }
}

void LabWindowPadd::drawMaking() {
  Settings &s = settings_;
  bool changed = false;
  ImGui::TextUnformatted("Make it by");
  int way = static_cast<int>(s.way);
  for (const Way w : {Way::Throw, Way::Guide, Way::Search, Way::Tumble}) {
    if (w != Way::Throw)
      ImGui::SameLine();
    changed |= ImGui::RadioButton(std::string(nameOf(w)).c_str(), &way, static_cast<int>(w));
  }
  s.way = static_cast<Way>(way);
  changed |= ImGui::Checkbox("Flat (2D: z stays 0)", &s.flat);
  if (ImGui::BeginCombo("shape", s.shape.c_str())) {
    for (const LabShape &shape : shapes_)
      if (ImGui::Selectable(shape.name.c_str(), shape.name == s.shape)) {
        s.shape = shape.name;
        changed = true;
      }
    ImGui::EndCombo();
  }
  changed |= ImGui::SliderFloat("size", &s.size, 0.2f, 3);
  ImGui::Separator();
  switch (s.way) {
  case Way::Throw:
    ImGui::TextWrapped("Where it starts and lands and how long it takes: the throw is worked "
                       "out, then flown.");
    changed |= dragPoint("from", s.throwing.from, s.flat);
    changed |= dragPoint("to", s.throwing.to, s.flat);
    changed |= dragNumber("seconds", s.throwing.seconds, 0.01f);
    changed |= dragNumber("gravity", s.throwing.gravity, 0.1f);
    break;
  case Way::Guide:
    ImGui::TextWrapped("A rough path as keys: a spring pulls it after them, so it lags, "
                       "overshoots and settles.");
    for (std::size_t i = 0; i < s.guide.keys.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      changed |= dragNumber("when", s.guide.keys[i].time, 0.01f);
      changed |= dragPoint("where", s.guide.keys[i].at, s.flat);
      ImGui::PopID();
    }
    if (ImGui::Button("Add a key")) {
      const Key last = s.guide.keys.back();
      s.guide.keys.push_back({last.time + 0.3, {last.at[0] + 1, last.at[1], last.at[2]}});
      changed = true;
    }
    ImGui::SameLine();
    if (s.guide.keys.size() > 2 && ImGui::Button("Take the last away")) {
      s.guide.keys.pop_back();
      changed = true;
    }
    changed |= dragNumber("stiffness", s.guide.stiffness, 1);
    changed |= dragNumber("damping", s.guide.damping, 0.1f);
    changed |= dragNumber("settle (s)", s.guide.settle, 0.05f);
    break;
  case Way::Search:
    ImGui::TextWrapped("No formula (air drag, a bounce): throws are tried until one lands "
                       "where it should.");
    changed |= dragPoint("from", s.search.goal.from, s.flat);
    changed |= dragPoint("to", s.search.goal.to, s.flat);
    changed |= dragNumber("seconds", s.search.goal.seconds, 0.01f);
    changed |= dragNumber("drag (kept a second)", s.search.drag, 0.01f);
    changed |= dragNumber("floor", s.search.floor, 0.05f);
    changed |= dragNumber("bounce", s.search.bounce, 0.01f);
    ImGui::Text("missed by %.3f", now_.made.miss);
    break;
  case Way::Tumble:
    ImGui::TextWrapped("The shape thrown spinning onto the floor: it bounces and rolls to rest "
                       "on its own shape (its hull), turning as it goes.");
    changed |= dragPoint("from", s.tumble.goal.from, s.flat);
    changed |= dragPoint("aim at", s.tumble.goal.to, s.flat);
    changed |= dragNumber("seconds", s.tumble.goal.seconds, 0.01f);
    changed |= dragNumber("gravity", s.tumble.goal.gravity, 0.1f);
    changed |= ImGui::DragFloat3("spin (x y z)", s.tumble.spin.data(), 0.1f);
    changed |= dragNumber("floor", s.tumble.floor, 0.05f);
    changed |= dragNumber("bounce", s.tumble.bounce, 0.01f);
    changed |= dragNumber("friction", s.tumble.friction, 0.01f);
    changed |= dragNumber("settle (s)", s.tumble.settle, 0.05f);
    ImGui::Text("rests after %.2f s", now_.made.recording.seconds());
    break;
  }
  ImGui::Separator();
  ImGui::TextUnformatted("Bake");
  changed |= ImGui::SliderInt("path points", &s.pathPoints, 2, 64);
  changed |= ImGui::SliderInt("timing points", &s.timingPoints, 2, 64);
  const auto &baked = now_.made.baked;
  ImGui::Text("strays at most %.3f from the physics", now_.made.error);
  ImGui::Text("%zu floats to play",
              baked.path.size() * 3 + baked.timing.size() + baked.turns.size() * 4 + 1);
  if (changed)
    remake();
  if (ImGui::Button("Play again"))
    playFrom_ = Clock::now();
  ImGui::SameLine();
  ImGui::Checkbox("the physics, not the bake", &showPhysics_);
  ImGui::Separator();
  ImGui::InputText("name", &name_);
  if (ImGui::Button("Save"))
    save();
  if (!saved_.empty())
    ImGui::TextWrapped("%s", saved_.c_str());
}

void LabWindowPadd::drawView(double t) {
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  const float left = kHistoryWidth + kMakingWidth, bottom = size.y - kPlotsHeight;
  // Turned by dragging, zoomed by scrolling, where no panel is
  const ImGuiIO &io = ImGui::GetIO();
  if (!io.WantCaptureMouse && io.MousePos.x > left && io.MousePos.y < bottom) {
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      yaw_ += io.MouseDelta.x * 0.008f;
      pitch_ = std::clamp(pitch_ + io.MouseDelta.y * 0.008f, -0.2f, 1.45f);
    }
    zoom_ = std::clamp(zoom_ * std::pow(0.9f, io.MouseWheel), 0.2f, 5.0f);
  }
  // The runs: this one, and the one compared beside it (or on top)
  struct Shown {
    const Playing *playing;
    Scene::Vector3 offset;
    Scene::Colour colour, path;
  };
  std::vector<Shown> shown{{&now_, {}, kNowColour, kNowPath}};
  if (other_) {
    float apart = 0;
    if (sideBySide_) {
      float lowZ = 1e9f, highZ = -1e9f;
      for (const Playing *p : {&now_, &*other_})
        for (const Point &q : p->made.recording.positions) {
          lowZ = std::min(lowZ, q[2]);
          highZ = std::max(highZ, q[2]);
        }
      apart = (highZ - lowZ) + std::max(now_.settings.size, other_->settings.size) * 2 + 1;
    }
    shown.push_back({&*other_, {0, 0, -apart}, kOtherColour, kOtherPath});
  }
  // The camera: around the middle of everything shown, far enough back to
  // see it all, moved sideways so it's in the middle of the clear space
  Scene::Vector3 low{1e9f, 1e9f, 1e9f}, high{-1e9f, -1e9f, -1e9f};
  for (const Shown &s : shown)
    for (const Point &q : s.playing->made.recording.positions) {
      const Scene::Vector3 p = vectorOf(q) + s.offset;
      low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
      high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
    }
  if (low.x > high.x)
    low = high = {};
  const Scene::Vector3 middle = (low + high) * 0.5f;
  const float radius = (high - low).length() * 0.5f + 1.5f;
  Scene::Camera camera;
  camera.fieldOfView = 40;
  const float tanHalf = std::tan(camera.fieldOfView * 3.14159265f / 360);
  const float aspect = size.x / size.y;
  const float shareHigh = bottom / size.y, shareWide = (size.x - left) / size.x;
  const float distance = radius / (tanHalf * std::min(shareHigh, shareWide * aspect)) * zoom_;
  const Scene::Vector3 toEye{std::cos(pitch_) * std::sin(yaw_), std::sin(pitch_),
                             std::cos(pitch_) * std::cos(yaw_)};
  const Scene::Vector3 right = Scene::Vector3{0, 1, 0}.cross(toEye).normalised();
  const Scene::Vector3 up = toEye.cross(right);
  // (the clear space's middle, -1 to 1 across and up the window)
  const float cx = (left + size.x) / size.x - 1, cy = 1 - bottom / size.y;
  const Scene::Vector3 shift =
      right * (-cx * distance * tanHalf * aspect) + up * (-cy * distance * tanHalf);
  camera.target = middle + shift;
  camera.eye = middle + toEye * distance + shift;
  camera.farPlane = distance + radius * 4 + 50;
  Scene::Environment environment;
  environment.background = {24, 26, 32};
  environment.ambient = {90, 90, 100};
  environment.addLight({Scene::Vector3{-0.4f, -1, -0.3f}.normalised(), {255, 250, 240}});
  renderer_->beginFrame(camera, environment);
  // The floor, a grid under it all
  {
    const auto floorOf = [](const Settings &s) {
      return s.way == Way::Tumble   ? static_cast<float>(s.tumble.floor)
             : s.way == Way::Search ? static_cast<float>(s.search.floor)
                                    : 0.0f;
    };
    const float y = floorOf(now_.settings);
    std::vector<Scene::Vector3> ends;
    const float x0 = std::floor(low.x) - 3, x1 = std::ceil(high.x) + 3;
    const float z0 = std::floor(low.z) - 3, z1 = std::ceil(high.z) + 3;
    for (float x = x0; x <= x1; x += 1)
      ends.insert(ends.end(), {{x, y, z0}, {x, y, z1}});
    for (float z = z0; z <= z1; z += 1)
      ends.insert(ends.end(), {{x0, y, z}, {x1, y, z}});
    renderer_->drawLines(ends, kFloor);
  }
  for (const Shown &s : shown) {
    const Playing &p = *s.playing;
    std::vector<Scene::Vector3> ends;
    const auto &positions = p.made.recording.positions;
    for (std::size_t i = 1; i < positions.size(); ++i)
      ends.insert(ends.end(),
                  {vectorOf(positions[i - 1]) + s.offset, vectorOf(positions[i]) + s.offset});
    renderer_->drawLines(ends, s.path);
    if (!p.shape)
      continue;
    const Pose pose = poseAt(p.made, t, showPhysics_);
    const Scene::Transform where =
        turned(pose.turn, vectorOf(pose.at) + s.offset) *
        Scene::Transform::scaling({p.settings.size, p.settings.size, p.settings.size}) *
        p.shape->fit;
    Scene::Material material;
    material.colour = s.colour;
    if (!p.shape->picture.empty())
      material.texture = cardPicture_;
    const auto index = static_cast<std::size_t>(p.shape - shapes_.data());
    renderer_->draw(meshes_[index], where, material);
  }
}

void LabWindowPadd::drawPlots(double t) {
  struct Series {
    const Made *made;
    const char *physics, *baked, *now;
  };
  std::vector<Series> series{{&now_.made, "physics", "baked", "now"}};
  if (other_)
    series.push_back({&other_->made, "vs physics", "vs baked", "vs now"});
  const float width = ImGui::GetContentRegionAvail().x / 3 - 6;
  const float height = ImGui::GetContentRegionAvail().y;
  const auto view = [&](const char *title, int a, int b, const char *xLabel, const char *yLabel) {
    if (!ImPlot::BeginPlot(title, ImVec2(width, height), ImPlotFlags_Equal))
      return;
    ImPlot::SetupAxes(xLabel, yLabel);
    const auto ai = static_cast<std::size_t>(a), bi = static_cast<std::size_t>(b);
    for (const Series &s : series) {
      std::vector<float> xs, ys;
      for (const Point &p : s.made->recording.positions) {
        xs.push_back(p[ai]);
        ys.push_back(p[bi]);
      }
      ImPlot::PlotLine(s.physics, xs.data(), ys.data(), static_cast<int>(xs.size()));
      xs.clear();
      ys.clear();
      for (const Point &p : s.made->baked.path) {
        xs.push_back(p[ai]);
        ys.push_back(p[bi]);
      }
      ImPlot::PlotScatter(
          s.baked, xs.data(), ys.data(), static_cast<int>(xs.size()),
          ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Circle, ImPlotProp_MarkerSize, 3.0f));
      const Point now = poseAt(*s.made, t, showPhysics_).at;
      ImPlot::PlotScatter(
          s.now, &now[ai], &now[bi], 1,
          ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Diamond, ImPlotProp_MarkerSize, 7.0f));
    }
    ImPlot::EndPlot();
  };
  view("From the side", 0, 1, "x", "y (up)");
  ImGui::SameLine();
  view("From above", 0, 2, "x", "z");
  ImGui::SameLine();
  // The timing: how far along, against time
  if (ImPlot::BeginPlot("Timing", ImVec2(width, height))) {
    ImPlot::SetupAxes("seconds", "how far along");
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
    for (const Series &s : series) {
      const Recording &r = s.made->recording;
      std::vector<double> ts, along;
      double came = 0, total = 0;
      for (std::size_t i = 1; i < r.positions.size(); ++i) {
        const Point &p = r.positions[i - 1], &q = r.positions[i];
        total += std::hypot(double(q[0]) - p[0], double(q[1]) - p[1], double(q[2]) - p[2]);
      }
      for (std::size_t i = 0; i < r.positions.size(); ++i) {
        if (i > 0) {
          const Point &p = r.positions[i - 1], &q = r.positions[i];
          came += std::hypot(double(q[0]) - p[0], double(q[1]) - p[1], double(q[2]) - p[2]);
        }
        ts.push_back(r.step * static_cast<double>(i));
        along.push_back(total > 0 ? came / total : 0);
      }
      ImPlot::PlotLine(s.physics, ts.data(), along.data(), static_cast<int>(ts.size()));
      const auto &timing = s.made->baked.timing;
      std::vector<double> bt, ba;
      for (std::size_t m = 0; m < timing.size(); ++m) {
        bt.push_back(r.seconds() * static_cast<double>(m) /
                     static_cast<double>(std::max<std::size_t>(timing.size() - 1, 1)));
        ba.push_back(timing[m]);
      }
      ImPlot::PlotScatter(
          s.baked, bt.data(), ba.data(), static_cast<int>(bt.size()),
          ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Circle, ImPlotProp_MarkerSize, 3.0f));
    }
    ImPlot::PlotInfLines("now", &t, 1);
    ImPlot::EndPlot();
  }
}

} // namespace AnimationLab
