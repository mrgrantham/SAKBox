// solve_layouts: the layout solver from the command line, printing the best
// layouts as lines of a layouts file (to add to layouts/layouts.txt and
// simulate with --layout NAME), each with its homes' nearest clusters and
// access spread as a comment above it.
//
//   bazel run -c opt //projects/cosmic_hex/solver:solve_layouts -- --threes 2 --away 3 --farther
//   yes
//
// Flags: --homes edge|corners|three (three: for three players), --singles A-B, --pairs A-B,
// --threes A-B,
// --finds A-B (each a value or a floor-ceiling range), --away N (finds never
// touch clusters of N or more; 0: anywhere), --farther yes (threes farther
// from every home than pairs and singles), --nearest-single, --nearest-pair,
// --nearest-three A-B (from every home, in ship moves), --reach-moves D
// --in-reach A-B (every home has A to B planets within D moves), --attempts
// N, --same-near D (every home the same planets within each distance up to
// D moves; 0: off), --keep N, --seed N, --name PREFIX.
#include <CosmicHex/Solver/LayoutSolver.hpp>

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

using namespace CosmicHex::Solver;

namespace {
// A whole number, or a clear stop if it isn't one
int number(std::string_view text) {
  int out = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
  if (error != std::errc{} || end != text.data() + text.size()) {
    std::fprintf(stderr, "not a number: %.*s\n", static_cast<int>(text.size()), text.data());
    std::exit(2);
  }
  return out;
}

// "N" (exactly) or "A-B" (a floor and a ceiling)
Bound boundOf(std::string_view text) {
  const std::size_t dash = text.find('-');
  if (dash == std::string_view::npos)
    return Bound::exactly(number(text));
  return {number(text.substr(0, dash)), number(text.substr(dash + 1))};
}
} // namespace

int main(int argc, char **argv) {
  SolverSpec spec;
  std::string name = "V";
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string_view flag = argv[i], value = argv[i + 1];
    if (flag == "--homes")
      spec.homes = value == "corners" ? HomePlaces::Corners
                   : value == "three" ? HomePlaces::Three
                                      : HomePlaces::Edge;
    else if (flag == "--singles")
      spec.singles = boundOf(value);
    else if (flag == "--pairs")
      spec.pairs = boundOf(value);
    else if (flag == "--threes")
      spec.threes = boundOf(value);
    else if (flag == "--finds")
      spec.finds = boundOf(value);
    else if (flag == "--away") {
      const int n = number(value);
      spec.findsAwayFromClustersOf = n > 0 ? std::optional<int>(n) : std::nullopt;
    } else if (flag == "--farther")
      spec.threesFartherThanSmaller = value == "yes";
    else if (flag == "--nearest-single")
      spec.nearestFromEveryHome[0] = boundOf(value);
    else if (flag == "--nearest-pair")
      spec.nearestFromEveryHome[1] = boundOf(value);
    else if (flag == "--nearest-three")
      spec.nearestFromEveryHome[2] = boundOf(value);
    else if (flag == "--reach-moves")
      spec.reachMoves = number(value);
    else if (flag == "--in-reach")
      spec.planetsInReach = boundOf(value);
    else if (flag == "--same-near")
      spec.sameNearPlanetsWithin = number(value);
    else if (flag == "--attempts")
      spec.attempts = number(value);
    else if (flag == "--keep")
      spec.keep = number(value);
    else if (flag == "--seed")
      spec.seed = static_cast<std::uint64_t>(number(value));
    else if (flag == "--name")
      name = value;
    else {
      std::fprintf(stderr, "unknown flag %s\n", argv[i]);
      return 2;
    }
  }
  const auto layouts = solve(spec);
  for (std::size_t i = 0; i < layouts.size(); ++i) {
    const SolvedLayout &l = layouts[i];
    std::printf("# access spread %.3f; singles %d, pairs %d, threes %d; nearest from each home:",
                l.accessSpread, l.clusters[0], l.clusters[1], l.clusters[2]);
    for (const auto &n : l.nearest)
      std::printf(" %d/%d/%d", n[0], n[1], n[2]);
    std::printf("; access:");
    for (double a : l.access)
      std::printf(" %.3f", a);
    std::printf("\n%s\n", layoutLine(l, name + std::to_string(i + 1)).c_str());
  }
  std::fprintf(stderr, "%zu layouts\n", layouts.size());
  return 0;
}
