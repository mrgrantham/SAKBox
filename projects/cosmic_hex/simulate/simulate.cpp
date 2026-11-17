// simulate: plays many games of Cosmic Hex between bots and reports what the
// design needs to tune: how often each seat wins (turn order's advantage),
// how many rounds a game lasts, where the winners' points came from, and how
// often a game doesn't finish. It can keep every game's action log, to
// replay or study.
//
//   bazel run -c opt //projects/cosmic_hex/simulate -- --games 100 --players 4 --iterations 200
//   ... --logs /tmp/games        (one action log per game)
#include "Batch.hpp"

#include <CosmicHex/BotStyles.hpp>
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
#include <numeric>
#include <string>
#include <string_view>

using namespace CosmicHex;
using Options = CosmicHex::Batch::Options;
namespace Batch = CosmicHex::Batch;

namespace {

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

// Five numbers, one a resource ("1,0,1,0,0": minerals, food, fuel, water,
// crystals), or a clear stop if it isn't
Resources five(std::string_view flag, std::string_view value) {
  Resources out{};
  std::string_view rest = value;
  for (std::size_t i = 0; i < out.size(); ++i) {
    const std::size_t comma = rest.find(',');
    if (rest.empty() || (comma == std::string_view::npos) != (i + 1 == out.size())) {
      std::fprintf(stderr, "%.*s needs five numbers, one a resource (1,0,1,0,0)\n",
                   static_cast<int>(flag.size()), flag.data());
      std::exit(2);
    }
    out[i] = number<int>(flag, rest.substr(0, comma));
    rest.remove_prefix(comma == std::string_view::npos ? rest.size() : comma + 1);
  }
  return out;
}

// "q,r,q,r,...": hexes, or a clear stop if it isn't
std::vector<Hex> hexes(std::string_view flag, std::string_view value) {
  const auto out = Batch::hexesFrom(value);
  if (!out) {
    std::fprintf(stderr, "%.*s needs pairs of numbers (q,r,q,r...)\n",
                 static_cast<int>(flag.size()), flag.data());
    std::exit(2);
  }
  return *out;
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
    else if (flag == "--csv")
      options.csv = value;
    else if (flag == "--core")
      options.core = value;
    else if (flag == "--catch-up-below")
      options.catchUpBelow = number<int>(flag, value);
    else if (flag == "--catch-up-gap")
      options.catchUpGap = number<int>(flag, value);
    else if (flag == "--discard-over")
      options.discardOver = number<int>(flag, value);
    else if (flag == "--skip-first")
      options.skipFirst = std::string_view(value) == "yes";
    else if (flag == "--first")
      options.randomFirst = std::string_view(value) == "random";
    else if (flag == "--seat-bonus") {
      std::string_view rest = value;
      for (int i = 0; i < kMaxPlayers && !rest.empty(); ++i) {
        const std::size_t comma = rest.find(',');
        options.seatBonus[static_cast<std::size_t>(i)] = number<int>(flag, rest.substr(0, comma));
        rest.remove_prefix(comma == std::string_view::npos ? rest.size() : comma + 1);
      }
    } else if (flag == "--empty")
      options.empty = number<int>(flag, value);
    else if (flag == "--outposts")
      options.outposts = number<int>(flag, value);
    else if (flag == "--station-cost")
      options.stationCost = five(flag, value);
    else if (flag == "--ship-cost")
      options.shipCost = five(flag, value);
    else if (flag == "--kinds")
      options.kindWeights = five(flag, value);
    else if (flag == "--exhaust")
      options.exhaustHomesAt = number<int>(flag, value);
    else if (flag == "--home-steps") { // from,amount,from,amount... (0,2,5,1)
      std::string_view rest = value;
      std::vector<int> numbers;
      while (!rest.empty()) {
        const std::size_t comma = rest.find(',');
        numbers.push_back(number<int>(flag, rest.substr(0, comma)));
        rest.remove_prefix(comma == std::string_view::npos ? rest.size() : comma + 1);
      }
      if (numbers.size() % 2 != 0) {
        std::fprintf(stderr, "--home-steps needs pairs: from,amount (0,2,5,1)\n");
        std::exit(2);
      }
      for (std::size_t i = 0; i < numbers.size(); i += 2)
        options.homeIncomeSteps.push_back({numbers[i], numbers[i + 1]});
    } else if (flag == "--finish-round")
      options.finishRound = std::string_view(value) != "no";
    else if (flag == "--core-control")
      options.coreControl = std::string_view(value) == "yes";
    else if (flag == "--first-boost")
      options.firstRoundBoost = std::string_view(value) != "no";
    else if (flag == "--home-die")
      options.homeDie = std::string_view(value) == "yes";
    else if (flag == "--trading")
      options.trading = std::string_view(value) != "no";
    else if (flag == "--cartel")
      options.cartelTake = std::string_view(value) == "all" ? 0 : number<int>(flag, value);
    else if (flag == "--exhaust-kind")
      options.exhaustedKindRandom = std::string_view(value) == "random";
    else if (flag == "--strong")
      options.strongIterations = number<int>(flag, value);
    else if (flag == "--variant")
      options.variant = value;
    else if (flag == "--planets")
      options.planets = hexes(flag, value);
    else if (flag == "--homes")
      options.homes = hexes(flag, value);
    else if (flag == "--edges")
      options.edges = std::string_view(value) == "yes";
    else if (flag == "--start")
      options.deepSpace = std::string_view(value) == "deep-space";
    else if (flag == "--move")
      options.move = number<int>(flag, value);
    else if (flag == "--boost")
      options.boost = number<int>(flag, value);
    else if (flag == "--layout")
      options.layout = value;
    else if (flag == "--layouts")
      options.layoutsFile = value;
    else if (flag == "--finds")
      options.smallFinds = std::string_view(value) != "usual";
    else if (flag == "--zone")
      options.zone = hexes(flag, value);
    else if (flag == "--moons")
      options.moons = number<int>(flag, value);
    else if (flag == "--win")
      options.win = number<int>(flag, value);
    else if (flag == "--styles") {
      std::string_view rest = value;
      while (!rest.empty()) {
        const std::size_t comma = rest.find(',');
        const auto style = botStyleNamed(rest.substr(0, comma));
        if (!style) {
          std::fprintf(stderr, "--styles takes plain, saver, rusher and explorer\n");
          std::exit(2);
        }
        options.styles.push_back(*style);
        rest.remove_prefix(comma == std::string_view::npos ? rest.size() : comma + 1);
      }
    } else {
      std::fprintf(stderr, "unknown flag %s\n", argv[i]);
      std::exit(2);
    }
  }
  if (!options.layout.empty()) {
    if (const auto problem = Batch::readLayout(options)) {
      std::fprintf(stderr, "%s (run from the repo's top folder, or give --layouts)\n",
                   problem->c_str());
      std::exit(2);
    }
  }
  return options;
}

} // namespace

