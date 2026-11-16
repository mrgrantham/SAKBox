#include "LabWindowPadd.hpp"

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <utility>

namespace CosmicHex::Lab {

namespace {

// Where results go: balance_lab_runs/ in the repo when run with bazel run
// (BUILD_WORKSPACE_DIRECTORY), or the working directory
std::filesystem::path runsFolder() {
  const char *workspace = std::getenv("BUILD_WORKSPACE_DIRECTORY");
  return std::filesystem::path(workspace ? workspace : ".") / "balance_lab_runs";
}

// "20261116-031500"
std::string timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  char text[32];
  std::strftime(text, sizeof text, "%Y%m%d-%H%M%S", &local);
  return text;
}

// Bars of `values` (one a category, labelled), with the fair share and its
// margin drawn across, in a plot `title`
void sharesChart(const char *title, const std::vector<double> &values,
                 const std::vector<const char *> &labels, double fair, double margin) {
  if (!ImPlot::BeginPlot(title, ImVec2(-1, 230),
                         ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText))
    return;
  const int count = static_cast<int>(values.size());
  ImPlot::SetupAxes(nullptr, "% of wins", ImPlotAxisFlags_None, ImPlotAxisFlags_None);
  ImPlot::SetupAxisTicks(ImAxis_X1, 0, count - 1, count, labels.data());
  ImPlot::SetupAxisLimits(ImAxis_X1, -0.6, count - 0.4, ImPlotCond_Always);
  const double top = std::max(fair * 2, *std::max_element(values.begin(), values.end()) + 5);
  ImPlot::SetupAxisLimits(ImAxis_Y1, 0, top, ImPlotCond_Always);
  ImPlot::PlotBars("wins", values.data(), count, 0.6);
  // The fair share, and the band a fair game's result falls in 95% of the time
  const double fairLine[] = {fair};
  ImPlot::PlotInfLines("fair share", fairLine, 1,
                       ImPlotSpec(ImPlotProp_Flags, ImPlotInfLinesFlags_Horizontal));
  if (margin > 0) {
    const double band[] = {fair - margin, fair + margin};
    ImPlot::PlotInfLines("margin (95%)", band, 2,
                         ImPlotSpec(ImPlotProp_Flags, ImPlotInfLinesFlags_Horizontal));
  }
  ImPlot::EndPlot();
}

} // namespace

LabWindowPadd::LabWindowPadd(ScratchPadd::System *system)
    : Base(system), options_(startOptions()), runAtStart_(startRunning()) {
  receive<GamePlayed>(
      [this](const GamePlayed &game, ScratchPadd::PaddAddress worker) { played(game, worker); });
}

void LabWindowPadd::starting() {
  namespace Scene = ScratchPadd::Scene;
  display_ = std::make_unique<Scene::Display>(1280, 860, "Cosmic Hex balance lab");
  if (!display_->isOpen()) {
    logger().error("No window: {}", display_->error());
    system_->stop();
    return;
  }
  renderer_ = std::make_unique<Scene::Renderer>(*display_);
  ui_ = std::make_unique<ScratchPadd::DebugUi::Ui>(*display_);
  [this]<std::size_t... I>(std::index_sequence<I...>) {
    (workers_.push_back(addressOf<SimulationPadd<static_cast<int>(I)>>()), ...);
  }(std::make_index_sequence<kSimulations>{});
  layouts_ = Batch::layoutNames(options_.layoutsFile);
  const auto chosen = std::find(layouts_.begin(), layouts_.end(), options_.layout);
  layoutChoice_ = chosen == layouts_.end() ? 0 : static_cast<int>(chosen - layouts_.begin());
  if (runAtStart_)
    runBatch();
  push([this] { drawFrame(); });
}

void LabWindowPadd::finishing() {
  running_ = false;
  ui_.reset();
  renderer_.reset();
  display_.reset();
}

