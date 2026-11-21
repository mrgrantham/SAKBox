#include "TablePadd.hpp"

#include "AppBasics.hpp"

#include <CosmicHex/Describe.hpp>
#include <CosmicHex/Handheld/PlatformProfile.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace CosmicHex::Handheld::App {

TablePadd::TablePadd(ScratchPadd::System *system) : Base(system) {
  receive<StartGame>([this](const StartGame &start) { this->start(start); });
  receive<TogglePause>([this](const TogglePause &) {
    paused_ = !paused_;
    if (game_)
      sendSnapshot(*game_);
  });
  receive<SetHold>([this](const SetHold &hold) {
    paused_ = hold.on;
    if (game_)
      sendSnapshot(*game_);
  });
  receive<StopGame>([this](const StopGame &) {
    game_.reset();
    paused_ = false;
  });
  receive<DiceRolled>([this](const DiceRolled &dice) {
    if (game_ && game_->rollDice(dice.first, dice.second))
      sendSnapshot(*game_);
  });
  receive<PlayMove>([this](const PlayMove &chosen) {
    if (game_ && game_->play(chosen.move))
      sendSnapshot(*game_);
  });
}

void TablePadd::start(const StartGame &start) {
  // Bots think up to 0.8 seconds a move (and at most PlatformProfile::botIterations)
  TableSettings settings{
      .botIterations = kThisPlatform.botIterations, .botSeconds = 0.8, .seed = startingSeed()};
  if (start.withPerson)
    settings.seats[0] = Seat::Person; // (you're player 1)
  settings.players = start.players;
  settings.board = start.board;
  settings.finishRound = start.finishRound;
  settings.seatBonus = start.seatBonus;
  settings.physicsDice = true; // (thrown on the stage)
  // Each simulated game in a frame arena (the search made
  // 59,000 allocations a second; one game holds up to about 75 KB)
  settings.frameArenaBytes = kThisPlatform.frameArenaBytes;
  settings.botArithmetic = kThisPlatform.botArithmetic;
  settings.measureSearch = kWatchAtStart; // (the -watch builds show it)
  seed_ = settings.seed;
  playedSent_ = 0;
  paused_ = start.held;
  sendSnapshot(game_.emplace(settings));
}

std::optional<std::chrono::duration<double>> TablePadd::repeatInterval() {
  return std::chrono::duration<double>(kSecondsAMove);
}

void TablePadd::repeat() {
  if (!game_ || paused_ || game_->over() || game_->waitingForPerson())
    return;
  TableGame &game = *game_;
  for (;;) {
    const bool dealer = CosmicHex::Game::currentPlayer(game.state()) == GameBots::kDealer;
    const auto started = Clock::now();
    if (!game.step())
      break; // (over, a person's move, or a bug: the game waits)
    if (!dealer) {
      lastThinkSeconds_ = secondsSince(started);
      break;
    }
  }
  sendSnapshot(game);
}

std::uint64_t TablePadd::startingSeed() {
  return static_cast<std::uint64_t>(Clock::now().time_since_epoch().count());
}

// Made in the message itself (sendMade): a snapshot holds the whole game,
// over a kilobyte, and its copies on this thread's stack overflowed it on a
// DS (the game froze)
void TablePadd::sendSnapshot(const TableGame &game) {
  const auto &history = game.history();
  const std::size_t shown = std::min<std::size_t>(history.size(), 8);
  const auto &played = game.played();
  const std::size_t from = std::min(playedSent_, played.size());
  playedSent_ = played.size();
  const bool dice = game.waitingForDice(), person = game.waitingForPerson();
  sendMade<TableSnapshot>([&] {
    std::vector<CosmicHex::Action> moves;
    std::vector<std::string> moveText;
    if (person) {
      moves = game.personMoves();
      for (const CosmicHex::Action &move : moves)
        moveText.push_back(CosmicHex::describe(game.state(), move));
    }
    return TableSnapshot{.state = game.state(),
                         .lastMoves = std::vector<std::string>(
                             history.end() - static_cast<long>(shown), history.end()),
                         .lastThinkSeconds = lastThinkSeconds_,
                         .paused = paused_,
                         .yourMove = person,
                         .moves = std::move(moves),
                         .moveText = std::move(moveText),
                         .waitingForDice = dice,
                         // (the game's seed and how far it's got: the same game throws the same)
                         .diceSeed = dice ? seed_ * 1000003u + history.size() : 0,
                         .firstRoll = game.firstRoll(),
                         // (the moves since the last snapshot)
                         .played = std::vector<TableGame::Played>(
                             played.begin() + static_cast<long>(from), played.end()),
                         .botGames = game.lastBotIterations(),
                         .botSearch = game.lastSearch()};
  });
}

} // namespace CosmicHex::Handheld::App
