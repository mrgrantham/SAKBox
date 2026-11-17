#include "LabWindowPadd.hpp"

#include "BoardDrawing.hpp"

#include <HexGrid/Layout.hpp>
#include <HexGrid/Paths.hpp>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "implot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>

namespace CosmicHex::Lab {

namespace {

// Where the lab writes: in the repo when run with bazel run
// (BUILD_WORKSPACE_DIRECTORY), or the working directory
std::filesystem::path workspace() {
  const char *folder = std::getenv("BUILD_WORKSPACE_DIRECTORY");
  return std::filesystem::path(folder ? folder : ".");
}
// Each batch's games, a CSV file each
std::filesystem::path runsFolder() { return workspace() / "balance_lab_runs"; }
// Layouts saved from the lab, beside the repo's
std::filesystem::path savedLayoutsFile() {
  return workspace() / "projects/cosmic_hex/layouts/saved_layouts.txt";
}

// "20261116-031500"
std::string timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now); // (Windows' thread-safe localtime, its arguments the other way round)
#else
  localtime_r(&now, &local);
#endif
  char text[32];
  std::strftime(text, sizeof text, "%Y%m%d-%H%M%S", &local);
  return text;
}

// ---- Drawing a layout ----

// A corner touching no planet or home (a deep-space start's place)
bool emptySpace(const LabLayout &layout, Corner corner) {
  for (Hex hex : HexGrid::hexesTouching(corner)) {
    const HexKind kind = kindAt(layout, hex);
    if (kind == HexKind::Planet || kind == HexKind::Home)
      return false;
  }
  return onBoard(corner);
}

// What to draw beyond the hexes
struct BoardMarks {
  bool letters = true;       // H, P and F on the hexes, the core's star
  bool emptyCorners = false; // the deep-space start's corners
};

void drawBoard(ImDrawList *draw, const HexGrid::Layout &at, const LabLayout &layout,
               BoardMarks marks) {
  const float size = at.size.x;
  for (Hex hex : HexGrid::spiral(Hex{}, kBoardRadius)) {
    const HexKind kind = kindAt(layout, hex);
    const ImU32 fill = kind == HexKind::Home     ? kHome
                       : kind == HexKind::Planet ? kPlanet
                       : kind == HexKind::Find   ? kFind
                                                 : kSpace;
    drawHex(draw, at, hex, fill);
    if (!marks.letters || size < 9)
      continue;
    const char *letter = kind == HexKind::Home     ? "H"
                         : kind == HexKind::Planet ? (hex == Hex{} ? "*" : "P")
                         : kind == HexKind::Find   ? "F"
                                                   : nullptr;
    if (letter)
      centredText(draw, toImVec(at.centre(hex)),
                  kind == HexKind::Home || kind == HexKind::Planet ? kDark : kText, letter);
  }
  if (marks.emptyCorners) {
    for (Corner corner : HexGrid::everyCorner(HexGrid::spiral(Hex{}, kBoardRadius)))
      if (emptySpace(layout, corner))
        draw->AddCircleFilled(toImVec(at.point(corner)), std::max(1.5f, size * 0.09f),
                              kEmptyCorner);
  }
}

// A layout's board drawn in a box `side` pixels square at the cursor, as a
// button; returns whether it was clicked
bool boardButton(const char *id, const LabLayout &layout, float side, bool chosen) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(id, ImVec2(side, side));
  ImDrawList *draw = ImGui::GetWindowDrawList();
  if (chosen || ImGui::IsItemHovered())
    draw->AddRect(corner, ImVec2(corner.x + side, corner.y + side), chosen ? kReach : kGrid, 4.0f,
                  0, 2.0f);
  const float size = side / kBoardAcross;
  drawBoard(draw, boardLayout(ImVec2(corner.x + side / 2, corner.y + side / 2), size), layout,
            {.letters = false});
  return clicked;
}

