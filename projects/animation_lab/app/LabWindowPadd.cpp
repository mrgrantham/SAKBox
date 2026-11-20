#include "LabWindowPadd.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "implot.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

namespace AnimationLab {

namespace Scene = ScratchPadd::Scene;

void LabWindowPadd::starting() {
  display_ = std::make_unique<Scene::Display>(1280, 820, "Animation lab");
  if (!display_->isOpen()) {
    logger().error("No window: {}", display_->error());
    system_->stop();
    return;
  }
  renderer_ = std::make_unique<Scene::Renderer>(*display_);
  ui_ = std::make_unique<ScratchPadd::DebugUi::Ui>(*display_);
  remake();
  push([this] { drawFrame(); });
}

void LabWindowPadd::finishing() {
  ui_.reset();
  renderer_.reset();
  display_.reset();
}

void LabWindowPadd::remake() {
  miss_ = 0;
  switch (way_) {
  case Way::Throw: {
    Throw goal = throw_;
    goal.flat = flat_;
    made_ = solveThrow(goal);
    break;
  }
  case Way::Guide: {
    Guide guide = guide_;
    guide.flat = flat_;
    made_ = guideAlong(guide);
    break;
  }
  case Way::Search: {
    Search search = search_;
    search.goal.flat = flat_;
    const Found found = searchThrow(search);
    made_ = found.recording;
    miss_ = found.miss;
    break;
  }
  }
  baked_ =
      bake(made_, static_cast<std::size_t>(pathPoints_), static_cast<std::size_t>(timingPoints_));
  error_ = bakeError(made_, baked_);
  playFrom_ = std::chrono::steady_clock::now();
}

void LabWindowPadd::save() {
  std::filesystem::create_directories("animation_lab_motions");
  const std::string path = "animation_lab_motions/" + name_ + ".txt";
  std::ofstream out(path);
  out << writeText(baked_);
  saved_ = out ? "saved to " + path : "couldn't write " + path;
}

void LabWindowPadd::drawFrame() {
  if (!display_->isOpen()) {
    system_->stop();
    return;
  }
  display_->pollEvents();
  ui_->newFrame();
  Scene::Environment environment;
  environment.background = {24, 26, 32};
  renderer_->beginFrame(Scene::Camera{}, environment);
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(size);
  ImGui::Begin("Animation lab", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
  ImGui::BeginChild("making", ImVec2(380, 0), ImGuiChildFlags_Borders);
  drawMaking();
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("seeing", ImVec2(0, 0));
  drawSeeing();
  ImGui::EndChild();
  ImGui::End();
  renderer_->endFrame();
  push([this] { drawFrame(); });
}

namespace {
// A point's three numbers as one row of drag fields (z hidden when flat)
bool dragPoint(const char *label, Point &p, bool flat) {
  return ImGui::DragFloat3(label, p.data(), 0.05f) && (!flat || (p[2] = 0, true));
}
} // namespace

void LabWindowPadd::drawMaking() {
  bool changed = false;
  ImGui::TextUnformatted("Make it by");
  int way = static_cast<int>(way_);
  changed |= ImGui::RadioButton("Throw", &way, 0);
  ImGui::SameLine();
  changed |= ImGui::RadioButton("Guide", &way, 1);
  ImGui::SameLine();
  changed |= ImGui::RadioButton("Search", &way, 2);
  way_ = static_cast<Way>(way);
  changed |= ImGui::Checkbox("Flat (2D: z stays 0)", &flat_);
  ImGui::Separator();
  switch (way_) {
  case Way::Throw:
    ImGui::TextWrapped("Where it starts and lands and how long it takes: the throw is worked "
                       "out, then flown.");
    changed |= dragPoint("from", throw_.from, flat_);
    changed |= dragPoint("to", throw_.to, flat_);
    changed |= ImGui::DragScalar("seconds", ImGuiDataType_Double, &throw_.seconds, 0.01f);
    changed |= ImGui::DragScalar("gravity", ImGuiDataType_Double, &throw_.gravity, 0.1f);
    break;
  case Way::Guide:
    ImGui::TextWrapped("A rough path as keys: a spring pulls it after them, so it lags, "
                       "overshoots and settles.");
    for (std::size_t i = 0; i < guide_.keys.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      changed |= ImGui::DragScalar("when", ImGuiDataType_Double, &guide_.keys[i].time, 0.01f);
      changed |= dragPoint("where", guide_.keys[i].at, flat_);
      ImGui::PopID();
    }
    if (ImGui::Button("Add a key")) {
      const Key last = guide_.keys.back();
      guide_.keys.push_back({last.time + 0.3, {last.at[0] + 1, last.at[1], last.at[2]}});
      changed = true;
    }
    ImGui::SameLine();
    if (guide_.keys.size() > 2 && ImGui::Button("Take the last away")) {
      guide_.keys.pop_back();
      changed = true;
    }
    changed |= ImGui::DragScalar("stiffness", ImGuiDataType_Double, &guide_.stiffness, 1);
    changed |= ImGui::DragScalar("damping", ImGuiDataType_Double, &guide_.damping, 0.1f);
    changed |= ImGui::DragScalar("settle (s)", ImGuiDataType_Double, &guide_.settle, 0.05f);
    break;
  case Way::Search:
    ImGui::TextWrapped("No formula (air drag, a bounce): throws are tried until one lands "
                       "where it should.");
    changed |= dragPoint("from", search_.goal.from, flat_);
    changed |= dragPoint("to", search_.goal.to, flat_);
    changed |= ImGui::DragScalar("seconds", ImGuiDataType_Double, &search_.goal.seconds, 0.01f);
    changed |=
        ImGui::DragScalar("drag (kept a second)", ImGuiDataType_Double, &search_.drag, 0.01f);
    changed |= ImGui::DragScalar("floor", ImGuiDataType_Double, &search_.floor, 0.05f);
    changed |= ImGui::DragScalar("bounce", ImGuiDataType_Double, &search_.bounce, 0.01f);
    ImGui::Text("missed by %.3f", miss_);
    break;
  }
  ImGui::Separator();
  ImGui::TextUnformatted("Bake");
  changed |= ImGui::SliderInt("path points", &pathPoints_, 2, 64);
  changed |= ImGui::SliderInt("timing points", &timingPoints_, 2, 64);
  ImGui::Text("strays at most %.3f from the physics", error_);
  ImGui::Text("%zu floats to play", baked_.path.size() * 3 + baked_.timing.size() + 1);
  if (changed)
    remake();
  if (ImGui::Button("Play again"))
    playFrom_ = std::chrono::steady_clock::now();
  ImGui::Separator();
  ImGui::InputText("name", &name_);
  if (ImGui::Button("Save"))
    save();
  if (!saved_.empty())
    ImGui::TextWrapped("%s", saved_.c_str());
}

void LabWindowPadd::drawSeeing() {
  // Where it is now, played back on a loop (a pause between)
  const double seconds = made_.seconds();
  const double since =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - playFrom_).count();
  const double t = seconds > 0 ? std::fmod(since, seconds + 0.6) : 0;
  const auto nowRecorded = static_cast<std::size_t>(std::lround(std::min(t, seconds) / made_.step));
  const Point recorded = made_.positions.empty()
                             ? Point{}
                             : made_.positions[std::min(nowRecorded, made_.positions.size() - 1)];
  const Point baked = baked_.at(static_cast<float>(std::min(t, seconds)));
  // A view of the motion: two of its axes
  const auto view = [&](const char *title, int a, int b, const char *xLabel, const char *yLabel) {
    if (!ImPlot::BeginPlot(title, ImVec2(-1, 300), ImPlotFlags_Equal))
      return;
    ImPlot::SetupAxes(xLabel, yLabel);
    std::vector<float> xs, ys;
    for (const Point &p : made_.positions) {
      xs.push_back(p[static_cast<std::size_t>(a)]);
      ys.push_back(p[static_cast<std::size_t>(b)]);
    }
    ImPlot::PlotLine("physics", xs.data(), ys.data(), static_cast<int>(xs.size()));
    xs.clear();
    ys.clear();
    for (const Point &p : baked_.path) {
      xs.push_back(p[static_cast<std::size_t>(a)]);
      ys.push_back(p[static_cast<std::size_t>(b)]);
    }
    ImPlot::PlotScatter(
        "baked path", xs.data(), ys.data(), static_cast<int>(xs.size()),
        ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Circle, ImPlotProp_MarkerSize, 3.0f));
    const float rx = recorded[static_cast<std::size_t>(a)],
                ry = recorded[static_cast<std::size_t>(b)];
    const float bx = baked[static_cast<std::size_t>(a)], by = baked[static_cast<std::size_t>(b)];
    ImPlot::PlotScatter(
        "physics now", &rx, &ry, 1,
        ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Square, ImPlotProp_MarkerSize, 7.0f));
    ImPlot::PlotScatter(
        "baked now", &bx, &by, 1,
        ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Diamond, ImPlotProp_MarkerSize, 7.0f));
    ImPlot::EndPlot();
  };
  view("From the side", 0, 1, "x", "y (up)");
  if (!flat_)
    view("From above", 0, 2, "x", "z");
  // The timing: how far along, against time
  if (ImPlot::BeginPlot("Timing", ImVec2(-1, 220))) {
    ImPlot::SetupAxes("seconds", "how far along");
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
    std::vector<double> ts, along;
    double came = 0, total = 0;
    for (std::size_t i = 1; i < made_.positions.size(); ++i) {
      const Point &p = made_.positions[i - 1], &q = made_.positions[i];
      total += std::hypot(double(q[0]) - p[0], double(q[1]) - p[1], double(q[2]) - p[2]);
    }
    for (std::size_t i = 0; i < made_.positions.size(); ++i) {
      if (i > 0) {
        const Point &p = made_.positions[i - 1], &q = made_.positions[i];
        came += std::hypot(double(q[0]) - p[0], double(q[1]) - p[1], double(q[2]) - p[2]);
      }
      ts.push_back(made_.step * static_cast<double>(i));
      along.push_back(total > 0 ? came / total : 0);
    }
    ImPlot::PlotLine("physics", ts.data(), along.data(), static_cast<int>(ts.size()));
    std::vector<double> bt, ba;
    for (std::size_t m = 0; m < baked_.timing.size(); ++m) {
      bt.push_back(seconds * static_cast<double>(m) /
                   static_cast<double>(baked_.timing.size() - 1));
      ba.push_back(baked_.timing[m]);
    }
    ImPlot::PlotScatter(
        "baked timing", bt.data(), ba.data(), static_cast<int>(bt.size()),
        ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Circle, ImPlotProp_MarkerSize, 3.0f));
    const double now = std::min(t, seconds);
    ImPlot::PlotInfLines("now", &now, 1);
    ImPlot::EndPlot();
  }
}

} // namespace AnimationLab