void LabWindowPadd::runBatch() {
  problem_.clear();
  batchOptions_ = options_;
  if (!layouts_.empty()) {
    batchOptions_.layout = layouts_[static_cast<std::size_t>(layoutChoice_)];
    if (const auto problem = Batch::readLayout(batchOptions_)) {
      problem_ = *problem;
      return;
    }
  }
  batchOptions_.variant = (batchOptions_.edges ? "edges_" : "") + batchOptions_.layout +
                          (batchOptions_.smallFinds ? "_small" : "");
  ++batch_;
  nextGame_ = 0;
  tally_ = Tally{};
  tally_.players = batchOptions_.players;
  // Every game's line, as the simulator writes them
  std::error_code error;
  std::filesystem::create_directories(runsFolder(), error);
  csvPath_ = (runsFolder() / (timestamp() + "-" + batchOptions_.variant + ".csv")).string();
  csv_ = std::ofstream(csvPath_);
  csv_ << Batch::csvHeader();
  running_ = true;
  for (ScratchPadd::PaddAddress worker : workers_)
    handOut(worker);
}

void LabWindowPadd::handOut(ScratchPadd::PaddAddress worker) {
  if (!running_ || nextGame_ >= batchOptions_.games)
    return;
  sendTo(worker, PlayGame{batch_, nextGame_++, batchOptions_});
  ++inFlight_;
}

void LabWindowPadd::played(const GamePlayed &game, ScratchPadd::PaddAddress worker) {
  --inFlight_;
  if (game.batch == batch_) {
    tally_.add(game.result);
    csv_ << Batch::csvLine(batchOptions_, game.result) << std::flush;
  }
  handOut(worker);
  if (running_ && nextGame_ >= batchOptions_.games && inFlight_ == 0)
    running_ = false; // all played
}

