#pragma once

// WriterPadd.hpp - writes a DS's debug log to the card when nothing else has
// work. List it in an app's system; it does nothing without a CardLog (a
// release build, a 3DS, a desktop):
//
//   ScratchPadd::SystemImplementation<GamePadd, ScreenPadd, ...,
//                                     ScratchPadd::DebugLog::WriterPadd> system;
//
// While it runs, logging only adds a line to the CardLog in memory (no padd
// waits for the card), and the CardLog rings this padd's doorbell every
// "Lines between writes" lines, and at once for a warning or an error: the
// padd then writes them all and syncs once, on its own thread, which runs
// only when nothing else has work. Its repeat() writes whatever's left once
// a second. It publishes the CardLog's figures (CardLog::Figures) as shared
// state after each write. Before it starts and after it stops, the CardLog
// writes lines itself, as they're logged.

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/DebugLog/CardLog.hpp>

#include <chrono>
#include <optional>
#include <string>

namespace ScratchPadd::DebugLog {

class WriterPadd : public Base {
public:
  explicit WriterPadd(System *system) : Base(system), log_(CardLog::active()) {}
  std::string name() override { return "DebugLogWriter"; }
#if SCRATCHPADD_HAS_THREADS
  Runs runs() override { return Runs::WhenIdle; }
#endif
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    if (!log_) {
      return std::nullopt;
    }
    return std::chrono::seconds(1);
  }
  ControlValueMap generateControls() override {
    if (!log_) {
      return {};
    }
    return {{kWriteEvery, ControlType::Integer(static_cast<std::int32_t>(log_->writeEvery()),
                                               std::make_pair(1, 256))}};
  }
  void controlChanged(const std::string &, const ControlTypeVariant &) override {
    log_->setWriteEvery(static_cast<unsigned>(control<ControlType::Integer>(kWriteEvery).value));
  }

  void starting() override {
    if (log_) {
      // (the doorbell: rung from whichever thread logs, it queues a write
      // here, on this padd's own thread)
      log_->attach([this] { push([this] { write(); }); });
    }
  }
  void finishing() override {
    if (log_) {
      log_->detach();
      write();
    }
  }
  void repeat() override { write(); }

private:
  static constexpr const char *kWriteEvery = "Lines between writes";

  void write() {
    log_->write();
    publishState(log_->figures());
  }

  CardLog *log_; // (the Session's, which outlives the system)
};

} // namespace ScratchPadd::DebugLog
