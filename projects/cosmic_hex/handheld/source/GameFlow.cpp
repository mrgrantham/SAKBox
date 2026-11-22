// ScreenPadd's game flow: what each new state from the table sets going,
// starting and stopping games (ScreenFlow's steps), who goes first, the
// dice, and the win
#include "ScreenPadd.hpp"

#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/CardChoices.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace CosmicHex::Handheld::App {

// A new state from the table: what moved animated, and the shows it brings
// started, in this order (each show is timed after the ones before it)
void ScreenPadd::onSnapshot(const TableSnapshot &snapshot) {
  const bool moved = startMotion(snapshot);
  startBanners(snapshot);
  startShows(snapshot);
  // (pausing the game pauses its clock, so its animations and dice stop too)
  gameClock_.setPaused(snapshot.paused);
  if (moved)
    motionStarted_ = gameClock_.seconds();
  showGains(snapshot);
  latest_ = snapshot;
  ++snapshots_; // (the stage has something new to show)
  touchDirty_ = true;
  followOnTouchPanel(snapshot);
  announceTradeAnswer();
  // Someone won: their ships fly round above the board, then the
  // title again, with who (ScreenFlow's Won)
  if (snapshot.state.winner >= 0 && !celebration_ && flow_.screen() == ScreenFlow::Screen::Game)
    celebrate(snapshot.state);
}

// What moved since the last, animated from now (only when something moves:
// a snapshot that moves nothing, a hold or a choice, leaves what's moving
// going); true if anything did
bool ScreenPadd::startMotion(const TableSnapshot &snapshot) {
  if (!latest_)
    return false;
  bool moved = false;
  if (BoardMotion next(latest_->state, snapshot.state, &motion_); next.anything()) {
    motion_ = std::move(next);
    moved = true;
  }
  // Tiles turned over: each shown big on the stage, one after
  // another, once the ship's slide is done (DiscoveryShow)
  discovery_.arrived(latest_->state, snapshot.state, showsNow() + kSecondsAnimating);
  return moved;
}

namespace {
// Whether these moves deal the starting hands (Settings::startCards)
bool dealsStartingHands(const TableSnapshot &snapshot) {
  return std::ranges::any_of(snapshot.played, [](const TableGame::Played &move) {
    return move.action.kind == CosmicHex::Action::Kind::DealStartCard;
  });
}
} // namespace

// The start's placing (whose it is and what, once who goes first is
// shown, and the starting hands once they're dealt), or a new turn: its
// banner
void ScreenPadd::startBanners(const TableSnapshot &snapshot) {
  const CosmicHex::Phase phase = snapshot.state.phase;
  const bool placing =
      phase == CosmicHex::Phase::PlaceColony || phase == CosmicHex::Phase::PlaceShip;
  const bool newPlacer =
      !latest_ || snapshot.state.turn != latest_->state.turn || phase != latest_->state.phase;
  if (placing && newPlacer && dealsStartingHands(snapshot))
    placingBannerAfterDeal_ = true; // (updateRollPay starts it as the deal ends)
  else if (placing && newPlacer && !firstRoll_.on() && !descentStarted_ && !lookingSince_)
    startPlacingBanner(snapshot.state);
  if (latest_ && snapshot.state.turn != latest_->state.turn && !placing &&
      snapshot.state.winner < 0) {
    const int turn = snapshot.state.turn;
    const bool yours = mode_ == Mode::Playing && turn == 0;
    // (yours: the roll's prompt follows on the same band when the roll
    // waits for you: promptForYourRoll)
    banner_.start(gameClock_.seconds(), yours ? "YOUR TURN" : names_.upperOf(turn) + "'S TURN",
                  yours ? "" : "thinking...", BoardColours::ofPlayer(turn));
  }
}

// The shows a move brings: a home's pay, a roll's, a card played, your
// cards dealt, the points bars' outline moving
void ScreenPadd::startShows(const TableSnapshot &snapshot) {
  // A home's pay on its die: after the turn's banner (HomePayShow)
  homePay_.saw(snapshot.played, snapshot.state,
               showsNow() + banner_.remaining(gameClock_.seconds()));
  // A roll's pay: once the dice are pulled away (RollPayShow)
  if (latest_)
    rollPay_.saw(snapshot.played, latest_->state);
  // An action card played, or a Charter revealed: shown big (CardPlayShow)
  cardPlay_.saw(snapshot.played, showsNow());
  // Your two cards dealt after buying: thrown onto the stage
  const bool keeping = mode_ == Mode::Playing && snapshot.yourMove &&
                       snapshot.state.phase == CosmicHex::Phase::KeepContract;
  const auto choice = keeping ? keepChoiceOf(snapshot.state, snapshot.moves) : std::nullopt;
  if (choice)
    dealt_.deal(choice->cards, showsNow());
  else
    dealt_.stop();
  // A new turn: the points bars' outline moves to its row
  if (latest_ && snapshot.state.turn != latest_->state.turn) {
    outlineFrom_ = latest_->state.turn;
    outlineMovedAt_ = showsNow();
  }
}

