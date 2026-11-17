#include <CosmicHex/Handheld/TableGame.hpp>

#include <CosmicHex/Describe.hpp>
#include <CosmicHex/TradingRule.hpp>

#include <SeededRandom/Seed.hpp>

#include <algorithm>

namespace CosmicHex::Handheld {

namespace {

bool trading(const State &state) {
  return state.phase == Phase::Trading || state.phase == Phase::TradeReply ||
         state.phase == Phase::TradeCounter;
}

} // namespace

TableGame::TableGame(const TableSettings &settings)
    : settings_(settings),
      play_(newGame(settings.players, agreedRules(settings.board),
                    static_cast<int>(SeededRandom::Seed::mixedFrom(settings.seed)
                                         .stream("first")
                                         .below(static_cast<std::uint32_t>(settings.players))))),
      dealer_(SeededRandom::Seed::mixedFrom(settings.seed).stream("dealer")),
      bots_(SeededRandom::Seed::mixedFrom(settings.seed).stream("bots")),
      ismcts_({.rolloutDepth = 60}) {
  play_.addMeta("seed", SeededRandom::Seed::mixedFrom(settings.seed).code());
  play_.addMeta("bots", "ismcts " + std::to_string(settings.botIterations));
}

bool TableGame::waitingForPerson() const {
  const int who = Game::currentPlayer(state());
  return !over() && who != GameBots::kDealer &&
         settings_.seats[static_cast<std::size_t>(who)] == Seat::Person;
}

std::optional<Action> TableGame::step() {
  if (over() || waitingForPerson())
    return std::nullopt;
  const State &now = state();
  const bool dealer = Game::currentPlayer(now) == GameBots::kDealer;
  // The dealer draws; a bot trades by the trading rule and otherwise
  // searches, as the simulator's bots do
  const Action move = dealer ? Game::dealerAction(now, dealer_)
                      : trading(now)
                          ? tradingRuleMove(now)
                          : ismcts_.choose(now, bots_, {.iterations = settings_.botIterations});
  const State before = now;
  if (!play_.play(move))
    return std::nullopt; // (a bug in the rules or a bot: the game stops there)
  record(before, move);
  return move;
}

bool TableGame::play(const Action &move) {
  if (!waitingForPerson())
    return false;
  const std::vector<Action> legal = Game::legalActions(state());
  if (std::find(legal.begin(), legal.end(), move) == legal.end())
    return false;
  const State before = state();
  if (!play_.play(move))
    return false;
  record(before, move);
  return true;
}

void TableGame::record(const State &before, const Action &move) {
  history_.push_back(describe(before, move));
}

} // namespace CosmicHex::Handheld