// Bars of `values` (one a category, labelled), with the fair share and its
// margin drawn across, in a plot `title`
void sharesChart(const char *title, const std::vector<double> &values,
                 const std::vector<const char *> &labels, double fair, double margin) {
  if (!ImPlot::BeginPlot(title, ImVec2(-1, 210),
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
  display_ = std::make_unique<Scene::Display>(1680, 980, "Cosmic Hex balance lab");
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
  showRealValues();
  loadLayouts();
  const auto chosen = std::find_if(layouts_.begin(), layouts_.end(),
                                   [&](const LabLayout &l) { return l.name == options_.layout; });
  chooseLayout(chosen == layouts_.end() ? 0 : static_cast<int>(chosen - layouts_.begin()));
  choosing_ = startChoosing();
  if (!startReplay().first.empty()) {
    openPath_ = startReplay().first;
    openLogFile();
    replay_.goTo(static_cast<std::size_t>(std::max(startReplay().second, 0)));
    showReplay_ = true;
  }
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

// ---- Layouts ----

void LabWindowPadd::loadLayouts() {
  layouts_ = readLayouts(options_.layoutsFile);
  for (LabLayout &saved : readLayouts(savedLayoutsFile().string()))
    layouts_.push_back(std::move(saved));
}

void LabWindowPadd::chooseLayout(int index) {
  if (layouts_.empty())
    return;
  layoutChoice_ = std::clamp(index, 0, static_cast<int>(layouts_.size()) - 1);
  edited_ = layouts_[static_cast<std::size_t>(layoutChoice_)];
  // Finds beside the small systems, if asked for and the file has them
  if (options_.smallFinds) {
    Batch::Options withFinds;
    withFinds.layout = edited_.name;
    withFinds.layoutsFile = edited_.file;
    withFinds.smallFinds = true;
    if (!Batch::readLayout(withFinds) && !withFinds.zone.empty())
      edited_.zone = withFinds.zone;
  }
  changed_ = false;
  saveName_ = edited_.name + "-edit";
  saveMessage_.clear();
  // A ship's reach shown from the first home's corner nearest the centre,
  // until another corner is clicked
  reachFrom_.reset();
  if (!edited_.homes.empty()) {
    float best = 1e9f;
    for (Corner corner : HexGrid::corners(edited_.homes[0])) {
      const HexGrid::Point p = boardLayout(ImVec2(0, 0), 1).point(corner);
      const float d = std::hypot(p.x, p.y);
      if (d < best) {
        best = d;
        reachFrom_ = corner;
      }
    }
  }
}

void LabWindowPadd::saveLayout() {
  LabLayout saved = edited_;
  saved.name = saveName_;
  saved.file = savedLayoutsFile().string();
  if (saved.name.empty() || saved.name.find(' ') != std::string::npos) {
    saveMessage_ = "A name, without spaces, please";
    return;
  }
  for (const LabLayout &layout : layouts_)
    if (layout.name == saved.name) {
      saveMessage_ = "There's already a layout called " + saved.name;
      return;
    }
  saved.zone = zoneOf(saved); // (its finds' places, as shown)
  std::error_code error;
  std::filesystem::create_directories(savedLayoutsFile().parent_path(), error);
  std::ofstream file(saved.file, std::ios::app);
  file << layoutLine(saved) << "\n";
  if (!file) {
    saveMessage_ = "Couldn't write " + saved.file;
    return;
  }
  file.close();
  loadLayouts();
  const auto found = std::find_if(layouts_.begin(), layouts_.end(),
                                  [&](const LabLayout &l) { return l.name == saved.name; });
  chooseLayout(static_cast<int>(found - layouts_.begin()));
  saveMessage_ = "Saved as " + saved.name + " (in " + saved.file + ")";
}

// The rules' own move and boost: 2 edges and 1 more with corners and
// edges, Settings' hexes without
int LabWindowPadd::defaultMove(bool edges) { return edges ? 2 : Settings{}.shipMove; }
int LabWindowPadd::defaultBoost(bool edges) { return edges ? 1 : Settings{}.boostMove; }

// Every setting shows the number the games will use (no 0 for "the rules'
// own"): the move and boost for the rules chosen, points to win
void LabWindowPadd::showRealValues() {
  if (options_.move <= 0)
    options_.move = defaultMove(options_.edges);
  if (options_.boost <= 0)
    options_.boost = defaultBoost(options_.edges);
  if (options_.win <= 0)
    options_.win = Settings{}.winPoints;
}

int LabWindowPadd::shipMove() const { return options_.move; }
int LabWindowPadd::boostMove() const { return options_.boost; }

// ---- Batches ----

void LabWindowPadd::runBatch() {
  problem_.clear();
  batchOptions_ = options_;
  batchOptions_.layout = edited_.name + (changed_ ? "-unsaved" : "");
  batchOptions_.homes = edited_.homes;
  batchOptions_.planets = edited_.planets;
  batchOptions_.zone = edited_.zone; // (empty: the usual places)
  batchOptions_.variant = std::string(batchOptions_.edges ? "edges_" : "") +
                          (batchOptions_.deepSpace ? "deep_" : "") + batchOptions_.layout +
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
  // Each game's action log, in a folder named as the CSV is
  results_.clear();
  logsFolder_ = csvPath_.substr(0, csvPath_.size() - 4);
  std::filesystem::create_directories(logsFolder_, error);
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
    char name[32];
    std::snprintf(name, sizeof name, "game_%04d.log", game.result.game + 1);
    std::ofstream(std::filesystem::path(logsFolder_) / name) << ActionLog::write(game.result.log);
    results_.push_back(game.result);
  }
  handOut(worker);
  if (running_ && nextGame_ >= batchOptions_.games && inFlight_ == 0)
    running_ = false; // all played
}

// ---- The window ----

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
  // One window filling the display, in three columns
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(size);
  ImGui::Begin("Balance lab", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
  ImGui::BeginChild("settings", ImVec2(410, 0), ImGuiChildFlags_Borders);
  drawSettings();
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("board", ImVec2(560, 0), ImGuiChildFlags_Borders);
  if (ImGui::BeginTabBar("boardTabs")) {
    if (ImGui::BeginTabItem("Layout")) {
      drawLayoutView();
      ImGui::EndTabItem();
    }
    const ImGuiTabItemFlags replayFlags = showReplay_ ? ImGuiTabItemFlags_SetSelected : 0;
    showReplay_ = false;
    if (ImGui::BeginTabItem("Replay", nullptr, replayFlags)) {
      drawReplayTab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("charts", ImVec2(0, 0));
  if (ImGui::BeginTabBar("resultTabs")) {
    if (ImGui::BeginTabItem("Charts")) {
      drawCharts();
      ImGui::EndTabItem();
    }
    const std::string games = "Games (" + std::to_string(results_.size()) + ")###games";
    if (ImGui::BeginTabItem(games.c_str())) {
      drawGames();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::EndChild();
  ImGui::End();
  if (choosing_)
    drawLayoutChooser();
  renderer_->endFrame();
  push([this] { drawFrame(); });
}

void LabWindowPadd::drawSettings() {
  ImGui::TextUnformatted("Cosmic Hex balance lab");
  ImGui::Separator();
  ImGui::BeginDisabled(running_);
  // The layout: its thumbnail, and Choose for all of them
  if (!layouts_.empty()) {
    if (boardButton("##current", edited_, 120, false))
      choosing_ = true;
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Layout %s%s", edited_.name.c_str(), changed_ ? " (edited)" : "");
    if (!edited_.reach.empty())
      ImGui::TextWrapped("Single, pair, three: %s moves", edited_.reach.c_str());
    ImGui::Text("%d empty-space corners", emptySpaceCorners(edited_));
    if (ImGui::Button("Choose a layout..."))
      choosing_ = true;
    ImGui::EndGroup();
  }
  ImGui::Separator();
  // Labels on the left, the fields a fixed width, so nothing's cut off
  ImGui::PushItemWidth(140);
  const bool wasEdges = options_.edges;
  if (ImGui::Checkbox("Corners and edges", &options_.edges)) {
    // The move and boost follow the rules, unless they'd been changed
    if (options_.move == defaultMove(wasEdges))
      options_.move = defaultMove(options_.edges);
    if (options_.boost == defaultBoost(wasEdges))
      options_.boost = defaultBoost(options_.edges);
  }
  ImGui::Checkbox("Deep-space start", &options_.deepSpace);
  if (ImGui::Checkbox("Finds beside small systems", &options_.smallFinds) && !changed_)
    chooseLayout(layoutChoice_); // (its finds from the file)
  ImGui::SliderInt("Players", &options_.players, 3, 4);
  ImGui::InputInt("Games", &options_.games, 50, 200);
  ImGui::InputInt("Bot thinking", &options_.iterations, 50, 200);
  ImGui::InputInt("Points to win", &options_.win);
  ImGui::InputInt(options_.edges ? "Ship move (edges)" : "Ship move (hexes)", &options_.move);
  ImGui::InputInt(options_.edges ? "Fuel boost (edges)" : "Fuel boost (hexes)", &options_.boost);
  ImGui::InputInt("Boost below points", &options_.catchUpBelow);
  ImGui::InputInt("Imperial Tithe over", &options_.discardOver);
  ImGui::InputInt("Moons", &options_.moons);
  ImGui::InputInt("Alien outposts", &options_.outposts);
  ImGui::InputInt("Homes exhaust at (0: never)", &options_.exhaustHomesAt);
  ImGui::Checkbox("Exhausted home's kind random", &options_.exhaustedKindRandom);
  ImGui::InputInt("Cartel takes (0: all)", &options_.cartelTake);
  ImGui::Checkbox("Trading between players", &options_.trading);
  ImGui::Checkbox("Home income on the home die", &options_.homeDie);
  // Round 61: free stations, and the ship costing a food more with them
  bool freeStations = options_.stationCost.has_value();
  if (ImGui::Checkbox("Free stations", &freeStations))
    options_.stationCost = freeStations ? std::optional<Resources>(Resources{}) : std::nullopt;
  bool dearerShip = options_.shipCost.has_value();
  if (ImGui::Checkbox("Ship costs a food more", &dearerShip))
    options_.shipCost =
        dearerShip ? std::optional<Resources>(Resources{1, 1, 1, 0, 0}) : std::nullopt;
  // Round 62: how common each kind of planet is
  ImGui::InputScalarN("Planet mix (min, food, fuel, water, cry)", ImGuiDataType_S32,
                      options_.kindWeights.data(), kResources);
  ImGui::PopItemWidth();
  options_.games = std::max(options_.games, 1);
  options_.iterations = std::max(options_.iterations, 1);
  options_.win = std::max(options_.win, 1);
  options_.move = std::max(options_.move, 1);
  options_.boost = std::max(options_.boost, 0);
  options_.outposts = std::clamp(options_.outposts, 0, 12 - options_.empty - 1);
  for (int &weight : options_.kindWeights)
    weight = std::max(weight, 0);
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

void LabWindowPadd::drawLayoutChooser() {
  ImGui::SetNextWindowSize(ImVec2(980, 720), ImGuiCond_Appearing);
  ImGui::SetNextWindowPos(ImVec2(80, 60), ImGuiCond_Appearing);
  if (!ImGui::Begin("Choose a layout", &choosing_)) {
    ImGui::End();
    return;
  }
  // Every layout as a thumbnail with its name, as many to a row as fit
  const float side = 130, gap = ImGui::GetStyle().ItemSpacing.x;
  const int perRow =
      std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + gap) / (side + gap)));
  for (int i = 0; i < static_cast<int>(layouts_.size()); ++i) {
    const LabLayout &layout = layouts_[static_cast<std::size_t>(i)];
    if (i % perRow != 0)
      ImGui::SameLine();
    ImGui::BeginGroup();
    const std::string id = "##layout" + std::to_string(i);
    if (boardButton(id.c_str(), layout, side, i == layoutChoice_)) {
      chooseLayout(i);
      choosing_ = false;
    }
    ImGui::Text("%s  (%d empty)", layout.name.c_str(), emptySpaceCorners(layout));
    ImGui::EndGroup();
  }
  ImGui::End();
}

void LabWindowPadd::drawLayoutView() {
  ImGui::Checkbox("Edit planets and finds", &editing_);
  ImGui::SameLine();
  ImGui::TextDisabled(editing_ ? "click a hex: empty, planet, find, empty"
                               : "click a corner: a ship's reach from there");
  // The board, as large as the column allows
  const float side = std::min(ImGui::GetContentRegionAvail().x, 520.0f);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##board", ImVec2(side, side));
  const bool clicked = ImGui::IsItemClicked();
  const float size = side / kBoardAcross;
  const HexGrid::Layout at = boardLayout(ImVec2(origin.x + side / 2, origin.y + side / 2), size);
  ImDrawList *draw = ImGui::GetWindowDrawList();
  drawBoard(draw, at, edited_, {.letters = true, .emptyCorners = options_.deepSpace});

  const ImVec2 mouse = ImGui::GetMousePos();
  if (clicked && editing_) {
    const Hex hex = at.hexAt({mouse.x, mouse.y});
    if (HexGrid::length(hex) <= kBoardRadius) {
      cycle(edited_, hex);
      changed_ = true;
    }
  } else if (clicked) {
    // The nearest corner to the click
    float best = 1e9f;
    for (Corner corner : HexGrid::everyCorner(HexGrid::spiral(Hex{}, kBoardRadius))) {
      const HexGrid::Point p = at.point(corner);
      const float d = std::hypot(p.x - mouse.x, p.y - mouse.y);
      if (d < best && onBoard(corner)) {
        best = d;
        reachFrom_ = corner;
      }
    }
  }

  // A ship's reach from the chosen corner: filled within a turn's move,
  // rings within the fuel boost's (with corners: corner to corner along
  // edges; without: hex to hex, shown on the hexes)
  if (reachFrom_) {
    const int move = shipMove(), boost = boostMove();
    if (options_.edges) {
      for (const auto &reach :
           HexGrid::reachable(*reachFrom_, move + boost, [](Corner c) { return onBoard(c); })) {
        const ImVec2 p = toImVec(at.point(reach.place));
        if (reach.steps <= move)
          draw->AddCircleFilled(p, size * 0.2f, kReach);
        else
          draw->AddCircle(p, size * 0.22f, kFuel, 0, 2.0f);
      }
    } else {
      const Hex from = HexGrid::hexesTouching(*reachFrom_)[0];
      for (const auto &reach : HexGrid::reachable(
               from, move + boost, [](Hex h) { return HexGrid::length(h) <= kBoardRadius; })) {
        const ImVec2 c = toImVec(at.centre(reach.place));
        if (reach.steps <= move)
          draw->AddCircleFilled(c, size * 0.3f, kReach);
        else
          draw->AddCircle(c, size * 0.32f, kFuel, 0, 2.0f);
      }
    }
    draw->AddCircleFilled(toImVec(at.point(*reachFrom_)), size * 0.25f, kHome);
    ImGui::Text("A ship's reach: %d %s a turn (filled), %d more with fuel (rings)", move,
                options_.edges ? "edges" : "hexes", boost);
  }
  ImGui::Text("H home, P planet, * the core, F a find%s",
              options_.deepSpace ? "; dots: empty-space corners" : "");
  ImGui::Text("Empty-space corners (deep-space starts): %d", emptySpaceCorners(edited_));

  // Saving an edited layout as a new one
  ImGui::Separator();
  ImGui::PushItemWidth(200);
  ImGui::InputText("Name", &saveName_);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  ImGui::BeginDisabled(!changed_);
  if (ImGui::Button("Save as a new layout"))
    saveLayout();
  ImGui::SameLine();
  if (ImGui::Button("Undo edits"))
    chooseLayout(layoutChoice_);
  ImGui::EndDisabled();
  if (!saveMessage_.empty())
    ImGui::TextWrapped("%s", saveMessage_.c_str());
}

// ---- Replays ----

void LabWindowPadd::watch(std::size_t result) {
  const Batch::GameResult &game = results_[result];
  std::string title = "Game " + std::to_string(game.game + 1) + " of " + batchOptions_.variant;
  title += game.winner >= 0 ? ": Player " + std::to_string(game.winner + 1) + " won in round " +
                                  std::to_string(game.rounds)
                            : ": unfinished";
  replay_.show(GameReplay::of(game.log), std::move(title));
  showReplay_ = true;
}

void LabWindowPadd::openLogFile() {
  std::ifstream file(openPath_);
  if (!file) {
    openMessage_ = "Couldn't open " + openPath_;
    return;
  }
  std::stringstream text;
  text << file.rdbuf();
  auto replay = GameReplay::fromText(text.str());
  if (!replay) {
    openMessage_ = "Not a Cosmic Hex log (" + replay.error() + ")";
    return;
  }
  openMessage_.clear();
  replay_.show(std::move(*replay), std::filesystem::path(openPath_).filename().string());
}

void LabWindowPadd::drawReplayTab() {
  // Opening a log file: the lab's own (in balance_lab_runs/), the
  // simulator's --logs, and later a real game's
  ImGui::PushItemWidth(330);
  ImGui::InputTextWithHint("##open", "a log file's path", &openPath_);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button("Open"))
    openLogFile();
  if (!openMessage_.empty())
    ImGui::TextWrapped("%s", openMessage_.c_str());
  ImGui::Separator();
  if (replay_.empty()) {
    ImGui::TextWrapped("Pick a game in the Games tab (right) to watch it here, once a batch has "
                       "played some, or open a log file.");
    return;
  }
  replay_.draw(ImGui::GetContentRegionAvail().x);
}

void LabWindowPadd::drawGames() {
  if (results_.empty()) {
    ImGui::TextUnformatted("The batch's games appear here as they finish: pick one to watch it.");
    return;
  }
  ImGui::TextWrapped("Each game's log is saved in %s", logsFolder_.c_str());
  constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                     ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable;
  if (!ImGui::BeginTable("games", 6, kFlags))
    return;
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_DefaultSort);
  ImGui::TableSetupColumn("Winner");
  ImGui::TableSetupColumn("Went");
  ImGui::TableSetupColumn("Rounds");
  ImGui::TableSetupColumn("Points", ImGuiTableColumnFlags_NoSort);
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort);
  ImGui::TableHeadersRow();
  // The order the table's sorted in (by game, winner, turn order or rounds)
  std::vector<std::size_t> order(results_.size());
  for (std::size_t i = 0; i < order.size(); ++i)
    order[i] = i;
  const auto turnOf = [&](const Batch::GameResult &g) {
    return g.winner < 0 ? 9 : (g.winner - g.first + batchOptions_.players) % batchOptions_.players;
  };
  if (const ImGuiTableSortSpecs *sort = ImGui::TableGetSortSpecs(); sort && sort->SpecsCount > 0) {
    const ImGuiTableColumnSortSpecs &spec = sort->Specs[0];
    const auto key = [&](const Batch::GameResult &g) {
      switch (spec.ColumnIndex) {
      case 1:
        return g.winner < 0 ? 9 : g.winner;
      case 2:
        return turnOf(g);
      case 3:
        return g.rounds;
      default:
        return g.game;
      }
    };
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
      const int ka = key(results_[a]), kb = key(results_[b]);
      return spec.SortDirection == ImGuiSortDirection_Descending ? ka > kb : ka < kb;
    });
  }
  static constexpr std::array<const char *, 4> kOrder = {"1st", "2nd", "3rd", "4th"};
  for (std::size_t i : order) {
    const Batch::GameResult &game = results_[i];
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%d", game.game + 1);
    ImGui::TableNextColumn();
    if (game.winner >= 0)
      ImGui::Text("Player %d", game.winner + 1);
    else
      ImGui::TextDisabled("unfinished");
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(game.winner >= 0 ? kOrder[static_cast<std::size_t>(turnOf(game))] : "");
    ImGui::TableNextColumn();
    ImGui::Text("%d", game.rounds);
    ImGui::TableNextColumn();
    std::string points;
    for (int p = 0; p < batchOptions_.players; ++p)
      points += (p ? " " : "") + std::to_string(game.points[static_cast<std::size_t>(p)]);
    ImGui::TextUnformatted(points.c_str());
    ImGui::TableNextColumn();
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::SmallButton("Watch"))
      watch(i);
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void LabWindowPadd::drawCharts() {
  if (tally_.games == 0) {
    ImGui::TextUnformatted(
        "Choose the settings and press Run: the charts fill in as games finish.");
    return;
  }
  const int players = tally_.players;
  ImGui::TextWrapped("%s: %d games, %d finished, %.1f rounds a game, %.1f badges a game",
                     batchOptions_.variant.c_str(), tally_.games, tally_.finished,
                     tally_.averageRounds(), tally_.badges / tally_.games);
  static const std::vector<const char *> kTurns = {"1st", "2nd", "3rd", "4th"};
  static const std::vector<const char *> kHomes = {"Home 1", "Home 2", "Home 3", "Home 4"};
  std::vector<double> byTurn, byHome;
  for (int p = 0; p < players; ++p) {
    byTurn.push_back(tally_.share(tally_.winsByTurn[static_cast<std::size_t>(p)]));
    byHome.push_back(tally_.share(tally_.winsByHome[static_cast<std::size_t>(p)]));
  }
  const std::vector<const char *> turns(kTurns.begin(), kTurns.begin() + players);
  const std::vector<const char *> homes(kHomes.begin(), kHomes.begin() + players);
  sharesChart("Wins by turn position", byTurn, turns, tally_.fairShare(), tally_.margin());
  sharesChart("Wins by home", byHome, homes, tally_.fairShare(), tally_.margin());
  if (ImPlot::BeginPlot("Rounds a game", ImVec2(-1, 190),
                        ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("rounds", "games", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    std::vector<double> counts(tally_.gamesByRounds.begin(), tally_.gamesByRounds.end());
    ImPlot::PlotBars("games", counts.data(), static_cast<int>(counts.size()), 0.8);
    ImPlot::EndPlot();
  }
  // Where the winners' points came from: on average a win (bars), and how
  // often each kind gave any (the labels)
  if (tally_.finished > 0 &&
      ImPlot::BeginPlot("Winners' points by source: a win's average (in: % of wins with any)",
                        ImVec2(-1, 210),
                        ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend)) {
    constexpr int kSources = static_cast<int>(Tally::kSourceNames.size());
    static std::array<std::string, kSources> labels;
    std::array<const char *, kSources> names{};
    std::array<double, kSources> average{};
    for (std::size_t i = 0; i < Tally::kSourceNames.size(); ++i) {
      average[i] = tally_.winnerPoints[i] / tally_.finished;
      labels[i] =
          std::string(Tally::kSourceNames[i]) + "\nin " +
          std::to_string(static_cast<int>(std::lround(tally_.share(tally_.winsWithSource[i])))) +
          "%";
      names[i] = labels[i].c_str();
    }
    ImPlot::SetupAxes(nullptr, "points a win", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisTicks(ImAxis_X1, 0, kSources - 1, kSources, names.data());
    ImPlot::SetupAxisLimits(ImAxis_X1, -0.6, kSources - 0.4, ImPlotCond_Always);
    ImPlot::PlotBars("points", average.data(), kSources, 0.6);
    ImPlot::EndPlot();
  }
  // Round 61's statistics: the game's flow, and trading, the winner's
  // beside the average player's
  if (tally_.finished > 0 &&
      ImGui::BeginTable("flow", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("A game");
    ImGui::TableSetupColumn("The winner");
    ImGui::TableSetupColumn("A player, on average");
    ImGui::TableHeadersRow();
    const double games = tally_.games, wins = tally_.finished;
    const auto row = [](const char *name, const std::string &winner, const std::string &everyone) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(name);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(winner.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(everyone.c_str());
    };
    const auto fixed = [](double v) {
      char text[32];
      std::snprintf(text, sizeof text, "%.1f", v);
      return std::string(text);
    };
    row("Lead changes", "", fixed(tally_.leadChanges / games));
    row("Round the winner took the lead for good", fixed(tally_.leadTakenRound / wins), "");
    row("Most the winner was behind (points)", fixed(tally_.comeback / wins), "");
    row("7s rolled", "", fixed(tally_.sevens / games));
    row("7s that cost someone resources (the Tithe)", "",
        fixed(tally_.sevens > 0 ? 100.0 * tally_.sevensWithDiscards / tally_.sevens : 0.0) + "%");
    row("Enforcers played", "", fixed(tally_.enforcers / games));
    row("Interstellar Cartels played", "", fixed(tally_.cartels / games));
    row("Shipyard Surplus played", "", fixed(tally_.shipyards / games));
    row("Supply drops played", "", fixed(tally_.supplyDrops / games));
    for (std::size_t i = 0; i < Tally::kTradeNames.size(); ++i)
      row(Tally::kTradeNames[i], fixed(tally_.winnerTrades[i] / wins),
          fixed(tally_.everyoneTrades[i] / (games * tally_.players)));
    ImGui::EndTable();
  }
  if (ImPlot::BeginPlot("Stations at the end (a game)", ImVec2(-1, 190),
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
}

} // namespace CosmicHex::Lab