// Your cards gained (your hand shown, the new ones dropping in: HandShow),
// and points and badges won (popping in: ScorePops)
void ScreenPadd::showGains(const TableSnapshot &snapshot) {
  if (latest_ && mode_ == Mode::Playing) {
    CosmicHex::Resources added{};
    bool any = false;
    for (std::size_t r = 0; r < added.size(); ++r) {
      added[r] = std::max(0, snapshot.state.player[0].resources[r] -
                                 latest_->state.player[0].resources[r]);
      any = any || added[r] > 0;
    }
    if (any)
      hand_.gained(added);
  }
  std::vector<int> points, badges;
  for (int p = 0; p < snapshot.state.players; ++p) {
    points.push_back(CosmicHex::points(snapshot.state, p));
    badges.push_back(snapshot.state.player[static_cast<std::size_t>(p)].badges);
  }
  scorePops_.saw(points, badges, showsNow());
}

// The touch panel's buttons and choosing screens follow the game
// (BottomScreen: the resource picker opens by itself, and so on)
void ScreenPadd::followOnTouchPanel(const TableSnapshot &snapshot) {
  if (bottomScreen_)
    bottomScreen_->update(snapshot.state, snapshot.moves,
                          mode_ == Mode::Playing && snapshot.yourMove, yourRollWaits(snapshot));
  // (a move the touch panel queued, once the rules allow it: a bank
  // trade after ending the turn's offers to players)
  if (bottomScreen_ && mode_ == Mode::Playing && snapshot.yourMove)
    if (const auto next = bottomScreen_->nextQueued()) {
      send(PlayMove{*next});
      latest_->yourMove = false;
    }
}

// An offer to you, or a counter to yours, just arrived (the user's: it
// used to just appear): a banner says who offers, in their colour, with the
// deal, and the trade screen comes down the touch panel once its words have
// settled (TradeArrival; presentTradeArriving)
void ScreenPadd::announceTradeAnswer() {
  if (!bottomScreen_)
    return;
  const TradeRoom &room = bottomScreen_->trade();
  if (tradeArrival_.saw(room.answering(), gameClock_.seconds()))
    banner_.start(gameClock_.seconds(), room.headline(), room.offerWords(),
                  BoardColours::ofPlayer(room.from()));
}

// What a step of the flow asks for: stop the game, start one (as the setup
// has it), hold or release it, or end the app
void ScreenPadd::apply(const ScreenFlow::Step &step) {
  if (step.changed)
    touchDirty_ = true;
  if (step.quit)
    quitting_ = true;
  if (step.stopGame) {
    celebration_.reset();
    send(StopGame{});
    latest_.reset();
    motion_ = BoardMotion();
    roll_.stop();
    selected_.reset();
    cursor_ = 0;
  }
  if (step.startGame)
    startGame();
  if (step.holdChanged)
    holdFor(Hold::Menu, step.holdGame);
  if (step.changed)
    menus_.screenChanged();
}

void ScreenPadd::startGame() {
  // (from the setup: the board comes down to the game's view, and the
  // touch panel fades from the menu to the map)
  if (flow_.screen() == ScreenFlow::Screen::Game) {
    descentStarted_ = Clock::now();
    lookingSince_.reset();
    fadeFromMenu_ = true;
    fading_ = false; // (the map drawn once, at the fade's first frame)
  }
  const StartMenu &setup = menus_.setup();
  const bool play = !setup.watching();
  mode_ = play ? Mode::Playing : Mode::Watching;
  latest_.reset();
  roll_.stop();
  touchDirty_ = true;
  youPaused_ = false;
  placingBannerAfterDeal_ = false;
  holds_ = descentStarted_ ? static_cast<std::uint8_t>(Hold::Start) : 0;
  names_ =
      PlayerNames::forGame(setup.players(), play ? 0 : -1, menus_.yourName(),
                           static_cast<std::uint64_t>(Clock::now().time_since_epoch().count()));
  if (bottomScreen_)
    bottomScreen_->setNames(names_);
  send(StartGame{.withPerson = play,
                 .players = setup.players(),
                 .board = setup.boardItself(),
                 .finishRound = setup.finishRound(),
                 .seatBonus = setup.seatBonus(),
                 .held = descentStarted_.has_value()});
}

