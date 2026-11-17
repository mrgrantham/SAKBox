// LabWindowPadd.hpp - the balance lab's window, in three columns:
//
//   settings   the layout (a thumbnail; Choose opens every layout as
//              thumbnails), the rules' settings, Run and Stop
//   board      the chosen layout drawn large: homes, planets, finds, the
//              empty-space corners a deep-space start may use; click a corner
//              to see how far a ship gets in a turn (and with fuel); in Edit,
//              click a hex to make it a planet, a find or empty, and save the
//              result as a new layout
//              (its Layout tab); its Replay tab plays a game back, from the
//              batch or from a log file
//   charts     the batch's results as its games come in (Charts), and its
//              games as a list to pick one to watch (Games)
//
// It hands one game at a time to each SimulationPadd and the next as each
// reports back, and writes every game to a CSV file (the simulator's
// columns) under balance_lab_runs/, and each game's action log beside it
// (a folder a batch) for replays.
#pragma once

#include "BoardProving.hpp"
#include "LabLayouts.hpp"
#include "ReplayView.hpp"
#include "SimulationPadd.hpp"
#include "Tally.hpp"

#include <CosmicHex/Solver/LayoutSolver.hpp>
#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace CosmicHex::Lab {

inline constexpr int kSimulations = 8; // games played at once (SimulationPadds)

class LabWindowPadd : public ScratchPadd::Base {
public:
  // The batch to show first, and whether to start it at once (main sets it
  // from the command line before the system starts: a padd is made from the
  // system alone)
  static void startWith(Batch::Options options, bool runAtStart, bool choosing = false) {
    startOptions() = std::move(options);
    startRunning() = runAtStart;
    startChoosing() = choosing;
  }
  // The solver's tab open at the start, having solved once (--solve)
  static void solveAtStart() { startSolving() = true; }
  // ...and then proves this many of them by playing them (--prove N)
  static void proveAtStart(int boards) {
    startSolving() = true;
    startProving() = boards;
  }
  // A game's log to open in the Replay tab at the start, at this action
  static void replayAtStart(std::string logFile, int atAction) {
    startReplay() = {std::move(logFile), atAction};
  }

  explicit LabWindowPadd(ScratchPadd::System *system);

  std::string name() override { return "BalanceLab"; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void starting() override;
  void finishing() override;

private:
  static Batch::Options &startOptions() {
    static Batch::Options options;
    return options;
  }
  static bool &startRunning() {
    static bool run = false;
    return run;
  }
  static bool &startChoosing() {
    static bool choosing = false;
    return choosing;
  }
  static bool &startSolving() {
    static bool solving = false;
    return solving;
  }
  static int &startProving() {
    static int boards = 0;
    return boards;
  }
  static std::pair<std::string, int> &startReplay() {
    static std::pair<std::string, int> replay;
    return replay;
  }

  void drawFrame();
  void drawSettings();
  void drawLayoutChooser();
  void drawLayoutView();
  void drawSolverTab();
  void solveLayouts(); // the solver run with the tab's spec
  // Proving the solved boards (BoardProving): the next run started, if any
  void proveNext();
  void drawReplayTab();
  void drawCharts();
  void drawGames();

  // Watching a game: one of the batch's (its index in results_), or a log file
  void watch(std::size_t result);
  void openLogFile();

  // Every layout from the repo's layouts file and the lab's own (saved ones)
  void loadLayouts();
  void chooseLayout(int index);
  void saveLayout();

  // A batch: from the settings shown, game 0 on
  void runBatch();
  void stopBatch() { running_ = false; }
  // Gives `worker` the batch's next game, if there is one and it's running
  void handOut(ScratchPadd::PaddAddress worker);
  void played(const GamePlayed &game, ScratchPadd::PaddAddress worker);

  // A ship's move along edges (or hexes, without corners) and its fuel
  // boost, as the settings make them, and the rules' own for each
  int shipMove() const;
  int boostMove() const;
  static int defaultMove(bool edges);
  static int defaultBoost(bool edges);
  void showRealValues();

  Batch::Options options_;      // as the settings show them
  Batch::Options batchOptions_; // the running batch's
  bool runAtStart_ = false;

  std::vector<LabLayout> layouts_;
  int layoutChoice_ = 0;
  LabLayout edited_;                // the chosen layout, with any edits
  bool editing_ = false;            // clicks on the board change hexes
  bool changed_ = false;            // edited since chosen or saved
  std::string saveName_;            // the name to save it as
  std::string saveMessage_;         // what the last save did
  std::optional<Corner> reachFrom_; // where a ship's reach is shown from
  bool choosing_ = false;           // the layouts' thumbnails are open
  // The layout solver's tab: its spec as the controls show it, and what it
  // found (as layouts to show and choose, and the solver's own figures)
  Solver::SolverSpec solverSpec_;
  int solverAway_ = 3; // finds never touch clusters of this many (0: anywhere)
  std::vector<LabLayout> solved_;
  std::vector<Solver::SolvedLayout> solvedFigures_;
  bool showLayout_ = false; // a solved layout was chosen: show its tab
  bool showSolver_ = false; // (--solve) the solver's tab, once
  // Playing the solved boards to see which are fair (round 87)
  BoardProving proving_;
  std::optional<BoardProving::Run> provingRun_; // the batch running for it, if one is
  int boardsToProve_ = 6;
  int gamesBeforeProving_ = 0; // (the settings' games, given back after)

  std::vector<ScratchPadd::PaddAddress> workers_;
  int batch_ = 0;    // the batch's number (results from an older one are dropped)
  int nextGame_ = 0; // the next game to hand out
  int inFlight_ = 0; // games being played
  bool running_ = false;
  Tally tally_;
  std::ofstream csv_;
  std::string csvPath_;
  std::string problem_;                    // what went wrong starting the last batch, if anything
  std::vector<Batch::GameResult> results_; // the batch's games, as they finish
  std::string logsFolder_;                 // where each game's action log is written

  ReplayView replay_;
  bool showReplay_ = false; // bring the Replay tab forward (next frame)
  std::string openPath_;    // a log file to open
  std::string openMessage_; // what opening it did

  std::unique_ptr<ScratchPadd::Scene::Display> display_;
  std::unique_ptr<ScratchPadd::Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;
};

} // namespace CosmicHex::Lab
