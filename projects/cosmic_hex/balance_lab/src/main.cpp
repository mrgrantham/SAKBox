// balance_lab: Cosmic Hex's balance lab, step 1 (roadmap round 35): choose a
// layout and the rules' settings, run a batch of bot games (eight at once,
// one per SimulationPadd), and watch the results fill in as charts: wins by
// turn position and by home against a fair share, how long games last, and
// where stations end up. Every game is saved as it finishes, a CSV line as
// the simulator writes them, under balance_lab_runs/.
//
//   bazel run -c opt //projects/cosmic_hex/balance_lab
//   bazel run -c opt //projects/cosmic_hex/balance_lab -- --layout R2 --edges yes --games 200 --run
#include "LabWindowPadd.hpp"
#include "SimulationPadd.hpp"

#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

using CosmicHex::Lab::LabWindowPadd;
using CosmicHex::Lab::SimulationPadd;

// A whole number from the command line, or a clear stop
int number(std::string_view flag, std::string_view text) {
  int value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    std::fprintf(stderr, "%.*s needs a number\n", static_cast<int>(flag.size()), flag.data());
    std::exit(2);
  }
  return value;
}

// The batch to show at the start (the simulator's flags, a few of them)
struct StartWith {
  CosmicHex::Batch::Options options;
  bool run = false;
  bool choose = false; // the layouts' thumbnails open
};
StartWith parse(int argc, char **argv) {
  StartWith start;
  CosmicHex::Batch::Options &o = start.options;
  o.games = 200;
  o.iterations = 400;
  o.randomFirst = true;
  o.catchUpBelow = 5; // (the agreed boost below 5 points)
  o.layout = "R2";
  o.edges = true; // (corners and edges: the rules being tested; the user's default)
  for (int i = 1; i < argc; ++i) {
    const std::string_view flag = argv[i];
    if (flag == "--run") {
      start.run = true;
      continue;
    }
    if (flag == "--choose") {
      start.choose = true;
      continue;
    }
    if (i + 1 >= argc) {
      std::fprintf(stderr, "%s needs a value\n", argv[i]);
      std::exit(2);
    }
    const std::string_view value = argv[++i];
    if (flag == "--layout")
      o.layout = value;
    else if (flag == "--layouts")
      o.layoutsFile = value;
    else if (flag == "--edges")
      o.edges = value != "no";
    else if (flag == "--finds")
      o.smallFinds = value == "small";
    else if (flag == "--games")
      o.games = number(flag, value);
    else if (flag == "--iterations")
      o.iterations = number(flag, value);
    else if (flag == "--players")
      o.players = number(flag, value);
    else {
      std::fprintf(stderr, "unknown flag %s\n", argv[i - 1]);
      std::exit(2);
    }
  }
  return start;
}

} // namespace

int main(int argc, char **argv) {
  const StartWith start = parse(argc, argv);
  LabWindowPadd::startWith(start.options, start.run, start.choose);
  ScratchPadd::DebugLog::Session log; // (the terminal and a file)
  ScratchPadd::SystemImplementation<LabWindowPadd, SimulationPadd<0>, SimulationPadd<1>,
                                    SimulationPadd<2>, SimulationPadd<3>, SimulationPadd<4>,
                                    SimulationPadd<5>, SimulationPadd<6>, SimulationPadd<7>>
      system;
  system.instantiate();
  system.start(); // returns when the window closes
  return EXIT_SUCCESS;
}