// The start's banner: what's being placed and by whom
void ScreenPadd::startPlacingBanner(const State &state) {
  const bool ship = state.phase == CosmicHex::Phase::PlaceShip;
  const int turn = state.turn;
  if (mode_ == Mode::Playing && turn == 0)
    banner_.start(gameClock_.seconds(), ship ? "YOUR SHIP" : "YOUR HOME COLONY",
                  ship ? "touch a corner in open space, then End Turn"
                       : "touch a corner of a home, then End Turn",
                  BoardColours::ofPlayer(turn));
  else
    banner_.start(gameClock_.seconds(),
                  names_.upperOf(turn) + (ship ? " PLACES A SHIP" : " CHOOSES A HOME"),
                  "thinking...", BoardColours::ofPlayer(turn));
}

// After the board comes down: who goes first, fading in over it (the
// user's: a moment to look at the board first); each player's die, those
// tied rolling again, until A or it's done; then the game goes on
void ScreenPadd::startFirstRoll() {
  lookingSince_.reset();
  if (flow_.screen() == ScreenFlow::Screen::Game && latest_)
    firstRoll_.start(latest_->firstRoll,
                     static_cast<std::uint64_t>(Clock::now().time_since_epoch().count()),
                     kThisPlatform.diceStep, showsNow());
}

void ScreenPadd::endFirstRoll() {
  firstRoll_.stop();
  if (flow_.screen() == ScreenFlow::Screen::Game)
    holdFor(Hold::Start, false);
  // (with starting hands to deal, the placing's banner waits for the deal)
  if (latest_ && latest_->state.phase != CosmicHex::Phase::StartCardsDraw)
    startPlacingBanner(latest_->state);
}

// Whether the dice wait for you to throw them: your turn's roll, when
// you're playing (the user's: never thrown for you)
bool ScreenPadd::yourRollWaits(const TableSnapshot &snapshot) const {
  return mode_ == Mode::Playing && snapshot.waitingForDice && snapshot.state.turn == 0 &&
         snapshot.diceSeed != thrownSeed_;
}

// The dice: when the game waits for them, they're thrown into
// the glass container (DiceTray), stepped once a frame until they settle,
// shown for a moment, their faces sent to the table, and the container
// pulled off the stage
// Your roll waiting: the banner says ROLL THE DICE, on the turn's band if
// it's still up, and holds until you roll (the user's: with nothing on the
// screen the game looked stuck); then it goes the usual way
void ScreenPadd::promptForYourRoll() {
  const bool waits = latest_ && yourRollWaits(*latest_) && !rollAsked_;
  if (waits && !rollPrompted_ && !homePay_.moving(showsNow())) {
    banner_.then(gameClock_.seconds(), {"ROLL THE DICE", "touch Roll, or press A", true},
                 BoardColours::ofPlayer(0));
    rollPrompted_ = true;
  } else if (!waits && rollPrompted_) {
    banner_.release(gameClock_.seconds());
    rollPrompted_ = false;
  }
}

void ScreenPadd::throwDice() {
  promptForYourRoll();
  // Yours when you say (Roll, or A); a bot's once its turn's banner has
  // gone (the user's)
  const bool ready =
      latest_ && (yourRollWaits(*latest_) ? rollAsked_ : !banner_.showing(gameClock_.seconds())) &&
      !homePay_.moving(showsNow()); // (and the home's pay shown first)
  if (roll_.idle() && latest_ && latest_->waitingForDice && latest_->diceSeed != thrownSeed_ &&
      ready) {
    rollAsked_ = false;
    // (Roll's button goes once they're thrown)
    if (bottomScreen_) {
      bottomScreen_->update(latest_->state, latest_->moves,
                            mode_ == Mode::Playing && latest_->yourMove, false);
      touchDirty_ = true;
    }
    thrownSeed_ = latest_->diceSeed;
    roll_.throwFrom(thrownSeed_, gameClock_.seconds());
  }
  // (stepped on the game's clock; the faces, once shown, to the table)
  if (const auto faces = roll_.update(gameClock_.seconds()))
    send(DiceRolled{faces->first, faces->second});
}

// Someone's won: their ships (and, if fewer than three, more from their
// colonies) fly round above the board (WinCelebration), then the title;
// straight to the title when animations are off
void ScreenPadd::celebrate(const State &state) {
  if (menus_.animations() == Animations::Off) {
    apply(flow_.on(ScreenFlow::Event::GameWon));
    return;
  }
  std::vector<Scene::Vector3> ships, colonies;
  for (const CosmicHex::Ship &ship : state.ships)
    if (ship.owner == state.winner)
      ships.push_back(BoardPieces::at(ship.corner));
  for (const CosmicHex::Station &station : state.stations)
    if (station.owner == state.winner && station.colony)
      colonies.push_back(BoardPieces::at(station.at));
  celebration_.emplace(ships, colonies, showsNow());
}

// Each frame: once it's over (or skipped), the title
void ScreenPadd::updateCelebration() {
  if (celebration_ && celebration_->over(showsNow())) {
    celebration_.reset();
    apply(flow_.on(ScreenFlow::Event::GameWon));
  }
}

} // namespace CosmicHex::Handheld::App
