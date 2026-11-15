// simulate: plays many games of Cosmic Hex between bots and reports what the
// design needs to tune: how often each seat wins (turn order's advantage),
// how many rounds a game lasts, where the winners' points came from, and how
// often a game doesn't finish. It can keep every game's action log, to
// replay or study.
//
//   bazel run -c opt //projects/cosmic_hex/simulate -- --games 100 --players 4 --iterations 200
//   ... --logs /tmp/games        (one action log per game)
#include <CosmicHex/Rules.hpp>

#include <ActionLog/ActionLog.hpp>
#include <GameBots/Bots.hpp>
#include <SeededRandom/Seed.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>

using namespace CosmicHex;

namespace {

struct Options {
  int games = 50;
  int players = 4;
  int iterations = 100;
  int rolloutDepth = 60;
  int maxRounds = 200;
  std::uint64_t seed = 1;
  std::string logs; // a folder to write each game's log to, if given
};

// A flag's number, or a clear stop if it isn't one
template <typename T> T number(std::string_view flag, std::string_view value) {
  T out{};
  const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), out);
  if (error != std::errc{} || end != value.data() + value.size()) {
    std::fprintf(stderr, "%.*s needs a number, not %.*s\n", static_cast<int>(flag.size()),
                 flag.data(), static_cast<int>(value.size()), value.data());
    std::exit(2);
  }
  return out;
}

Options parse(int argc, char **argv) {
  Options options;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string_view flag = argv[i];
    const char *value = argv[i + 1];
    if (flag == "--games")
      options.games = number<int>(flag, value);
    else if (flag == "--players")
      options.players = number<int>(flag, value);
    else if (flag == "--iterations")
      options.iterations = number<int>(flag, value);
    else if (flag == "--rollout")
      options.rolloutDepth = number<int>(flag, value);
    else if (flag == "--max-rounds")
      options.maxRounds = number<int>(flag, value);
    else if (flag == "--seed")
      options.seed = number<std::uint64_t>(flag, value);
    else if (flag == "--logs")
      options.logs = value;
    else {
      std::fprintf(stderr, "unknown flag %s\n", argv[i]);
      std::exit(2);
    }
  }
  return options;
}

// Where a player's points came from
struct Sources {
  int stations = 0, colonies = 0, core = 0, badges = 0, contracts = 0;
};
Sources sourcesOf(const State &state, int player) {
  Sources s;
  const int all = stationsOf(state, player);
  s.stations = all;
  s.core = state.coreHolder == player ? state.settings.coreControlPoints : 0;
  s.badges = state.player[static_cast<std::size_t>(player)].badgePoints;
  for (int id : state.player[static_cast<std::size_t>(player)].revealed)
    s.contracts += contracts()[static_cast<std::size_t>(id)].points;
  s.colonies = points(state, player) - s.stations - s.core - s.badges - s.contracts;
  return s;
}

} // namespace

int main(int argc, char **argv) {
  const Options options = parse(argc, argv);
  GameBots::Ismcts<Game> bot({.rolloutDepth = options.rolloutDepth});
  std::array<int, kMaxPlayers> wins{};
  int finished = 0, totalRounds = 0, totalActions = 0;
  Sources winnerPoints;
  const auto started = std::chrono::steady_clock::now();

  for (int g = 0; g < options.games; ++g) {
    const SeededRandom::Seed seed =
        SeededRandom::Seed::mixedFrom(options.seed * 1000003 + static_cast<std::uint64_t>(g));
    SeededRandom::Generator dealer = seed.stream("dealer"), bots = seed.stream("bots");
    ActionLog::Play<Game> play(newGame(options.players));
    play.addMeta("seed", seed.code());
    play.addMeta("bots", "ismcts " + std::to_string(options.iterations));
    const auto choose = [&](const State &state) {
      return bot.choose(state, bots, {.iterations = options.iterations});
    };
    std::vector<std::function<Action(const State &)>> players(
        static_cast<std::size_t>(options.players), choose);
    // Played turn by turn, so a game that drags on can be stopped at a round
    while (!Game::isOver(play.state()) && play.state().round <= options.maxRounds)
      GameBots::playMatch<Game>(play, players, dealer, 1);

    const State &end = play.state();
    totalActions += static_cast<int>(play.log().actions.size());
    if (end.winner >= 0) {
      ++finished;
      ++wins[static_cast<std::size_t>(end.winner)];
      totalRounds += end.round;
      const Sources s = sourcesOf(end, end.winner);
      winnerPoints.stations += s.stations;
      winnerPoints.colonies += s.colonies;
      winnerPoints.core += s.core;
      winnerPoints.badges += s.badges;
      winnerPoints.contracts += s.contracts;
    }
    if (!options.logs.empty()) {
      std::ofstream file(options.logs + "/game_" + std::to_string(g) + ".log");
      file << ActionLog::write(play.log());
    }
    std::fprintf(stderr, "game %d: %s after %d rounds\n", g + 1,
                 end.winner >= 0 ? ("seat " + std::to_string(end.winner + 1) + " won").c_str()
                                 : "unfinished",
                 end.round);
  }

  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  std::printf("%d games, %d players, ISMCTS %d iterations (%.1f s, %.2f s a game)\n", options.games,
              options.players, options.iterations, seconds, seconds / options.games);
  std::printf("finished: %d (%d stopped at round %d)\n", finished, options.games - finished,
              options.maxRounds);
  if (finished == 0)
    return 0;
  std::printf("wins by seat:");
  for (int p = 0; p < options.players; ++p)
    std::printf("  %d: %d (%.0f%%)", p + 1, wins[static_cast<std::size_t>(p)],
                100.0 * wins[static_cast<std::size_t>(p)] / finished);
  std::printf("\nrounds a game: %.1f on average; actions a game: %.0f\n",
              static_cast<double>(totalRounds) / finished,
              static_cast<double>(totalActions) / options.games);
  std::printf(
      "winners' points, on average: stations %.1f, colonies %.1f, Core Control %.1f, badges %.1f, "
      "contracts %.1f\n",
      static_cast<double>(winnerPoints.stations) / finished,
      static_cast<double>(winnerPoints.colonies) / finished,
      static_cast<double>(winnerPoints.core) / finished,
      static_cast<double>(winnerPoints.badges) / finished,
      static_cast<double>(winnerPoints.contracts) / finished);
  return 0;
}
