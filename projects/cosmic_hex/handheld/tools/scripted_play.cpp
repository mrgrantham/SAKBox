// scripted_play - whole games of Cosmic Hex played through the handheld
// app's own shows and touch panel, player 0 by the scripted player
// (ScriptedGame), over many seeds: a table of every show by how it ended,
// and each one cut short or skipped without the player asking, with what
// the game was doing when it started.
//
//   bazel run //projects/cosmic_hex/handheld:scripted_play -- GAMES [FIRST_SEED] [options]
//     --watch       four bots, watched (no player)
//     --quick       Animations Quick       --off   Animations Off
//     --ds          a DS's slower frames (20 a second)
//     --impatient   B pressed now and then while something animates
//     --quiet       the table only
#include <CosmicHex/Handheld/ScriptedGame.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

using CosmicHex::Handheld::ScriptedGame;
using CosmicHex::Handheld::ScriptedGameSettings;
using CosmicHex::Handheld::ShowLog;

} // namespace

int main(int argc, char **argv) {
  int games = 10;
  std::uint64_t first = 1;
  ScriptedGameSettings settings;
  bool quiet = false, trace = false;
  int numbers = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--watch")
      settings.person = false;
    else if (arg == "--quick")
      settings.animations = CosmicHex::Handheld::Animations::Quick;
    else if (arg == "--off")
      settings.animations = CosmicHex::Handheld::Animations::Off;
    else if (arg == "--impatient")
      settings.impatience = 10;
    else if (arg == "--ds")
      settings.frameSeconds = 1.0 / 20;
    else if (arg == "--quiet")
      quiet = true;
    else if (arg == "--trace")
      trace = true;
    else if (numbers++ == 0)
      games = static_cast<int>(std::strtol(argv[i], nullptr, 10));
    else
      first = std::strtoull(argv[i], nullptr, 10);
  }
  ShowLog all(false);
  std::size_t shows = 0, cut = 0, finished = 0;
  // (each game's own log; the counts summed into one table)
  std::array<std::array<std::size_t, 8>, CosmicHex::Handheld::kShowKinds> counts{};
  for (int g = 0; g < games; ++g) {
    settings.seed = first + static_cast<std::uint64_t>(g);
    ScriptedGame game(settings);
    // (--trace: each of the player's inputs, as the screen stood)
    int inputs = 0;
    if (trace)
      game.onFrame([&inputs](ScriptedGame &g) {
        static double lastLook = 0;
        const bool periodic = g.seconds() - lastLook > 200;
        if ((g.inputs() == inputs && !periodic) || !g.shows().latest())
          return;
        lastLook = g.seconds();
        inputs = g.inputs();
        const auto &state = g.shows().latest()->state;
        std::string moves;
        for (const CosmicHex::Action &move : g.shows().latest()->moves)
          moves += std::to_string(static_cast<int>(move.kind)) + " ";
        std::fprintf(stderr,
                     "%8.2fs input %d: round %d turn %d phase %d mode %d yours %d '%s' %s\n",
                     g.seconds(), inputs, state.round, state.turn, static_cast<int>(state.phase),
                     static_cast<int>(g.screen().mode()), g.shows().latest()->yourMove ? 1 : 0,
                     g.screen().message().c_str(), moves.c_str());
        std::fprintf(stderr,
                     "          held %d pieces %d playing %d; table: phase %d turn %d person %d "
                     "held %d moves %zu\n",
                     g.shows().holding() ? 1 : 0, g.shows().piecesMoving() ? 1 : 0,
                     g.shows().showsPlaying() ? 1 : 0, static_cast<int>(g.table().state().phase),
                     g.table().state().turn, g.table().waitingForPerson() ? 1 : 0,
                     g.tableHeld() ? 1 : 0, g.table().played().size());
      });
    game.run();
    finished += game.table().over() ? 1 : 0;
    for (const ShowLog::Entry &entry : game.log().entries()) {
      ++shows;
      ++counts[static_cast<std::size_t>(entry.kind)][static_cast<std::size_t>(entry.cut())];
      if (entry.cut() == ShowLog::Cut::None || ShowLog::asked(entry.cut()))
        continue;
      ++cut;
      if (!quiet)
        std::printf(
            "seed %llu %8.2fs %-12s %-15s late %.2fs seen %.2f/%.2fs under %s | %s\n",
            static_cast<unsigned long long>(settings.seed), entry.startedAt,
            std::string(CosmicHex::Handheld::nameOf(entry.kind)).c_str(),
            std::string(ShowLog::nameOf(entry.cut())).c_str(), entry.lateBy, entry.seenSeconds,
            entry.movingSeconds,
            entry.covered ? std::string(CosmicHex::Handheld::nameOf(entry.coveredBy)).c_str() : "-",
            entry.note.c_str());
    }
    std::fprintf(stderr, "game %d (seed %llu): %s after %.0f s, %d inputs\n", g + 1,
                 static_cast<unsigned long long>(settings.seed),
                 game.table().over() ? "over" : "unfinished", game.seconds(), game.inputs());
  }
  std::printf("\n%d games (%zu finished), %zu shows, %zu cut short or skipped unasked\n", games,
              finished, shows, cut);
  std::printf("%-14s", "show");
  for (std::size_t c = 0; c < 8; ++c)
    std::printf("%15s", std::string(ShowLog::nameOf(static_cast<ShowLog::Cut>(c))).c_str());
  std::printf("\n");
  for (std::size_t k = 0; k < counts.size(); ++k) {
    std::printf("%-14s", std::string(CosmicHex::Handheld::nameOf(
                                         static_cast<CosmicHex::Handheld::ShowKind>(k)))
                             .c_str());
    for (const std::size_t n : counts[k])
      std::printf("%15zu", n);
    std::printf("\n");
  }
  return 0;
}
