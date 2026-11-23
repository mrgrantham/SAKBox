#include <CosmicHex/Handheld/ScriptedGame.hpp>

#include <CosmicHex/Handheld/PlayerNames.hpp>
#include <CosmicHex/Handheld/ShowTimes.hpp>

#include <SeededRandom/Seed.hpp>

#include <chrono>
#include <utility>

namespace CosmicHex::Handheld {

namespace {

constexpr int kWidth = 256, kHeight = 192; // (a DS's screens)

TableSettings tableFor(const ScriptedGameSettings &settings) {
  TableSettings table{.botIterations = settings.botIterations, .seed = settings.seed};
  if (settings.person)
    table.seats[0] = Seat::Person;
  table.players = settings.players;
  table.board = boardChoices(settings.players).front().board; // (the setup's default)
  table.physicsDice = true; // (thrown on the stage, as the app's are)
  return table;
}

} // namespace

ScriptedGame::ScriptedGame(const ScriptedGameSettings &settings)
    : settings_(settings), table_(tableFor(settings)), shows_(static_cast<float>(kWidth), at(0)),
      screen_(kWidth, kHeight),
      map_(kWidth, kHeight, ScratchPadd::TextureRecipe::Smoothing::Coarse), player_(settings.seed),
      impatient_(SeededRandom::Seed::mixedFrom(settings.seed).stream("impatience")) {
  shows_.setBottomScreen(&screen_);
  shows_.setLog(&log_);
  shows_.setAnimations(settings.animations);
  // What the shows ask of the app, sent to the table as messages
  shows_.setHooks({.hold =
                       [this](bool held) {
                         post([this, held] {
                           tableHeld_ = held;
                           sendSnapshot(now_);
                         });
                       },
                   .play =
                       [this](const Action &move) {
                         post([this, move] {
                           if (!table_.play(move))
                             return;
                           table_.playDraws();
                           sendSnapshot(now_);
                         });
                       },
                   .diceRolled =
                       [this](int first, int second) {
                         post([this, first, second] {
                           if (!table_.rollDice(first, second))
                             return;
                           table_.playDraws();
                           sendSnapshot(now_);
                         });
                       },
                   .closeUpTile = {}});
  // The game starts held, the board come down (the app's descent isn't
  // drawn here): a moment to look at it, then who goes first
  shows_.startGame(
      settings.person,
      PlayerNames::forGame(settings.players, settings.person ? 0 : -1, "", settings.seed), true);
  shows_.boardDown();
  sendSnapshot(0);
  nextMove_ = kSecondsAMove;
}

ScriptedGame::Time ScriptedGame::at(double seconds) const {
  return Time{} +
         std::chrono::duration_cast<Time::duration>(std::chrono::duration<double>(seconds));
}

void ScriptedGame::post(std::function<void()> message) {
  messages_.push_back({now_ + settings_.messageSeconds, std::move(message)});
}

// The table as it is, on its way to the screens (TablePadd::sendSnapshot)
void ScriptedGame::sendSnapshot(double arrives) {
  snapshots_.push_back({arrives + settings_.messageSeconds,
                        snapshotOf(table_, playedSent_, tableHeld_, settings_.seed)});
  playedSent_ = table_.played().size();
  // (TablePadd: its own next move a move's time after any state it sends)
  nextMove_ = std::max(nextMove_, arrives + kSecondsAMove);
}

// The table's thread: messages answered between its moves; a move each
// kSecondsAMove once the last is done, unless held (TablePadd::repeat)
void ScriptedGame::runTable() {
  while (now_ >= tableBusyUntil_ && !messages_.empty() && messages_.front().at <= now_) {
    const std::function<void()> act = std::move(messages_.front().act);
    messages_.pop_front();
    act();
  }
  if (now_ < tableBusyUntil_ || now_ < nextMove_)
    return;
  nextMove_ = now_ + kSecondsAMove;
  if (tableHeld_ || table_.over() || table_.waitingForPerson())
    return;
  // (a bot's search takes its time; the dealer's draws none)
  const bool searched = table_.playOn() == TableGame::Made::PlayerMove;
  const double done = now_ + (searched ? settings_.thinkSeconds : 0);
  tableBusyUntil_ = done;
  nextMove_ = done + kSecondsAMove;
  sendSnapshot(done);
}

void ScriptedGame::apply(const ScriptedInput &input) {
  ++inputs_;
  if (input.kind == ScriptedInput::Kind::Tap) {
    // (as the app's tapBoard: the board's spot under it, when the board shows)
    const bool board = screen_.mode() == BottomScreen::Mode::Board ||
                       screen_.mode() == BottomScreen::Mode::Placing;
    const auto spot = board ? map_.at(input.x, input.y) : std::nullopt;
    shows_.use(screen_.tap(input.x, input.y, spot));
    return;
  }
  if (input.kind != ScriptedInput::Kind::Press)
    return;
  // (as the app's buttons: B skips what's animating; A throws your dice)
  if (input.key == BottomScreen::Key::Back && shows_.skippable()) {
    shows_.skip(true);
    return;
  }
  if (input.key == BottomScreen::Key::Confirm && shows_.yourRollWaits()) {
    shows_.askRoll();
    return;
  }
  if (screen_.looking() || (shows_.latest() && shows_.latest()->yourMove))
    shows_.use(screen_.press(input.key));
}

bool ScriptedGame::frame() {
  if (over_)
    return false;
  now_ += settings_.frameSeconds;
  shows_.setNow(at(now_));
  runTable();
  while (!snapshots_.empty() && snapshots_.front().arrives <= now_) {
    const TableSnapshot snapshot = std::move(snapshots_.front().snapshot);
    snapshots_.pop_front();
    shows_.arrive(snapshot);
    if (snapshot.state.winner >= 0 && !shows_.celebrating()) {
      if (settings_.animations == Animations::Off)
        over_ = true;
      else
        shows_.celebrate(snapshot.state);
    }
  }
  screen_.setTime(now_, shows_.showsNow());
  if (onFrame_)
    onFrame_(*this);
  // The player: B now and then while something animates (when impatient),
  // and a touch or a button every so often on their move
  if (settings_.person && settings_.impatience > 0 && now_ >= nextInput_ && shows_.skippable() &&
      impatient_.below(100) < static_cast<std::uint64_t>(settings_.impatience)) {
    apply({.kind = ScriptedInput::Kind::Press, .key = BottomScreen::Key::Back});
    nextInput_ = now_ + settings_.tapSeconds;
  }
  // (a person waits for what's playing over the board to finish, but for
  // the cards dealt, which wait for them to keep one)
  const bool waitForShows = shows_.showsPlaying() && !shows_.dealt().on();
  if (settings_.person && shows_.latest() && now_ >= nextInput_ && !waitForShows) {
    const ScriptedInput input =
        player_.next(*shows_.latest(), screen_, map_, shows_.yourRollWaits());
    if (input.kind != ScriptedInput::Kind::None) {
      apply(input);
      nextInput_ = now_ + settings_.tapSeconds;
    }
  }
  shows_.frame();
  if (shows_.celebrationOver()) {
    shows_.endCelebration();
    over_ = true;
  }
  if (now_ >= settings_.mostSeconds)
    over_ = true;
  if (over_)
    log_.finish(now_);
  return !over_;
}

void ScriptedGame::run() {
  while (frame()) {
  }
}

} // namespace CosmicHex::Handheld
