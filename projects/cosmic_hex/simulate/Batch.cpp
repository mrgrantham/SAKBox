// Batch.cpp - see Batch.hpp
#include "Batch.hpp"

#include <CosmicHex/TradingRule.hpp>

#include <SeededRandom/Seed.hpp>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <functional>
#include <numeric>
#include <sstream>

namespace CosmicHex::Batch {

std::optional<std::vector<Hex>> hexesFrom(std::string_view text) {
  std::vector<int> numbers;
  while (!text.empty()) {
    const std::size_t comma = text.find(',');
    const std::string_view part = text.substr(0, comma);
    int value = 0;
    const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
    if (error != std::errc{} || end != part.data() + part.size())
      return std::nullopt;
    numbers.push_back(value);
    text.remove_prefix(comma == std::string_view::npos ? text.size() : comma + 1);
  }
  if (numbers.size() % 2 != 0)
    return std::nullopt;
  std::vector<Hex> out;
  for (std::size_t i = 0; i < numbers.size(); i += 2)
    out.push_back(Hex{numbers[i], numbers[i + 1]});
  return out;
}

std::optional<std::string> readLayout(Options &options) {
  std::ifstream file(options.layoutsFile);
  if (!file)
    return "can't read " + options.layoutsFile;
  for (std::string line; std::getline(file, line);) {
    if (line.empty() || line[0] == '#' || !line.starts_with(options.layout + " "))
      continue;
    std::string_view rest = std::string_view(line).substr(options.layout.size() + 1);
    while (!rest.empty()) {
      const std::size_t space = rest.find(' ');
      const std::string_view field = rest.substr(0, space);
      rest.remove_prefix(space == std::string_view::npos ? rest.size() : space + 1);
      const std::size_t eq = field.find('=');
      if (eq == std::string_view::npos)
        continue;
      const std::string_view key = field.substr(0, eq), value = field.substr(eq + 1);
      std::vector<Hex> *into = key == "homes"                               ? &options.homes
                               : key == "planets"                           ? &options.planets
                               : key == "zone"                              ? &options.zone
                               : key == "small_finds" && options.smallFinds ? &options.zone
                                                                            : nullptr;
      if (!into)
        continue; // (other fields, such as reach=, are for people reading the file)
      const auto hexes = hexesFrom(value);
      if (!hexes)
        return "layout " + options.layout + ": " + std::string(key) + " isn't q,r pairs";
      *into = *hexes;
    }
    return std::nullopt;
  }
  return "no layout " + options.layout + " in " + options.layoutsFile;
}

std::vector<std::string> layoutNames(const std::string &fileName) {
  std::vector<std::string> names;
  std::ifstream file(fileName);
  for (std::string line; std::getline(file, line);) {
    if (line.empty() || line[0] == '#')
      continue;
    names.push_back(line.substr(0, line.find(' ')));
  }
  return names;
}

Settings settingsFor(const Options &options) {
  Settings settings;
  // The smaller core: the centre and planets matching the homes' symmetry
  // (an opposite pair for two and four players; for three, the three a
  // third of the way round from each other, each 2 hexes from a home)
  if (options.core == "three")
    settings.coreHexes = options.players == 3 ? std::vector<Hex>{{0, 0}, {2, -1}, {-1, -1}, {-1, 2}}
                                              : std::vector<Hex>{{0, 0}, {1, -2}, {-1, 2}};
  settings.seatBonus = options.seatBonus;
  settings.planetHexes = options.planets;
  settings.homeHexes = options.homes;
  settings.moons = options.moons;
  settings.zoneHexes = options.zone;
  // Corners and edges: ships move 2 edges, 1 more for fuel,
  // unless --move and --boost say otherwise
  settings.corners = options.edges;
  settings.deepSpaceStart = options.deepSpace;
  if (options.edges) {
    settings.shipMove = 2;
    settings.boostMove = 1;
  }
  if (options.move > 0)
    settings.shipMove = options.move;
  if (options.boost > 0)
    settings.boostMove = options.boost;
  if (options.win > 0)
    settings.winPoints = options.win;
  if (options.relayPoints >= 0)
    settings.relayPoints = options.relayPoints;
  if (options.dominionPoints >= 0)
    settings.dominionPoints = options.dominionPoints;
  if (!options.charters.empty())
    settings.charters = options.charters;
  else if (options.players == 3)
    settings.winPoints = kThreePlayerWinPoints; // (the agreed rules' for three)
  settings.catchUpBelow = options.catchUpBelow;
  settings.catchUpGap = options.catchUpGap;
  settings.discardOver = options.discardOver;
  settings.firstSkipsIncome = options.skipFirst;
  // The zone's 12: the empty ones, the outposts and the pirates, and the
  // rest given in turn to salvage, ruins, salvage, ruins...
  settings.finds = {options.empty, 0, 0, options.outposts, 1};
  for (int i = 0; i < 12 - options.empty - options.outposts - 1; ++i)
    ++settings.finds[static_cast<std::size_t>(1 + i % 2)];
  if (options.stationCost)
    settings.stationCost = *options.stationCost;
  if (options.shipCost)
    settings.shipCost = *options.shipCost;
  settings.kindWeights = options.kindWeights;
  settings.exhaustHomesAt = options.exhaustHomesAt;
  settings.cartelTake = options.cartelTake;
  settings.playerTrading = options.trading;
  settings.homeDie = options.homeDie;
  settings.finishRound = options.finishRound;
  if (!options.coreControl)
    settings.coreControlPoints = 0;
  settings.firstRoundBoost = options.firstRoundBoost;
  settings.homeIncomeSteps = options.homeIncomeSteps;
  settings.exhaustedKindRandom = options.exhaustedKindRandom;
  // (--setting: each as the logs write it, appended so it wins)
  for (const std::string &field : options.settings) {
    const auto changed = settingsFromLine(settingsLine(settings) + " " + field);
    if (!changed) {
      std::fprintf(stderr, "--setting %s: not a setting the rules read\n", field.c_str());
      std::exit(2);
    }
    settings = *changed;
  }
  return settings;
}

GameStats statsOf(const ActionLog::Log<Game> &log) {
  GameStats stats;
  State state = log.start;
  std::vector<int> revealedBy(stats.contracts.size(), -1);
  // Points at each turn's end, to follow the lead
  std::vector<std::pair<int, std::array<int, kMaxPlayers>>> turnEnds; // (round, points)
  for (const Action &action : log.actions) {
    switch (action.kind) {
    case Action::Kind::ChooseHome:
      if (state.homeIncomeLeft > 1)
        ++stats.caughtUp;
      break;
    case Action::Kind::Roll:
      if (action.a + action.b == 7) {
        ++stats.sevens;
        bool someoneDiscarded = false;
        for (const CosmicHex::Player &player : state.player) {
          const int held = std::accumulate(player.resources.begin(), player.resources.end(), 0);
          if (state.settings.discardOver > 0 && held > state.settings.discardOver) {
            stats.discarded += held / 2;
            someoneDiscarded = true;
          }
        }
        stats.sevensWithDiscards += someoneDiscarded;
      }
      if (action.a + action.b != 7 && state.blockade >= 0) {
        const Planet &planet = state.planets[static_cast<std::size_t>(state.blockade)];
        if (planet.number == action.a + action.b && state.settings.corners) {
          // (corners) every station touching it
          for (const Station &station : state.stations)
            for (Hex hex : HexGrid::hexesTouching(station.at))
              if (hex == planet.hex)
                stats.denied +=
                    station.colony ? state.settings.colonyPays : state.settings.stationPays;
        } else if (planet.number == action.a + action.b) {
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
    // (Charters only: the table has a row for each, and an Enforcer's or a
    // Cartel's id is past its end)
    case Action::Kind::DealContracts:
      if (isCharter(action.a))
        ++stats.contracts[static_cast<std::size_t>(action.a)][0];
      if (isCharter(action.b))
        ++stats.contracts[static_cast<std::size_t>(action.b)][0];
      break;
    case Action::Kind::PlayEnforcer:
      ++stats.enforcers;
      break;
    case Action::Kind::PlayCartel:
      ++stats.cartels;
      break;
    case Action::Kind::PlayShipyard:
      ++stats.shipyards;
      break;
    case Action::Kind::PlaySupplyDrop:
      ++stats.supplyDrops;
      break;
    case Action::Kind::Trade: {
      // By how many it took: the bank's rate, or an outpost's by ship or station
      const auto me = static_cast<std::size_t>(state.turn);
      const int held = state.player[me].resources[static_cast<std::size_t>(action.a)];
      State after = state;
      if (Game::apply(after, action)) {
        const int given = held - after.player[me].resources[static_cast<std::size_t>(action.a)];
        const Settings &s = state.settings;
        if (given == s.alienStationRate && s.alienStationRate < s.bankRate)
          ++stats.stationOutpostTrades[me];
        else if (given == s.alienRate && s.alienRate < s.bankRate)
          ++stats.shipOutpostTrades[me];
        else
          ++stats.bankTrades[me];
      }
      break;
    }
    case Action::Kind::OfferTrade:
      ++stats.offersMade[static_cast<std::size_t>(state.turn)];
      break;
    case Action::Kind::AcceptTrade:
      if (state.phase == Phase::TradeReply || state.phase == Phase::TradeCounter) {
        const bool counter = state.phase == Phase::TradeCounter;
        const Resources give = resourcesOf(counter ? state.offer.counterGive : state.offer.give),
                        get = resourcesOf(counter ? state.offer.counterGet : state.offer.get);
        const int moved = std::accumulate(give.begin(), give.end(), 0) +
                          std::accumulate(get.begin(), get.end(), 0);
        for (int p : {state.turn, state.offer.replier}) {
          ++stats.playerTrades[static_cast<std::size_t>(p)];
          stats.resourcesTraded[static_cast<std::size_t>(p)] += moved;
        }
      }
      break;
    case Action::Kind::KeepContract:
      if (isCharter(action.a))
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
    if (action.kind == Action::Kind::EndTurn || Game::isOver(state)) {
      std::array<int, kMaxPlayers> now{};
      for (int p = 0; p < state.players; ++p)
        now[static_cast<std::size_t>(p)] = points(state, p);
      turnEnds.emplace_back(state.round, now);
    }
  }
  // The lead: whoever has the most points alone at a turn's end (a tie
  // keeps it with its holder)
  int leader = -1;
  for (const auto &[round, now] : turnEnds) {
    const auto best = std::max_element(now.begin(), now.begin() + state.players);
    const bool alone = std::count(now.begin(), now.begin() + state.players, *best) == 1;
    const int top = static_cast<int>(best - now.begin());
    if (alone && top != leader) {
      if (leader >= 0)
        ++stats.leadChanges;
      leader = top;
      if (top == state.winner)
        stats.leadTakenRound = round;
    }
    if (state.winner >= 0)
      stats.comeback =
          std::max(stats.comeback, *best - now[static_cast<std::size_t>(state.winner)]);
  }
  for (std::size_t id = 0; id < revealedBy.size(); ++id)
    stats.contracts[id][3] += revealedBy[id] >= 0 && revealedBy[id] == state.winner;
  return stats;
}

std::array<int, 3> stationsBySystem(const State &state) {
  std::array<int, 3> counts{};
  // Each planet's system size: it and the planets reached through neighbours
  const auto systemSize = [&](Hex start) {
    std::vector<Hex> system{start};
    for (std::size_t i = 0; i < system.size(); ++i)
      for (const Planet &other : state.planets)
        if (other.homeOf < 0 && other.kind != PlanetKind::Home &&
            HexGrid::distance(other.hex, system[i]) == 1 &&
            std::find(system.begin(), system.end(), other.hex) == system.end())
          system.push_back(other.hex);
    return std::min<std::size_t>(system.size(), 3);
  };
  if (state.settings.corners) {
    // A station counts for the biggest system it touches (homes left out)
    for (const Station &station : state.stations) {
      std::size_t biggest = 0;
      for (Hex hex : HexGrid::hexesTouching(station.at))
        for (const Planet &planet : state.planets)
          if (planet.hex == hex && planet.homeOf < 0 && planet.kind != PlanetKind::Home)
            biggest = std::max(biggest, systemSize(hex));
      if (biggest > 0)
        ++counts[biggest - 1];
    }
    return counts;
  }
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

Sources sourcesOf(const State &state, int player) {
  Sources s;
  const int all = stationsOf(state, player);
  s.stations = all;
  s.core = state.coreHolder == player ? state.settings.coreControlPoints : 0;
  s.embargo = state.embargoHolder == player ? state.settings.embargoPoints : 0;
  s.badges = state.player[static_cast<std::size_t>(player)].badgePoints;
  for (int id : state.player[static_cast<std::size_t>(player)].revealed)
    s.contracts += charterPoints(state.settings, id);
  s.dominion = state.dominionHolder == player ? state.settings.dominionPoints : 0;
  s.colonies =
      points(state, player) - s.stations - s.core - s.embargo - s.dominion - s.badges - s.contracts;
  return s;
}

Bots::Bots(const Options &options) : plain({.rolloutDepth = options.rolloutDepth}) {
  for (std::size_t i = 0; i < styled.size(); ++i)
    styled[i] =
        GameBots::Ismcts<Game>({.rolloutDepth = options.rolloutDepth,
                                .rolloutWeight = rolloutWeightFor(static_cast<BotStyle>(i))});
}

GameResult playGame(const Options &options, int game, const Bots &bots) {
  const SeededRandom::Seed seed =
      SeededRandom::Seed::mixedFrom(options.seed * 1000003 + static_cast<std::uint64_t>(game));
  SeededRandom::Generator dealer = seed.stream("dealer"), random = seed.stream("bots");
  GameResult result;
  result.game = game;
  result.seed = seed.code();
  // The stronger bot (if any) at a different home each game, in turn
  result.strong = options.strongIterations > 0 ? game % options.players : -1;
  result.first = options.randomFirst ? static_cast<int>(seed.stream("first").below(
                                           static_cast<std::uint32_t>(options.players)))
                                     : 0;
  ActionLog::Play<Game> play(newGame(options.players, settingsFor(options), result.first));
  play.addMeta("seed", seed.code());
  play.addMeta("bots", "ismcts " + std::to_string(options.iterations));
  play.addMeta("core", options.core);
  std::vector<std::function<Action(const State &)>> players(
      static_cast<std::size_t>(options.players), [&](const State &state) {
        return bots.plain.choose(
            state, random,
            {.iterations = options.iterations, .frameArenaBytes = options.frameArenaBytes});
      });
  // (does thinking beat chance? the others pick any legal move)
  if (options.randomOthers)
    for (auto &player : players)
      player = [&](const State &state) {
        const std::vector<Action> moves = Game::legalActions(state);
        return moves[random.below(static_cast<std::uint32_t>(moves.size()))];
      };
  if (result.strong >= 0)
    players[static_cast<std::size_t>(result.strong)] = [&](const State &state) {
      return bots.plain.choose(
          state, random,
          {.iterations = options.strongIterations, .frameArenaBytes = options.frameArenaBytes});
    };
  // Styles: player p plays styles[(p + game) % count], so each style plays
  // from every home and turn position in turn
  result.styles.assign(static_cast<std::size_t>(options.players), BotStyle::Plain);
  if (!options.styles.empty()) {
    for (int p = 0; p < options.players; ++p) {
      const BotStyle style =
          options.styles[static_cast<std::size_t>(p + game) % options.styles.size()];
      result.styles[static_cast<std::size_t>(p)] = style;
      players[static_cast<std::size_t>(p)] = [&, style](const State &state) {
        return bots.styled[static_cast<std::size_t>(style)].choose(
            state, random,
            {.iterations = options.iterations, .frameArenaBytes = options.frameArenaBytes});
      };
    }
  }
  // Trading between players: every bot by the trading rule (TradingRule.hpp)
  for (auto &player : players)
    player = [search = player](const State &state) {
      const bool trading = state.phase == Phase::Trading || state.phase == Phase::TradeReply ||
                           state.phase == Phase::TradeCounter;
      return trading ? tradingRuleMove(state) : search(state);
    };
  // Played turn by turn, so a game that drags on can be stopped at a round
  while (!Game::isOver(play.state()) && play.state().round <= options.maxRounds)
    GameBots::playMatch<Game>(play, players, dealer, 1);

  const State &end = play.state();
  // A shared win counted as one of its winners, drawn at random,
  // so the seats' win shares stay fair (the rules name the first in turn
  // order)
  result.winner = end.winners.size() > 1
                      ? end.winners[seed.stream("shared win")
                                        .below(static_cast<std::uint32_t>(end.winners.size()))]
                      : end.winner;
  result.sharedWin = end.winners.size() > 1;
  result.rounds = end.round;
  result.actions = static_cast<int>(play.log().actions.size());
  for (int p = 0; p < options.players; ++p) {
    result.points[static_cast<std::size_t>(p)] = points(end, p);
    result.badgesAll += end.player[static_cast<std::size_t>(p)].badgesEarned;
  }
  if (end.winner >= 0)
    result.winnerSources = sourcesOf(end, end.winner);
  result.stats = statsOf(play.log());
  result.bySystem = stationsBySystem(end);
  result.log = play.log();
  return result;
}

std::string csvHeader() {
  return "variant,seed,players,iterations,first,strong,winner,rounds,actions,points1,points2,"
         "points3,points4,"
         "stations,colonies,core,badges,contracts,blockades,thefts,denied,caughtup,discarded,"
         "embargo,enforcers,styles,on_singles,on_pairs,on_threes,badges_all,"
         // the game's, then the winner's and everyone's together
         "cartels,lead_changes,lead_taken_round,comeback,sevens,sevens_discarding,"
         "w_bank,w_ship_outpost,w_station_outpost,w_player_trades,w_traded,w_offers,"
         "all_bank,all_ship_outpost,all_station_outpost,all_player_trades,all_traded,all_offers,"
         "shipyards,supply_drops\n";
}

std::string csvLine(const Options &options, const GameResult &result) {
  // The winner's seat counted from 1 (0: unfinished), everyone's points,
  // and where the winner's came from
  std::ostringstream csv;
  csv << options.variant << "," << result.seed << "," << options.players << ","
      << options.iterations << "," << result.first + 1 << "," << result.strong + 1 << ","
      << result.winner + 1 << "," << result.rounds << "," << result.actions;
  for (int p = 0; p < kMaxPlayers; ++p)
    csv << "," << (p < options.players ? result.points[static_cast<std::size_t>(p)] : 0);
  const Sources &s = result.winnerSources;
  const GameStats &stats = result.stats;
  csv << "," << s.stations << "," << s.colonies << "," << s.core << "," << s.badges << ","
      << s.contracts << "," << stats.blockades << "," << stats.thefts << "," << stats.denied << ","
      << stats.caughtUp << "," << stats.discarded << "," << s.embargo << "," << stats.enforcers
      << ",";
  for (std::size_t p = 0; p < result.styles.size(); ++p)
    csv << (p ? ";" : "") << nameOf(result.styles[p]);
  csv << "," << result.bySystem[0] << "," << result.bySystem[1] << "," << result.bySystem[2] << ","
      << result.badgesAll;
  csv << "," << stats.cartels << "," << stats.leadChanges << "," << stats.leadTakenRound << ","
      << stats.comeback << "," << stats.sevens << "," << stats.sevensWithDiscards;
  const std::array<const std::array<int, kMaxPlayers> *, 6> perPlayer = {
      &stats.bankTrades,   &stats.shipOutpostTrades, &stats.stationOutpostTrades,
      &stats.playerTrades, &stats.resourcesTraded,   &stats.offersMade};
  for (const auto *counts : perPlayer) // (the winner's)
    csv << "," << (result.winner >= 0 ? (*counts)[static_cast<std::size_t>(result.winner)] : 0);
  for (const auto *counts : perPlayer) // (everyone's)
    csv << "," << std::accumulate(counts->begin(), counts->end(), 0);
  csv << "," << stats.shipyards << "," << stats.supplyDrops << "\n";
  return csv.str();
}

} // namespace CosmicHex::Batch
