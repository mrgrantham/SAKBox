#include <CosmicHex/Handheld/TableGame.hpp>

#include <CosmicHex/Describe.hpp>
#include <CosmicHex/TradingRule.hpp>

#include <SeededRandom/Seed.hpp>

#include <algorithm>
#include <chrono>

namespace CosmicHex::Handheld {

namespace {

bool trading(const State &state) {
  return state.phase == Phase::Trading || state.phase == Phase::TradeReply ||
         state.phase == Phase::TradeCounter;
}

} // namespace

Settings rulesFor(const TableSettings &settings) {
  Settings rules = agreedRules(settings.board, settings.players);
  rules.finishRound = settings.finishRound;
  rules.seatBonus = settings.seatBonus;
  return rules;
}

TableGame::TableGame(const TableSettings &settings)
    : settings_(settings),
      play_(newGame(settings.players, rulesFor(settings),
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

bool TableGame::waitingForDice() const {
  return settings_.physicsDice && !over() && state().phase == Phase::RollDraw;
}

bool TableGame::rollDice(int first, int second) {
  if (!waitingForDice())
    return false;
  const Action roll{Action::Kind::Roll, first, second};
  const State before = state();
  if (!play_.play(roll))
    return false;
  record(before, roll);
  return true;
}

std::optional<Action> TableGame::step() {
  if (over() || waitingForPerson() || waitingForDice())
    return std::nullopt;
  const State &now = state();
  const bool dealer = Game::currentPlayer(now) == GameBots::kDealer;
  // The dealer draws; a bot trades by the trading rule and otherwise
  // searches, as the simulator's bots do
  // (by time, if set: up to botSeconds, at most botIterations)
  GameBots::Budget budget{.iterations = settings_.botIterations};
  if (settings_.botSeconds > 0) {
    const auto until = std::chrono::steady_clock::now() +
                       std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                           std::chrono::duration<double>(settings_.botSeconds));
    budget.outOfTime = [until] { return std::chrono::steady_clock::now() >= until; };
  }
  const Action move = dealer         ? Game::dealerAction(now, dealer_)
                      : trading(now) ? tradingRuleMove(now)
                                     : ismcts_.choose(now, bots_, budget);
  const State before = now;
  if (!play_.play(move))
    return std::nullopt; // (a bug in the rules or a bot: the game stops there)
  record(before, move);
  return move;
}

std::vector<Action> TableGame::personMoves() const {
  std::vector<Action> moves = Game::legalActions(state());
  if (state().phase != Phase::Trading)
    return moves;
  const Resources &have = state().player[static_cast<std::size_t>(state().turn)].resources;
  for (int giving = 1; giving <= 2; ++giving)
    for (int give = 0; give < kResources; ++give) {
      if (have[static_cast<std::size_t>(give)] < giving)
        continue;
      for (int get = 0; get < kResources; ++get) {
        if (get == give)
          continue;
        Resources gives{}, gets{};
        gives[static_cast<std::size_t>(give)] = giving;
        gets[static_cast<std::size_t>(get)] = 1;
        moves.push_back({Action::Kind::OfferTrade, bundleOf(gives), bundleOf(gets), -1});
      }
    }
  return moves;
}

bool TableGame::play(const Action &move) {
  if (!waitingForPerson())
    return false;
  const std::vector<Action> allowed = personMoves();
  if (std::find(allowed.begin(), allowed.end(), move) == allowed.end())
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
