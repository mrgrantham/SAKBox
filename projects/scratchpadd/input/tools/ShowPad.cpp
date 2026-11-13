// Prints the controller as the input library reads it, each time it
// changes: its kind, the buttons held (by the pad's own labels), the sticks
// and triggers, and each press. For checking a platform's reader with a
// real pad. Runs for the seconds given (30 by default), or until L, R and
// Start are held together (every pad has them; on a Mac, a PlayStation
// pad's Share never read as held at the same time as Options).
//
//   bazel run //projects/scratchpadd/input:show_pad -- 60
#include <ScratchPadd/Input/Reader.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

using namespace ScratchPadd::Input;

int main(int argc, char **argv) {
  const long seconds = argc > 1 ? std::strtol(argv[1], nullptr, 10) : 30;
  Reader reader;
  Gamepad before = reader.read();
  std::string last;
  std::printf("Reading the pad for %ld s (L, R and Start together to stop)\n", seconds);
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while (std::chrono::steady_clock::now() < end) {
    const Gamepad pad = reader.read();
    std::string line = std::string(profileOf(pad.kind).name) + ":";
    for (std::size_t b = 0; b < kButtons; ++b)
      if (pad.isHeld(Button(b)))
        line += std::string(" ") + pad.labelOf(Button(b));
    char numbers[160];
    std::snprintf(numbers, sizeof numbers,
                  "  left %+.2f %+.2f  right %+.2f %+.2f  triggers %.2f %.2f",
                  axis<double>(pad.left.x), axis<double>(pad.left.y), axis<double>(pad.right.x),
                  axis<double>(pad.right.y), trigger<double>(pad.leftTrigger),
                  trigger<double>(pad.rightTrigger));
    line += numbers;
    const Edges edges(before, pad);
    for (std::size_t b = 0; b < kButtons; ++b)
      if (edges.pressed(Button(b)))
        line += std::string("  [pressed ") + pad.labelOf(Button(b)) + "]";
    if (line != last) {
      std::printf("%s\n", line.c_str());
      std::fflush(stdout);
      last = line;
    }
    before = pad;
    if (pad.isHeld(Button::L) && pad.isHeld(Button::R) && pad.isHeld(Button::Start))
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
}
