// dice_fairness - throws Cosmic Hex's physics dice (DiceTray) many times and
// checks they're fair (round 87: the physics decides the roll, so it must be
// fair): how often each face comes up (a sixth each), and each sum against
// two fair dice, by chi-square; how long throws take to settle; and how many
// never do.
//
//   bazel run -c opt //projects/cosmic_hex/dice:dice_fairness -- 20000
//   (dice_fairness_float and dice_fairness_flex_point16: the same in float,
//   and in the DS's 16.16 fixed point)
#include <CosmicHex/Dice/DiceTray.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <string_view>

using CosmicHex::Dice::DiceTray;

int main(int argc, char **argv) {
  int throws = 6000;
  if (argc > 1) {
    const std::string_view text = argv[1];
    std::from_chars(text.data(), text.data() + text.size(), throws);
  }
  constexpr double kStep = 1.0 / 60;
  constexpr double kGiveUp = 20; // seconds: a throw still moving then is counted as stuck
  std::array<long, 7> faces{};
  std::array<long, 13> sums{};
  long stuck = 0;
  double totalSeconds = 0, longest = 0;
  for (int t = 0; t < throws; ++t) {
    DiceTray tray;
    tray.throwDice(0x5eed0000u + static_cast<unsigned>(t));
    double seconds = 0;
    while (!tray.settled() && seconds < kGiveUp) {
      tray.step(kStep);
      seconds += kStep;
    }
    if (!tray.settled()) {
      ++stuck;
      continue;
    }
    const auto [a, b] = tray.faces();
    ++faces[static_cast<std::size_t>(a)];
    ++faces[static_cast<std::size_t>(b)];
    ++sums[static_cast<std::size_t>(a) + static_cast<std::size_t>(b)];
    totalSeconds += tray.secondsToSettle();
    longest = std::max(longest, tray.secondsToSettle());
  }
  const long settled = throws - stuck;
  double chiFaces = 0;
  std::printf("throws %d, settled %ld, stuck %ld\n", throws, settled, stuck);
  std::printf("faces:");
  for (int f = 1; f <= 6; ++f) {
    const double expected = 2.0 * static_cast<double>(settled) / 6;
    const double diff = static_cast<double>(faces[static_cast<std::size_t>(f)]) - expected;
    chiFaces += diff * diff / expected;
    std::printf("  %d: %.2f%%", f, 100.0 * faces[static_cast<std::size_t>(f)] / (2.0 * settled));
  }
  std::printf("\nfaces chi-square %.2f (5 degrees of freedom: under 11.07 is fair at 5%%)\n",
              chiFaces);
  double chiSums = 0;
  std::printf("sums:");
  for (int s = 2; s <= 12; ++s) {
    const double ways = 6 - std::abs(7 - s);
    const double expected = static_cast<double>(settled) * ways / 36;
    const double diff = static_cast<double>(sums[static_cast<std::size_t>(s)]) - expected;
    chiSums += diff * diff / expected;
    std::printf("  %d: %.2f%%", s,
                100.0 * static_cast<double>(sums[static_cast<std::size_t>(s)]) /
                    static_cast<double>(settled));
  }
  std::printf("\nsums chi-square %.2f (10 degrees of freedom: under 18.31 is fair at 5%%)\n",
              chiSums);
  std::printf("settling: %.2f s on average, %.2f s at most\n", totalSeconds / settled, longest);
  return 0;
}
