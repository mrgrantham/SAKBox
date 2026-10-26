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
// tune; START quits.
#include <3ds.h>

#include <AudioPadd.hpp>
#include <HandheldDemo.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <deque>
#include <map>
#include <string>

namespace {

using namespace HandheldDemo;

class ScreenPadd : public ScratchPadd::Base {
public:
  ScreenPadd(ScratchPadd::System *system) : Base(system) {
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
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    consoleInit(GFX_TOP, &top_);
    status_[0] = "ScreenPadd: main thread " + threadNumber();
    push([this] { frame(); });
  }

private:
  static constexpr int kStatusRow = 3; // status lines 0-7 go from here
  static constexpr int kHelpRow = 12;
  static constexpr int kLogRow = 14;
  static constexpr std::size_t kLogLines = 16;

  // One frame, then the next (gspWaitForVBlank paces them at 60 a second)
  void frame() {
    if (!aptMainLoop()) {
      system_->stop();
      return;
    }
    hidScanInput();
    const u32 down = hidKeysDown();
    if (down & KEY_START) {
      system_->stop();
      return;
    }
    static const std::pair<u32, const char *> kButtons[] = {
        {KEY_A, "A"}, {KEY_B, "B"}, {KEY_X, "X"}, {KEY_Y, "Y"}};
    for (const auto &[key, button] : kButtons) {
      if (down & key) {
        send(ButtonPressed{button});
      }
    }
    draw();
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
    push([this] { frame(); });
  }

  // The title, the status lines (each at its own row), the buttons, then
  // the log (rows are 1-based; the top screen's console is 50 columns by 30
  // rows)
  void draw() {
    std::printf("\x1b[1;1HScratchPadd on a 3DS: padds talking by messages\x1b[K");
    for (const auto &[row, text] : status_) {
      std::printf("\x1b[%d;1H%.49s\x1b[K", kStatusRow + row, text.c_str());
    }
    std::printf("\x1b[%d;1HA chime  B knock  X blast  Y tune  START quit\x1b[K", kHelpRow);
    int row = kLogRow;
    for (const std::string &line : log_) {
      std::printf("\x1b[%d;1H%.49s\x1b[K", row++, line.c_str());
    }
  }

  PrintConsole top_;
  std::map<int, std::string> status_;
  std::deque<std::string> log_;
};

} // namespace

int main() {
  gfxInitDefault();
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
  {
    ScratchPadd::SystemImplementation<ScreenPadd, SoundBoardPadd, AudioPadd> system;
    system.instantiate();
    system.start(); // returns when ScreenPadd stops it (START, or the HOME Menu closing it)
  }
  gfxExit();
  return 0;
}
