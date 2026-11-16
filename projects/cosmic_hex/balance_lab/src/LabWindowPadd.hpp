// LabWindowPadd.hpp - the balance lab's window, in three columns:
//
//   settings   the layout (a thumbnail; Choose opens every layout as
//              thumbnails), the rules' settings, Run and Stop
//   board      the chosen layout drawn large: homes, planets, finds, the
//              empty-space corners a deep-space start may use; click a corner
//              to see how far a ship gets in a turn (and with fuel); in Edit,
//              click a hex to make it a planet, a find or empty, and save the
//              result as a new layout
//   charts     the batch's results as its games come in
//
// It hands one game at a time to each SimulationPadd and the next as each
// reports back, and writes every game to a CSV file (the simulator's
// columns) under balance_lab_runs/.
#pragma once

#include "LabLayouts.hpp"
#include "SimulationPadd.hpp"
#include "Tally.hpp"

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

  void drawFrame();
  void drawSettings();
  void drawLayoutChooser();
  void drawBoardView();
  void drawCharts();

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
  // boost, as the settings make them
  int shipMove() const;
  int boostMove() const;

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

  std::vector<ScratchPadd::PaddAddress> workers_;
  int batch_ = 0;    // the batch's number (results from an older one are dropped)
  int nextGame_ = 0; // the next game to hand out
  int inFlight_ = 0; // games being played
  bool running_ = false;
  Tally tally_;
  std::ofstream csv_;
  std::string csvPath_;
  std::string problem_; // what went wrong starting the last batch, if anything

  std::unique_ptr<ScratchPadd::Scene::Display> display_;
  std::unique_ptr<ScratchPadd::Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;
};

} // namespace CosmicHex::Lab