void LabWindowPadd::drawFrame() {
  namespace Scene = ScratchPadd::Scene;
  if (!display_->isOpen()) {
    system_->stop();
    return;
  }
  display_->pollEvents();
  ui_->newFrame();
  Scene::Environment environment;
  environment.background = {28, 26, 30};
  renderer_->beginFrame(Scene::Camera{}, environment);
  // One window filling the display: the settings on the left, the charts on
  // the right
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(size);
  ImGui::Begin("Balance lab", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
  ImGui::BeginChild("settings", ImVec2(330, 0), ImGuiChildFlags_Borders);
  drawSettings();
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("charts", ImVec2(0, 0));
  drawCharts();
  ImGui::EndChild();
  ImGui::End();
  renderer_->endFrame();
  push([this] { drawFrame(); });
}

void LabWindowPadd::drawSettings() {
  ImGui::TextUnformatted("Cosmic Hex balance lab");
  ImGui::Separator();
  ImGui::BeginDisabled(running_);
  if (!layouts_.empty()) {
    const char *current = layouts_[static_cast<std::size_t>(layoutChoice_)].c_str();
    if (ImGui::BeginCombo("Layout", current)) {
      for (int i = 0; i < static_cast<int>(layouts_.size()); ++i)
        if (ImGui::Selectable(layouts_[static_cast<std::size_t>(i)].c_str(), i == layoutChoice_))
          layoutChoice_ = i;
      ImGui::EndCombo();
    }
  }
  ImGui::Checkbox("Corners and edges", &options_.edges);
  ImGui::Checkbox("Finds beside small systems", &options_.smallFinds);
  ImGui::SliderInt("Players", &options_.players, 3, 4);
  ImGui::InputInt("Games", &options_.games, 50, 200);
  ImGui::InputInt("Bot thinking", &options_.iterations, 50, 200);
  ImGui::InputInt("Points to win (0: 12)", &options_.win);
  ImGui::InputInt("Ship move (0: rules')", &options_.move);
  ImGui::InputInt("Fuel boost (0: rules')", &options_.boost);
  ImGui::InputInt("Boost below points", &options_.catchUpBelow);
  ImGui::InputInt("Imperial Tithe over", &options_.discardOver);
  ImGui::InputInt("Moons", &options_.moons);
  options_.games = std::max(options_.games, 1);
  options_.iterations = std::max(options_.iterations, 1);
  ImGui::EndDisabled();
  ImGui::Separator();
  if (!running_) {
    if (ImGui::Button("Run", ImVec2(120, 0)))
      runBatch();
  } else if (ImGui::Button("Stop", ImVec2(120, 0))) {
    stopBatch();
  }
  ImGui::SameLine();
  ImGui::Text("%d of %d games", tally_.games, running_ ? batchOptions_.games : tally_.games);
  if (running_ || tally_.games > 0)
    ImGui::ProgressBar(static_cast<float>(tally_.games) / std::max(batchOptions_.games, 1));
  if (!problem_.empty())
    ImGui::TextWrapped("%s", problem_.c_str());
  if (!csvPath_.empty())
    ImGui::TextWrapped("Saved as it goes: %s", csvPath_.c_str());
}

void LabWindowPadd::drawCharts() {
  if (tally_.games == 0) {
    ImGui::TextUnformatted(
        "Choose the settings and press Run: the charts fill in as games finish.");
    return;
  }
  const int players = tally_.players;
  ImGui::Text("%s: %d games, %d finished, %.1f rounds a game, %.1f badges a game",
              batchOptions_.variant.c_str(), tally_.games, tally_.finished, tally_.averageRounds(),
              tally_.badges / tally_.games);
  static const std::vector<const char *> kTurns = {"1st", "2nd", "3rd", "4th"};
  static const std::vector<const char *> kHomes = {"Home 1", "Home 2", "Home 3", "Home 4"};
  std::vector<double> byTurn, byHome;
  for (int p = 0; p < players; ++p) {
    byTurn.push_back(tally_.share(tally_.winsByTurn[static_cast<std::size_t>(p)]));
    byHome.push_back(tally_.share(tally_.winsByHome[static_cast<std::size_t>(p)]));
  }
  const std::vector<const char *> turns(kTurns.begin(), kTurns.begin() + players);
  const std::vector<const char *> homes(kHomes.begin(), kHomes.begin() + players);
  if (ImGui::BeginTable("shares", 2)) {
    ImGui::TableNextColumn();
    sharesChart("Wins by turn position", byTurn, turns, tally_.fairShare(), tally_.margin());
    ImGui::TableNextColumn();
    sharesChart("Wins by home", byHome, homes, tally_.fairShare(), tally_.margin());
    ImGui::EndTable();
  }
  if (ImGui::BeginTable("more", 2)) {
    ImGui::TableNextColumn();
    if (ImPlot::BeginPlot("Rounds a game", ImVec2(-1, 230),
                          ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend)) {
      ImPlot::SetupAxes("rounds", "games", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
      std::vector<double> counts(tally_.gamesByRounds.begin(), tally_.gamesByRounds.end());
      ImPlot::PlotBars("games", counts.data(), static_cast<int>(counts.size()), 0.8);
      ImPlot::EndPlot();
    }
    ImGui::TableNextColumn();
    if (ImPlot::BeginPlot("Stations at the end (a game)", ImVec2(-1, 230),
                          ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend)) {
      static const char *kSystems[] = {"singles", "pairs", "threes"};
      ImPlot::SetupAxes(nullptr, "stations", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
      ImPlot::SetupAxisTicks(ImAxis_X1, 0, 2, 3, kSystems);
      ImPlot::SetupAxisLimits(ImAxis_X1, -0.6, 2.6, ImPlotCond_Always);
      std::array<double, 3> perGame{};
      for (std::size_t i = 0; i < perGame.size(); ++i)
        perGame[i] = tally_.stationsBySystem[i] / tally_.games;
      ImPlot::PlotBars("stations", perGame.data(), 3, 0.6);
      ImPlot::EndPlot();
    }
    ImGui::EndTable();
  }
}

} // namespace CosmicHex::Lab
