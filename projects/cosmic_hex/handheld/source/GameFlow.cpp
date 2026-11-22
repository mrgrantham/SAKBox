// ScreenPadd's game flow: each new state from the table handed to the
// shows (GameShows: what it sets going), what the shows ask of the app
// (their hooks), starting and stopping games (ScreenFlow's steps), and
// the win
#include "ScreenPadd.hpp"

#include <utility>
#include <vector>

namespace CosmicHex::Handheld::App {

// A new state from the table: its shows started (GameShows::arrive), the
// stage and the touch panel drawn again
void ScreenPadd::onSnapshot(const TableSnapshot &snapshot) {
  shows_.setNow(Clock::now());
  shows_.setInGame(flow_.screen() == ScreenFlow::Screen::Game);
  shows_.arrive(snapshot);
  ++snapshots_; // (the stage has something new to show)
  touchDirty_ = true;
  // Someone won: their ships fly round above the board, then the
  // title again, with who (ScreenFlow's Won)
  if (snapshot.state.winner >= 0 && !shows_.celebrating() &&
      flow_.screen() == ScreenFlow::Screen::Game)
    celebrate(snapshot.state);
}

// What the shows ask of the app: the table held or let go, a move made,
// the dice's faces sent; pictures drawn sharp before they're shown big
void ScreenPadd::hookShows() {
  shows_.setHooks({.hold = [this](bool held) { send(SetHold{held}); },
                   .play = [this](const CosmicHex::Action &move) { send(PlayMove{move}); },
                   .diceRolled = [this](int first, int second) { send(DiceRolled{first, second}); },
                   .closeUpTile = [this](CosmicHex::Art::Face face) { closeUpTile(face); }});
}

// What a step of the flow asks for: stop the game, start one (as the setup
// has it), hold or release it, or end the app
void ScreenPadd::apply(const ScreenFlow::Step &step) {
  if (step.changed)
    touchDirty_ = true;
  if (step.quit)
    quitting_ = true;
  if (step.stopGame) {
    send(StopGame{});
    shows_.stopGame();
    selected_.reset();
    cursor_ = 0;
  }
  if (step.startGame)
    startGame();
  shows_.setInGame(flow_.screen() == ScreenFlow::Screen::Game);
  if (step.holdChanged)
    shows_.holdFor(GameShows::Hold::Menu, step.holdGame);
  if (step.changed)
    menus_.screenChanged();
}

void ScreenPadd::startGame() {
  // (from the setup: the board comes down to the game's view, and the
  // touch panel fades from the menu to the map)
  if (flow_.screen() == ScreenFlow::Screen::Game) {
    descentStarted_ = Clock::now();
    fadeFromMenu_ = true;
    fading_ = false; // (the map drawn once, at the fade's first frame)
  }
  const StartMenu &setup = menus_.setup();
  const bool play = !setup.watching();
  touchDirty_ = true;
  PlayerNames names =
      PlayerNames::forGame(setup.players(), play ? 0 : -1, menus_.yourName(),
                           static_cast<std::uint64_t>(Clock::now().time_since_epoch().count()));
  if (bottomScreen_)
    bottomScreen_->setNames(names);
  shows_.setNow(Clock::now());
  shows_.startGame(play, std::move(names), descentStarted_.has_value());
  send(StartGame{.withPerson = play,
                 .players = setup.players(),
                 .board = setup.boardItself(),
                 .finishRound = setup.finishRound(),
                 .seatBonus = setup.seatBonus(),
                 .held = descentStarted_.has_value()});
}

// Someone's won: their ships (and, if fewer than three, more from their
// colonies) fly round above the board (WinCelebration), then the title;
// straight to the title when animations are off
void ScreenPadd::celebrate(const State &state) {
  if (menus_.animations() == Animations::Off) {
    apply(flow_.on(ScreenFlow::Event::GameWon));
    return;
  }
  shows_.celebrate(state);
}

// Each frame: once it's over (or skipped), the title
void ScreenPadd::updateCelebration() {
  if (shows_.celebrationOver()) {
    shows_.endCelebration();
    apply(flow_.on(ScreenFlow::Event::GameWon));
  }
}

} // namespace CosmicHex::Handheld::App
