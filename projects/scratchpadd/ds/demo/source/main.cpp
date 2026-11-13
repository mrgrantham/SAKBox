// ScratchPadd on a Nintendo DS: three padds, two of them on threads of their
// own (calico's, through std::thread), talking only by messages, as on a
// desktop and the 3DS.
//
//   ScreenPadd     (main thread) the frame loop: reads the buttons and sends
//                  them as messages; shows what the other padds send it
//   SoundBoardPadd (own thread)  gives AudioPadd sounds made in code, plays
//                  them when buttons are pressed, and a tune on a timer
//                  (HandheldDemo.hpp, shared with the 3DS demo)
//   AudioPadd      (own thread)  plays them on the DS's hardware sound
//                  channels (AudioPadd/HardwareChannelsAudioPadd.hpp)
//
// The top screen shows what the padds say; the bottom screen (the touch
// screen) shows every padd's controls (AudioPadd's volumes) as tabs
// (HandheldControls) once SELECT shows them. A, B and X play a chime, a knock
// and a blast; Y starts and stops the tune; START quits.
#include <filesystem.h>
#include <nds.h>

#include <AudioPadd.hpp>
#include <HandheldDemo.hpp>
#include <ScratchPadd/AppEnded/AppEnded.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/HandheldControls/ConsolePanel.hpp>
#include <ScratchPadd/Input/GamePadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <deque>
#include <map>
#include <string>
#include <utility>

namespace {

using namespace HandheldDemo;
using ScratchPadd::HandheldControls::printLine;

// Both screens' consoles are 32 columns by 24 rows
constexpr int kColumns = ScratchPadd::HandheldControls::usableColumns(32);
constexpr int kRows = 24;

// Whether the app's file system started (main sets it, before the padds)
bool fileSystem = true;

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
                          kColumns, kRows) {
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
  // The frame loop, on the main thread as in devkitPro's examples
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    // A text console on each screen: the top's for the padds' text, the
    // bottom's for the controls
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleInit(&top_, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&bottom_, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
    status_[0] = "ScreenPadd: main " + threadNumber();
    if (!logWhere.empty())
      status_[7] = "Log: " + logWhere;
    if (!fileSystem) {
      log_.push_back("No file system (nitroFSInit)");
    }
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
  static constexpr int kLogRow = 14;
  static constexpr std::size_t kLogLines = kRows - kLogRow;

  // One frame, then the next (swiWaitForVBlank paces them at 60 a second)
  void frame() {
    if (!pmMainLoop()) {
      system_->stop();
      return;
    }
    swiWaitForVBlank();
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
      if (!controls_.visible()) {
        showControlsHint();
      }
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
    push([this] { frame(); });
  }

  // On the top screen: the title, the status lines (each at its own row),
  // the buttons, then the log
  void draw() {
    printLine(0, kColumns, "ScratchPadd on a DS");
    for (const auto &[row, text] : status_) {
      printLine(kStatusRow + row, kColumns, "%s", text.c_str());
    }
    printLine(kHelpRow, kColumns, "A chime B knock X blast Y tune");
    printLine(kHelpRow + 1, kColumns, "SELECT controls  START quit");
    int row = kLogRow;
    for (const std::string &line : log_) {
      printLine(row++, kColumns, "%s", line.c_str());
    }
  }

  // While the controls are hidden, the bottom screen says how to show them
  void showControlsHint() {
    consoleSelect(&bottom_);
    printLine(0, kColumns, "SELECT: every padd's controls");
    consoleSelect(&top_);
  }

  PrintConsole top_, bottom_;
  ScratchPadd::HandheldControls::ControlTabs controls_;
  std::map<int, std::string> status_;
  std::deque<std::string> log_;
};

} // namespace

int main() {
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
  // The app's file system (NitroFS), packed into the .nds: AudioPadd reads
  // the sounds and the tune from it (nitro:/...). Without it, they don't load
  // and the log says why.
  fileSystem = nitroFSInit(nullptr);
#if defined(SAKBOX_DEBUG)
  // A debug build's log, on the flashcart's SD card (fat:/logs/...)
  ScratchPadd::DebugLog::Session log("scratchpadd-ds-demo");
  logWhere = log.where();
#endif
  {
    ScratchPadd::SystemImplementation<ScratchPadd::Input::GamePadd, ScreenPadd, SoundBoardPadd,
                                      AudioPadd>
        system;
    system.instantiate();
    system.start(); // returns when ScreenPadd stops it (START)
  }
  ScratchPadd::AppEnded::show("ScratchPadd demo");
  return 0;
}
