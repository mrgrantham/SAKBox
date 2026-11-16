// LabWindowPadd.hpp - the balance lab's window: a batch's settings, Run and
// Stop, and the results as charts while the games come in (wins by turn
// position and by home against a fair share, how long games last, where
// stations end up). It hands one game at a time to each SimulationPadd and
// the next as each reports back, and writes every game to a CSV file (the
// simulator's columns) under balance_lab_runs/.
#pragma once

#include "SimulationPadd.hpp"
#include "Tally.hpp"

#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <fstream>
#include <memory>
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
  static void startWith(Batch::Options options, bool runAtStart) {
    startOptions() = std::move(options);
    startRunning() = runAtStart;
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

  void drawFrame();
  void drawSettings();
  void drawCharts();

  // A batch: from the settings shown, game 0 on
  void runBatch();
  void stopBatch() { running_ = false; }
  // Gives `worker` the batch's next game, if there is one and it's running
  void handOut(ScratchPadd::PaddAddress worker);
  void played(const GamePlayed &game, ScratchPadd::PaddAddress worker);

  Batch::Options options_;      // as the settings show them
  Batch::Options batchOptions_; // the running batch's
  bool runAtStart_ = false;
  std::vector<std::string> layouts_; // names, from options_.layoutsFile
  int layoutChoice_ = 0;

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