int main(int argc, char **argv) {
  const Options options = parse(argc, argv);
  const Batch::Bots bots(options);
  std::array<int, kBotStyleNames.size()> styleGames{}, styleWins{};
  std::array<int, kMaxPlayers> wins{};
  int finished = 0, totalRounds = 0, totalActions = 0;
  Batch::Sources winnerPoints;
  const auto started = std::chrono::steady_clock::now();
  std::vector<std::array<int, 4>> contractTotals(contracts().size());
  std::ofstream csv;
  if (!options.csv.empty()) {
    csv.open(options.csv);
    csv << Batch::csvHeader();
  }

  for (int g = 0; g < options.games; ++g) {
    const Batch::GameResult result = Batch::playGame(options, g, bots);
    totalActions += result.actions;
    if (!options.styles.empty())
      for (BotStyle style : result.styles)
        ++styleGames[static_cast<std::size_t>(style)];
    if (result.winner >= 0) {
      ++finished;
      if (!options.styles.empty())
        ++styleWins[static_cast<std::size_t>(
            result.styles[static_cast<std::size_t>(result.winner)])];
      ++wins[static_cast<std::size_t>(result.winner)];
      totalRounds += result.rounds;
      const Batch::Sources &s = result.winnerSources;
      winnerPoints.stations += s.stations;
      winnerPoints.colonies += s.colonies;
      winnerPoints.core += s.core;
      winnerPoints.badges += s.badges;
      winnerPoints.contracts += s.contracts;
    }
    if (csv.is_open()) {
      csv << Batch::csvLine(options, result);
      for (std::size_t id = 0; id < result.stats.contracts.size(); ++id)
        for (int k = 0; k < 4; ++k)
          contractTotals[id][static_cast<std::size_t>(k)] +=
              result.stats.contracts[id][static_cast<std::size_t>(k)];
    }
    if (!options.logs.empty()) {
      std::ofstream file(options.logs + "/game_" + std::to_string(g) + ".log");
      file << ActionLog::write(result.log);
    }
    std::fprintf(stderr, "game %d: %s after %d rounds\n", g + 1,
                 result.winner >= 0 ? ("seat " + std::to_string(result.winner + 1) + " won").c_str()
                                    : "unfinished",
                 result.rounds);
  }

  if (!options.csv.empty()) {
    // Each contract: how often dealt, kept, revealed, and revealed by the
    // game's winner, over the run
    std::ofstream totals(options.csv + ".contracts.csv");
    totals << "id,name,points,dealt,kept,revealed,winner_revealed\n";
    for (std::size_t id = 0; id < contractTotals.size(); ++id) {
      totals << id << ",\"" << contracts()[id].name << "\"," << contracts()[id].points;
      for (int count : contractTotals[id])
        totals << "," << count;
      totals << "\n";
    }
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
  if (!options.styles.empty()) {
    // How often each style won the games it played (a fair share is one in
    // the number of players)
    std::printf("wins by style:");
    for (std::size_t i = 0; i < styleGames.size(); ++i)
      if (styleGames[i] > 0)
        std::printf("  %s: %.1f%% of %d", std::string(kBotStyleNames[i]).c_str(),
                    100.0 * styleWins[i] / styleGames[i], styleGames[i]);
    std::printf("\n");
  }
  return 0;
}
