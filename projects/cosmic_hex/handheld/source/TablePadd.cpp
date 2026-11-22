#include "TablePadd.hpp"

#include "AppBasics.hpp"

#include <CosmicHex/Handheld/PlatformProfile.hpp>

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
    if (!game_ || !game_->rollDice(dice.first, dice.second))
      return;
    game_->playDraws();
    sendSnapshot(*game_);
  });
  receive<PlayMove>([this](const PlayMove &chosen) {
    if (!game_ || !game_->play(chosen.move))
      return;
    game_->playDraws();
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
  // (a move's time after any state sent, the dice's or yours too: the
  // screen's hold for what it brought reaches the table first)
  if (secondsSince(sentAt_) < kSecondsAMove)
    return;
  game_->playOn();
  sendSnapshot(*game_);
}

std::uint64_t TablePadd::startingSeed() {
  return static_cast<std::uint64_t>(Clock::now().time_since_epoch().count());
}

// Made in the message itself (sendMade): a snapshot holds the whole game,
// over a kilobyte, and its copies on this thread's stack overflowed it on a
// DS (the game froze)
void TablePadd::sendSnapshot(const TableGame &game) {
  const std::size_t from = playedSent_;
  playedSent_ = game.played().size();
  sentAt_ = Clock::now();
  sendMade<TableSnapshot>([&] { return snapshotOf(game, from, paused_, seed_); });
}

} // namespace CosmicHex::Handheld::App
