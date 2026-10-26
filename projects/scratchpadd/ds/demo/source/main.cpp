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
// A, B and X play a chime, a knock and a blast; Y starts and stops the
// tune; START quits.
#include <nds.h>

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
  // The frame loop, on the main thread as in devkitPro's examples
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    consoleDemoInit();
    status_[0] = "ScreenPadd: main " + threadNumber();
    push([this] { frame(); });
  }

private:
  static constexpr int kStatusRow = 3; // status lines 0-7 go from here
  static constexpr int kHelpRow = 11;
  static constexpr int kLogRow = 13;
  static constexpr std::size_t kLogLines = 11;

  // One frame, then the next (swiWaitForVBlank paces them at 60 a second)
  void frame() {
    if (!pmMainLoop()) {
      system_->stop();
      return;
    }
    swiWaitForVBlank();
    scanKeys();
    const int down = keysDown();
    if (down & KEY_START) {
      system_->stop();
      return;
    }
    static const std::pair<int, const char *> kButtons[] = {
        {KEY_A, "A"}, {KEY_B, "B"}, {KEY_X, "X"}, {KEY_Y, "Y"}};
    for (const auto &[key, button] : kButtons) {
      if (down & key) {
        send(ButtonPressed{button});
      }
    }
    draw();
    push([this] { frame(); });
  }

  // The title, the status lines (each at its own row), the buttons, then
  // the log (rows are 1-based; the console is 32 columns by 24 rows)
  void draw() {
    std::printf("\x1b[1;1HScratchPadd on a DS\x1b[K");
    for (const auto &[row, text] : status_) {
      std::printf("\x1b[%d;1H%.32s\x1b[K", kStatusRow + row, text.c_str());
    }
    std::printf("\x1b[%d;1HA chime B knock X blast Y tune\x1b[K", kHelpRow);
    int row = kLogRow;
    for (const std::string &line : log_) {
      std::printf("\x1b[%d;1H%.32s\x1b[K", row++, line.c_str());
    }
  }

  std::map<int, std::string> status_;
  std::deque<std::string> log_;
};

} // namespace

int main() {
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
  ScratchPadd::SystemImplementation<ScreenPadd, SoundBoardPadd, AudioPadd> system;
  system.instantiate();
  system.start(); // returns when ScreenPadd stops it (START)
  return 0;
}
