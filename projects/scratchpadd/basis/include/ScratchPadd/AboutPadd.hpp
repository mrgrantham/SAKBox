// AboutPadd.hpp - the app's About tab on a controls panel: its title, name,
// build and platform (AppInfo), and a table of each padd and how it runs
// (its Runs), as Info controls (shown, not edited; the table in lines of
// tab-separated columns, which panels show as a table). It does nothing else, on the main thread
// (no thread of its own for that).
//
//   ScratchPadd::SystemImplementation<ScratchPadd::AboutPadd, GamePadd, ...> system;
//
// (Listed first, its tab comes first on a handheld's panel.)
#pragma once

#include <ScratchPadd/AppInfo.hpp>
#include <ScratchPadd/Base.hpp>

#include <string>

namespace ScratchPadd {

class AboutPadd : public Base {
public:
  explicit AboutPadd(System *system) : Base(system) {}
  std::string name() override { return "About"; }
  Runs runs() override { return Runs::OnMainThread; }
  ControlValueMap generateControls() override {
    const AppInfo &app = AppInfo::current();
    return {{"App", ControlType::Info(app.title)},
            {"App name", ControlType::Info(app.name)},
            {"Build", ControlType::Info(app.build)},
            {"Platform", ControlType::Info(app.platform)}};
  }
  // The padds, once they're all set up, in a table:
  //   Padd         Runs
  //   RagdollPadd  Steadily
  void starting() override {
    std::string table = "Padd\tRuns";
    for (const PaddAddress &padd : system_->padds())
      table += "\n" + padd.name() + "\t" + nameOf(padd.howItRuns());
    addControl("Padds", ControlType::Info(std::move(table)));
  }
};

} // namespace ScratchPadd
