// simulate: plays many games of Cosmic Hex between bots and reports what the
// design needs to tune: how often each seat wins (turn order's advantage),
// how many rounds a game lasts, where the winners' points came from, and how
// often a game doesn't finish. It can keep every game's action log, to
// replay or study.
//
//   bazel run -c opt //projects/cosmic_hex/simulate -- --games 100 --players 4 --iterations 200
//   ... --logs /tmp/games        (one action log per game)
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

namespace {

struct Options {
  int games = 50;
  int players = 4;
  int iterations = 100;
  int rolloutDepth = 60;
  int maxRounds = 200;
  std::uint64_t seed = 1;
  std::string logs;                         // a folder to write each game's log to, if given
  std::string csv;                          // a file to write one line a game to, if given
  int catchUpBelow = 0, catchUpGap = 0;     // catching up (Settings)
  int discardOver = 0;                      // the hand limit on a 7 (Settings)
  bool skipFirst = false;                   // the first player skips their first income
  bool randomFirst = false;                 // a random player goes first, rather than player 1
  std::array<int, kMaxPlayers> seatBonus{}; // extra start resources by turn order
  int empty = 6;              // empty tiles of the zone's 12 (the rest finds, shared out)
  int strongIterations = 0;   // a stronger bot, thinking this much, at one home in turn
  std::string variant;        // a label for the CSV
  std::string core = "seven"; // the core planets: seven (the centre and the six around it) or three
  std::vector<Hex> planets;   // a layout's planets (empty: one every third hex)
  std::vector<Hex> homes;     // and its homes (empty: the usual ones)
  int moons = 0;              // moons among the face-down planets
  std::vector<Hex> zone;      // where the finds are (empty: the usual places)
  std::string layout;         // a layout's name in the layouts file (--layout)
  std::string layoutsFile = "projects/cosmic_hex/layouts/layouts.txt";
  bool smallFinds = false;      // the layout's finds beside its small systems (--finds small)
  int win = 0;                  // points to win (0: the rules' default)
  std::vector<BotStyle> styles; // the players' styles, turned round a seat each game
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

// "q,r,q,r,...": hexes, or a clear stop if it isn't
std::vector<Hex> hexes(std::string_view flag, std::string_view value) {
  std::vector<int> numbers;
  while (!value.empty()) {
    const std::size_t comma = value.find(',');
    numbers.push_back(number<int>(flag, value.substr(0, comma)));
    value.remove_prefix(comma == std::string_view::npos ? value.size() : comma + 1);
  }
  if (numbers.size() % 2 != 0) {
    std::fprintf(stderr, "%.*s needs pairs of numbers (q,r,q,r...)\n",
                 static_cast<int>(flag.size()), flag.data());
    std::exit(2);
  }
  std::vector<Hex> out;
  for (std::size_t i = 0; i < numbers.size(); i += 2)
    out.push_back(Hex{numbers[i], numbers[i + 1]});
  return out;
}

// Reads layout `name` from `file` (layouts.txt: a line a layout, its name
// then homes=, planets= and small_finds=, each q,r pairs) into `options`'
// homes, planets and, if asked, zone; or stops clearly
void readLayout(Options &options) {
  std::ifstream file(options.layoutsFile);
  if (!file) {
    std::fprintf(stderr, "can't read %s (run from the repo's top folder, or give --layouts)\n",
                 options.layoutsFile.c_str());
    std::exit(2);
  }
  for (std::string line; std::getline(file, line);) {
    if (line.empty() || line[0] == '#' || !line.starts_with(options.layout + " "))
      continue;
    std::string_view rest = std::string_view(line).substr(options.layout.size() + 1);
    while (!rest.empty()) {
      const std::size_t space = rest.find(' ');
      const std::string_view field = rest.substr(0, space);
      rest.remove_prefix(space == std::string_view::npos ? rest.size() : space + 1);
      const std::size_t eq = field.find('=');
      const std::string_view key = field.substr(0, eq), value = field.substr(eq + 1);
      if (key == "homes")
        options.homes = hexes("homes", value);
      else if (key == "planets")
        options.planets = hexes("planets", value);
      else if (key == "small_finds" && options.smallFinds)
        options.zone = hexes("small_finds", value);
    }
    return;
  }
  std::fprintf(stderr, "no layout %s in %s\n", options.layout.c_str(), options.layoutsFile.c_str());
  std::exit(2);
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
    else if (flag == "--strong")
      options.strongIterations = number<int>(flag, value);
    else if (flag == "--variant")
      options.variant = value;
    else if (flag == "--planets")
      options.planets = hexes(flag, value);
    else if (flag == "--homes")
      options.homes = hexes(flag, value);
    else if (flag == "--layout")
      options.layout = value;
    else if (flag == "--layouts")
      options.layoutsFile = value;
    else if (flag == "--finds")
      options.smallFinds = std::string_view(value) == "small";
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
  if (!options.layout.empty())
    readLayout(options);
  return options;
}

// Where a player's points came from
// What happened in a game, from its log: how much the blockade did (moves to
// a planet, thefts, resources its planets didn't pay), how often a player
// took a catching-up resource, and each contract's story (dealt, kept,
// revealed, revealed by the winner)
struct GameStats {
  int blockades = 0, thefts = 0, denied = 0, caughtUp = 0, discarded = 0;
  int enforcers = 0; // played
  std::vector<std::array<int, 4>> contracts =
      std::vector<std::array<int, 4>>(CosmicHex::contracts().size());
};
GameStats statsOf(const ActionLog::Log<Game> &log) {
  GameStats stats;
  State state = log.start;
  std::vector<int> revealedBy(stats.contracts.size(), -1);
  for (const Action &action : log.actions) {
    switch (action.kind) {
    case Action::Kind::ChooseHome:
      if (state.homeIncomeLeft > 1)
        ++stats.caughtUp;
      break;
    case Action::Kind::Roll:
      if (action.a + action.b == 7 && state.settings.discardOver > 0) {
        for (const CosmicHex::Player &player : state.player) {
          const int held = std::accumulate(player.resources.begin(), player.resources.end(), 0);
          if (held > state.settings.discardOver)
            stats.discarded += held / 2;
        }
      }
      if (action.a + action.b != 7 && state.blockade >= 0) {
        const Planet &planet = state.planets[static_cast<std::size_t>(state.blockade)];
        if (planet.number == action.a + action.b) {
          for (int d = 0; d < kDocks; ++d) {
            if (planet.docks[static_cast<std::size_t>(d)] >= 0)
              stats.denied += planet.colony[static_cast<std::size_t>(d)]
                                  ? state.settings.colonyPays
                                  : state.settings.stationPays;
          }
        }
      }
      break;
    case Action::Kind::MoveBlockade:
      stats.blockades += action.a >= 0;
      stats.thefts += action.b >= 0;
      break;
    case Action::Kind::DealContracts:
      if (action.a != kEnforcerCard)
        ++stats.contracts[static_cast<std::size_t>(action.a)][0];
      if (action.b >= 0 && action.b != kEnforcerCard)
        ++stats.contracts[static_cast<std::size_t>(action.b)][0];
      break;
    case Action::Kind::PlayEnforcer:
      ++stats.enforcers;
      break;
    case Action::Kind::KeepContract:
      if (action.a == kEnforcerCard)
        break;
      ++stats.contracts[static_cast<std::size_t>(action.a)][1];
      break;
    case Action::Kind::RevealContract:
      ++stats.contracts[static_cast<std::size_t>(action.a)][2];
      revealedBy[static_cast<std::size_t>(action.a)] = state.turn;
      break;
    default:
      break;
    }
    Game::apply(state, action);
  }
  for (std::size_t id = 0; id < revealedBy.size(); ++id)
    stats.contracts[id][3] += revealedBy[id] >= 0 && revealedBy[id] == state.winner;
  return stats;
}

// Stations (homes left out) by the size of the system their planet is in:
// planets next to each other form one system (a single, a pair, a three)
std::array<int, 3> stationsBySystem(const State &state) {
  std::array<int, 3> counts{};
  for (const Planet &planet : state.planets) {
    if (planet.homeOf >= 0 || planet.kind == PlanetKind::Home)
      continue;
    // The size of its system: it and the planets reached through neighbours
    std::vector<Hex> system{planet.hex};
    for (std::size_t i = 0; i < system.size(); ++i)
      for (const Planet &other : state.planets)
        if (other.homeOf < 0 && other.kind != PlanetKind::Home &&
            HexGrid::distance(other.hex, system[i]) == 1 &&
            std::find(system.begin(), system.end(), other.hex) == system.end())
          system.push_back(other.hex);
    const std::size_t size = std::min<std::size_t>(system.size(), 3);
    for (int dock : planet.docks)
      if (dock >= 0)
        ++counts[size - 1];
  }
  return counts;
}

struct Sources {
  int stations = 0, colonies = 0, core = 0, embargo = 0, badges = 0, contracts = 0;
};
Sources sourcesOf(const State &state, int player) {
  Sources s;
  const int all = stationsOf(state, player);
  s.stations = all;
  s.core = state.coreHolder == player ? state.settings.coreControlPoints : 0;
  s.embargo = state.embargoHolder == player ? state.settings.embargoPoints : 0;
  s.badges = state.player[static_cast<std::size_t>(player)].badgePoints;
  for (int id : state.player[static_cast<std::size_t>(player)].revealed)
    s.contracts += contracts()[static_cast<std::size_t>(id)].points;
  s.colonies = points(state, player) - s.stations - s.core - s.embargo - s.badges - s.contracts;
  return s;
}

} // namespace

int main(int argc, char **argv) {
  const Options options = parse(argc, argv);
  GameBots::Ismcts<Game> bot({.rolloutDepth = options.rolloutDepth});
  // A bot for each style, the same search with its own rollouts
  std::array<GameBots::Ismcts<Game>, kBotStyleNames.size()> styledBots;
  for (std::size_t i = 0; i < styledBots.size(); ++i)
    styledBots[i] =
        GameBots::Ismcts<Game>({.rolloutDepth = options.rolloutDepth,
                                .rolloutWeight = rolloutWeightFor(static_cast<BotStyle>(i))});
  std::array<int, kBotStyleNames.size()> styleGames{}, styleWins{};
  std::array<int, kMaxPlayers> wins{};
  int finished = 0, totalRounds = 0, totalActions = 0;
  Sources winnerPoints;
  const auto started = std::chrono::steady_clock::now();
  std::vector<std::array<int, 4>> contractTotals(contracts().size());
  std::ofstream csv;
  if (!options.csv.empty()) {
    csv.open(options.csv);
    csv << "variant,seed,players,iterations,first,strong,winner,rounds,actions,points1,points2,"
           "points3,points4,"
           "stations,colonies,core,badges,contracts,blockades,thefts,denied,caughtup,discarded,"
           "embargo,enforcers,styles,on_singles,on_pairs,on_threes,badges_all\n";
  }

  for (int g = 0; g < options.games; ++g) {
    const SeededRandom::Seed seed =
        SeededRandom::Seed::mixedFrom(options.seed * 1000003 + static_cast<std::uint64_t>(g));
    SeededRandom::Generator dealer = seed.stream("dealer"), bots = seed.stream("bots");
    Settings settings;
    // The smaller core: the centre and planets matching the homes' symmetry
    // (an opposite pair for two and four players; for three, the three a
    // third of the way round from each other, each 2 hexes from a home)
    if (options.core == "three")
      settings.coreHexes = options.players == 3
                               ? std::vector<Hex>{{0, 0}, {2, -1}, {-1, -1}, {-1, 2}}
                               : std::vector<Hex>{{0, 0}, {1, -2}, {-1, 2}};
    settings.seatBonus = options.seatBonus;
    settings.planetHexes = options.planets;
    settings.homeHexes = options.homes;
    settings.moons = options.moons;
    settings.zoneHexes = options.zone;
    if (options.win > 0)
      settings.winPoints = options.win;
    settings.catchUpBelow = options.catchUpBelow;
    settings.catchUpGap = options.catchUpGap;
    settings.discardOver = options.discardOver;
    settings.firstSkipsIncome = options.skipFirst;
    // Fewer empty zone tiles: the rest are finds, given in turn to salvage,
    // ruins, salvage, ruins... (the alien station and the pirates stay one each)
    settings.finds = {options.empty, 0, 0, 1, 1};
    for (int i = 0; i < 12 - options.empty - 2; ++i)
      ++settings.finds[static_cast<std::size_t>(1 + i % 2)];
    // The stronger bot (if any) at a different home each game, in turn
    const int strong = options.strongIterations > 0 ? g % options.players : -1;
    const int first = options.randomFirst ? static_cast<int>(seed.stream("first").below(
                                                static_cast<std::uint32_t>(options.players)))
                                          : 0;
    ActionLog::Play<Game> play(newGame(options.players, settings, first));
    play.addMeta("seed", seed.code());
    play.addMeta("bots", "ismcts " + std::to_string(options.iterations));
    play.addMeta("core", options.core);
    const auto choose = [&](const State &state) {
      return bot.choose(state, bots, {.iterations = options.iterations});
    };
    std::vector<std::function<Action(const State &)>> players(
        static_cast<std::size_t>(options.players), choose);
    if (strong >= 0)
      players[static_cast<std::size_t>(strong)] = [&](const State &state) {
        return bot.choose(state, bots, {.iterations = options.strongIterations});
      };
    // Styles: player p plays styles[(p + g) % count], so each style plays
    // from every home and turn position in turn
    std::vector<BotStyle> styleOf(static_cast<std::size_t>(options.players), BotStyle::Plain);
    if (!options.styles.empty()) {
      for (int p = 0; p < options.players; ++p) {
        const BotStyle style =
            options.styles[static_cast<std::size_t>(p + g) % options.styles.size()];
        styleOf[static_cast<std::size_t>(p)] = style;
        ++styleGames[static_cast<std::size_t>(style)];
        players[static_cast<std::size_t>(p)] = [&, style](const State &state) {
          return styledBots[static_cast<std::size_t>(style)].choose(
              state, bots, {.iterations = options.iterations});
        };
      }
    }
    // Played turn by turn, so a game that drags on can be stopped at a round
    while (!Game::isOver(play.state()) && play.state().round <= options.maxRounds)
      GameBots::playMatch<Game>(play, players, dealer, 1);

    const State &end = play.state();
    totalActions += static_cast<int>(play.log().actions.size());
    if (end.winner >= 0) {
      ++finished;
      if (!options.styles.empty())
        ++styleWins[static_cast<std::size_t>(styleOf[static_cast<std::size_t>(end.winner)])];
      ++wins[static_cast<std::size_t>(end.winner)];
      totalRounds += end.round;
      const Sources s = sourcesOf(end, end.winner);
      winnerPoints.stations += s.stations;
      winnerPoints.colonies += s.colonies;
      winnerPoints.core += s.core;
      winnerPoints.badges += s.badges;
      winnerPoints.contracts += s.contracts;
    }
    if (csv.is_open()) {
      // One line a game: the winner's seat counted from 1 (0: unfinished),
      // everyone's points, and where the winner's came from
      csv << options.variant << "," << seed.code() << "," << options.players << ","
          << options.iterations << "," << first + 1 << "," << strong + 1 << "," << end.winner + 1
          << "," << end.round << "," << play.log().actions.size();
      for (int p = 0; p < kMaxPlayers; ++p)
        csv << "," << (p < options.players ? points(end, p) : 0);
      const Sources s = end.winner >= 0 ? sourcesOf(end, end.winner) : Sources{};
      const GameStats stats = statsOf(play.log());
      csv << "," << s.stations << "," << s.colonies << "," << s.core << "," << s.badges << ","
          << s.contracts << "," << stats.blockades << "," << stats.thefts << "," << stats.denied
          << "," << stats.caughtUp << "," << stats.discarded << "," << s.embargo << ","
          << stats.enforcers << ",";
      for (int p = 0; p < options.players; ++p)
        csv << (p ? ";" : "") << nameOf(styleOf[static_cast<std::size_t>(p)]);
      const std::array<int, 3> bySystem = stationsBySystem(end);
      int badgesAll = 0;
      for (int p = 0; p < options.players; ++p)
        badgesAll += end.player[static_cast<std::size_t>(p)].badgesEarned;
      csv << "," << bySystem[0] << "," << bySystem[1] << "," << bySystem[2] << "," << badgesAll
          << "\n";
      for (std::size_t id = 0; id < stats.contracts.size(); ++id) {
        for (int k = 0; k < 4; ++k)
          contractTotals[id][static_cast<std::size_t>(k)] +=
              stats.contracts[id][static_cast<std::size_t>(k)];
      }
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
