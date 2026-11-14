// ScratchPadd on a Nintendo 3DS: three padds, two of them on threads of
// their own, talking only by messages, as on a desktop.
//
//   ScreenPadd     (main thread) the frame loop: reads the buttons and sends
//                  them as messages; shows what the other padds send it on
//                  the top screen (status lines, then a log)
//   SoundBoardPadd (own thread)  gives AudioPadd sounds made in code, plays
//                  them when buttons are pressed, and a tune on a timer;
//                  logs AudioPadd's answers (a chime plays once all are
//                  loaded, so the log shows sound works without a button)
//   AudioPadd      (own thread)  plays them through the 3DS's DSP
//
// A, B and X play a chime, a knock and a blast; Y starts and stops the
// tune; SELECT shows every padd's controls (AudioPadd's volumes) on the
// bottom screen, as tabs (HandheldControls); START quits.
#include <3ds.h>

#include <AudioPadd.hpp>
#include <HandheldDemo.hpp>
#include <ScratchPadd/AboutPadd.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/HandheldControls/ConsolePanel.hpp>
#include <ScratchPadd/Input/GamePadd.hpp>
#include <ScratchPadd/Network/NetworkPadd.hpp>
#include <ScratchPadd/Network/RemoteControlsPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <deque>
#include <map>
#include <string>
#include <utility>

namespace {

using namespace HandheldDemo;
using ScratchPadd::HandheldControls::printLine;

// The top screen's console is 50 columns by 30 rows, the bottom's 40 by 30
constexpr int kTopColumns = ScratchPadd::HandheldControls::usableColumns(50);
constexpr int kBottomColumns = ScratchPadd::HandheldControls::usableColumns(40);

// Where a debug build's log goes (main sets it), for the last status line
std::string logWhere;

namespace Input = ScratchPadd::Input;

class ScreenPadd : public ScratchPadd::Base {
public:
  ScreenPadd(ScratchPadd::System *system)
      : Base(system), controls_(
                          [this](ScratchPadd::PaddAddress owner,
                                 const ScratchPadd::MessageType::ControlChange &change) {
                            sendTo(owner, change);
                          },
                          kBottomColumns, 30) {
    // Every padd's controls, for the panel SELECT shows
    receive<ScratchPadd::MessageType::ControlSnapshot>(
        [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot,
               ScratchPadd::PaddAddress owner) { controls_.show(snapshot, owner); });
    receive<StatusLine>([this](const StatusLine &line) { status_[line.row] = line.text; });
    receive<LogLine>([this](const LogLine &line) {
      log_.push_back(line.text);
      while (log_.size() > kLogLines) {
        log_.pop_front();
      }
    });
  }
  std::string name() override { return "ScreenPadd"; }
  // The frame loop and the HOME Menu's checks, on the main thread as in devkitPro's examples
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    consoleInit(GFX_BOTTOM, &bottom_);
    consoleInit(GFX_TOP, &top_);
    status_[0] = "ScreenPadd: main thread " + threadNumber();
    if (!logWhere.empty())
      status_[7] = "Log: " + logWhere;
    controls_.setVisible(false); // SELECT shows them
    showControlsHint();
    send(ScratchPadd::MessageType::ControlRequest{});
    push([this] { frame(); });
  }

private:
  Input::Gamepad lastPad_; // last frame's, for what's pressed since
  // Rows on the top screen, from 0
  static constexpr int kStatusRow = 2; // status lines 0-7 go from here
  static constexpr int kHelpRow = 11;
  static constexpr int kLogRow = 13;
  static constexpr std::size_t kLogLines = 16;

  // One frame, then the next (gspWaitForVBlank paces them at 60 a second)
  void frame() {
    if (!aptMainLoop()) {
      system_->stop();
      return;
    }
    // The pad as GamePadd last read it, and what was pressed since the
    // last frame
    const Input::Gamepad pad = currentState<Input::Gamepad>().value;
    const Input::Edges pressed(lastPad_, pad);
    const Input::Gamepad before = std::exchange(lastPad_, pad);
    if (pressed.pressed(Input::Button::Start)) {
      system_->stop();
      return;
    }
    // The controls on the bottom screen; while they're shown, the buttons
    // are theirs
    const bool wasShown = controls_.visible();
    controls_.handle(ScratchPadd::HandheldControls::panelInputFrom(before, pad));
    consoleSelect(&bottom_);
    if (controls_.visible() != wasShown) {
      std::printf("\x1b[2J");
      if (!controls_.visible())
        showControlsHint();
    }
    ScratchPadd::HandheldControls::draw(controls_);
    consoleSelect(&top_);
    if (!wasShown && !controls_.visible()) {
      // The face buttons, by the names on a Nintendo pad
      for (const Input::Button button : {Input::Button::FaceRight, Input::Button::FaceBottom,
                                         Input::Button::FaceTop, Input::Button::FaceLeft}) {
        if (pressed.pressed(button)) {
          send(ButtonPressed{pad.labelOf(button)});
        }
      }
    }
    draw();
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
    push([this] { frame(); });
  }

  // On the top screen: the title, the status lines (each at its own row),
  // the buttons, then the log
  void draw() {
    printLine(0, kTopColumns, "ScratchPadd on a 3DS: padds talking by messages");
    for (const auto &[row, text] : status_) {
      printLine(kStatusRow + row, kTopColumns, "%s", text.c_str());
    }
    printLine(kHelpRow, kTopColumns, "A chime  B knock  X blast  Y tune  START quit");
    printLine(kHelpRow + 1, kTopColumns, "SELECT controls (bottom screen)");
    int row = kLogRow;
    for (const std::string &line : log_) {
      printLine(row++, kTopColumns, "%s", line.c_str());
    }
  }

  // While the controls are hidden, the bottom screen says how to show them
  void showControlsHint() {
    consoleSelect(&bottom_);
    printLine(0, kBottomColumns, "SELECT: every padd's controls");
    consoleSelect(&top_);
  }

  PrintConsole top_, bottom_;
  ScratchPadd::HandheldControls::ControlTabs controls_;
  std::map<int, std::string> status_;
  std::deque<std::string> log_;
};

} // namespace

int main() {
  gfxInitDefault();
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
#if defined(SAKBOX_DEBUG)
  // A debug build's log, on the SD card and served over Wi-Fi (tools/console_log reads it)
  ScratchPadd::DebugLog::Session log;
  logWhere = log.where();
#endif
  {
    ScratchPadd::SystemImplementation<
        ScratchPadd::AboutPadd, ScratchPadd::Input::GamePadd, ScreenPadd, SoundBoardPadd, AudioPadd,
        ScratchPadd::Storage::StoragePadd, ScratchPadd::Network::NetworkPadd,
        ScratchPadd::Network::RemoteControlsPadd>
        system; // (StoragePadd: a debug build's log on the SD card; NetworkPadd: it over
                // Wi-Fi, and RemoteControlsPadd: the controls from the Mac)
    system.instantiate();
    system.start(); // returns when ScreenPadd stops it (START, or the HOME Menu closing it)
  }
  gfxExit();
  return 0;
}
