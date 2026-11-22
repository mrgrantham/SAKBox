#include <CosmicHex/Handheld/GameShows.hpp>

#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/CardChoices.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/PlatformProfile.hpp>
#include <ScratchPadd/Motion/Motion.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace CosmicHex::Handheld {

GameShows::GameShows(float stageWidth, RealClock::time_point start)
    : now_(start), lastFrame_(start), gameClock_(start), showClock_(start),
      roll_(kThisPlatform.diceStep, kThisPlatform.mostDiceSteps), banner_(stageWidth) {}

void GameShows::setAnimations(Animations animations) {
  animations_ = animations;
  gameClock_.setRate(rateOf(animations), now_);
  showClock_.setRate(rateOf(animations), now_);
}

// ------------------------------------------------------------- A game

void GameShows::startGame(bool playing, PlayerNames names, bool comingDown) {
  stopGame();
  playing_ = playing;
  names_ = std::move(names);
  comingDown_ = comingDown;
  holds_ = comingDown ? static_cast<std::uint8_t>(Hold::Start) : 0;
}

// (every show and hold of the last game gone: a new game starts clean)
void GameShows::stopGame() {
  latest_.reset();
  motion_ = BoardMotion();
  motionStarted_ = -1e9;
  roll_.stop();
  rollAsked_ = rollPrompted_ = false;
  comingDown_ = false;
  lookingSince_.reset();
  banner_.stop();
  bannerWaiting_.reset();
  bannerFor_.reset();
  tradeArrival_ = TradeArrival();
  moveWaiting_.reset();
  placingBannerAfterDeal_ = false;
  outlineMovedAt_.reset();
  discovery_ = DiscoveryShow();
  dealt_ = DealtShow();
  firstRoll_.stop();
  homePay_ = HomePayShow();
  rollPay_ = RollPayShow();
  scorePops_ = ScorePops();
  cardPlay_ = CardPlayShow();
  pick_ = PickShow();
  hand_ = HandShow();
  celebration_.reset();
  holds_ = 0;
  youPaused_ = false;
}

void GameShows::boardDown() {
  comingDown_ = false;
  lookingSince_ = now_;
}

// After the board comes down: who goes first, fading in over it (the
// user's: a moment to look at the board first); each player's die, those
// tied rolling again, until A or it's done; then the game goes on
void GameShows::startFirstRoll() {
  lookingSince_.reset();
  if (inGame_ && latest_)
    firstRoll_.start(latest_->firstRoll,
                     static_cast<std::uint64_t>(now_.time_since_epoch().count()),
                     kThisPlatform.diceStep, showsNow());
}

void GameShows::hurryFirstRoll() {
  firstRoll_.hurry(showsNow());
  if (firstRoll_.done(showsNow()))
    endFirstRoll();
}

void GameShows::endFirstRoll() {
  firstRoll_.stop();
  if (inGame_)
    holdFor(Hold::Start, false);
  // (with starting hands to deal, the placing's banner waits for the deal)
  if (latest_ && latest_->state.phase != Phase::StartCardsDraw)
    startPlacingBanner(latest_->state);
}

// ---------------------------------------------------------- A new state

namespace {
// The moves just made, in words (what a show starting now came with)
std::string movesOf(const TableSnapshot &snapshot) {
  std::string words;
  const std::size_t n = std::min(snapshot.played.size(), snapshot.lastMoves.size());
  for (std::size_t i = snapshot.lastMoves.size() - n; i < snapshot.lastMoves.size(); ++i)
    words += (words.empty() ? "" : " / ") + snapshot.lastMoves[i];
  return words;
}
} // namespace

void GameShows::arrive(const TableSnapshot &snapshot) {
  if (log_)
    log_->setContext(movesOf(snapshot));
  const bool moved = startMotion(snapshot);
  startBanners(snapshot);
  startShows(snapshot);
  if (moved)
    motionStarted_ = gameNow();
  showGains(snapshot);
  latest_ = snapshot;
  touchChanged_ = true;
  followOnTouchPanel(snapshot);
  announceTradeAnswer();
  // (the table held at once for what this brings, before its next move)
  updateHolds();
}

// What moved since the last, animated from now (only when something moves:
// a snapshot that moves nothing, a hold or a choice, leaves what's moving
// going); true if anything did
bool GameShows::startMotion(const TableSnapshot &snapshot) {
  if (!latest_)
    return false;
  bool moved = false;
  if (BoardMotion next(latest_->state, snapshot.state, &motion_); next.anything()) {
    if (log_ && piecesMoving())
      log_->replaced(ShowKind::Pieces);
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
    return move.action.kind == Action::Kind::DealStartCard;
  });
}
} // namespace

// The start's placing (whose it is and what, once who goes first is
// shown, and the starting hands once they're dealt), or a new turn: its
// banner
void GameShows::startBanners(const TableSnapshot &snapshot) {
  const Phase phase = snapshot.state.phase;
  const bool placing = phase == Phase::PlaceColony || phase == Phase::PlaceShip;
  const bool newPlacer =
      !latest_ || snapshot.state.turn != latest_->state.turn || phase != latest_->state.phase;
  if (placing && newPlacer && dealsStartingHands(snapshot))
    placingBannerAfterDeal_ = true; // (updateRollPay starts it as the deal ends)
  else if (placing && newPlacer && !firstRoll_.on() && !comingDown_ && !lookingSince_)
    startPlacingBanner(snapshot.state);
  if (latest_ && snapshot.state.turn != latest_->state.turn && !placing &&
      snapshot.state.winner < 0) {
    const int turn = snapshot.state.turn;
    const bool yours = playing_ && turn == 0;
    // (yours: the roll's prompt follows on the same band when the roll
    // waits for you: promptForYourRoll)
    showBanner(yours ? "YOUR TURN" : names_.upperOf(turn) + "'S TURN", yours ? "" : "thinking...",
               BoardColours::ofPlayer(turn));
  }
}

// The shows a move brings: a home's pay, a roll's, a card played, your
// cards dealt, the points bars' outline moving
void GameShows::startShows(const TableSnapshot &snapshot) {
  // A home's pay on its die: after the turn's banner (HomePayShow)
  homePay_.saw(snapshot.played, snapshot.state, showsNow() + banner_.remaining(gameNow()));
  // A roll's pay: once the dice are pulled away (RollPayShow)
  if (latest_)
    rollPay_.saw(snapshot.played, latest_->state, showsNow());
  // An action card played, or a Charter revealed: shown big (CardPlayShow)
  cardPlay_.saw(snapshot.played, showsNow());
  // Your two cards dealt after buying: thrown onto the stage
  const bool keeping = playing_ && snapshot.yourMove && snapshot.state.phase == Phase::KeepContract;
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
void GameShows::showGains(const TableSnapshot &snapshot) {
  if (latest_ && playing_) {
    Resources added{};
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
void GameShows::followOnTouchPanel(const TableSnapshot &snapshot) {
  if (!bottomScreen_)
    return;
  bottomScreen_->update(snapshot.state, snapshot.moves, playing_ && snapshot.yourMove,
                        yourRollWaits(snapshot));
  // (a move the touch panel queued, once the rules allow it: a bank
  // trade after ending the turn's offers to players)
  if (playing_ && snapshot.yourMove)
    if (const auto next = bottomScreen_->nextQueued())
      play(*next);
}

// An offer to you, or a counter to yours, just arrived (the user's: it
// used to just appear): a banner says who offers, in their colour, with the
// deal, waiting its turn as every banner does (showBanner), and the trade
// screen comes down the touch panel once its words have settled
// (TradeArrival)
void GameShows::announceTradeAnswer() {
  if (!bottomScreen_)
    return;
  const TradeRoom &room = bottomScreen_->trade();
  if (tradeArrival_.saw(room.answering(), gameNow()))
    showBanner(room.headline(), room.offerWords(), BoardColours::ofPlayer(room.from()), true);
}

// The start's banner: what's being placed and by whom
void GameShows::startPlacingBanner(const State &state) {
  const bool ship = state.phase == Phase::PlaceShip;
  const int turn = state.turn;
  if (playing_ && turn == 0)
    showBanner(ship ? "YOUR SHIP" : "YOUR HOME COLONY",
               ship ? "touch a corner in open space, then End Turn"
                    : "touch a corner of a home, then End Turn",
               BoardColours::ofPlayer(turn));
  else
    showBanner(names_.upperOf(turn) + (ship ? " PLACES A SHIP" : " CHOOSES A HOME"), "thinking...",
               BoardColours::ofPlayer(turn));
}

// --------------------------------------------------------------- The dice

// Whether the dice wait for you to throw them: your turn's roll, when
// you're playing (the user's: never thrown for you)
bool GameShows::yourRollWaits(const TableSnapshot &snapshot) const {
  return playing_ && snapshot.waitingForDice && snapshot.state.turn == 0 &&
         snapshot.diceSeed != thrownSeed_;
}

// Your roll waiting: the banner says ROLL THE DICE, on the turn's band if
// it's still up, and holds until you roll (the user's: with nothing on the
// screen the game looked stuck); then it goes the usual way
void GameShows::promptForYourRoll() {
  const bool waits = latest_ && yourRollWaits(*latest_) && !rollAsked_;
  if (waits && !rollPrompted_ && !homePay_.current() && !bannerWaiting_) {
    banner_.then(gameNow(), {"ROLL THE DICE", "touch Roll, or press A", true},
                 BoardColours::ofPlayer(0));
    rollPrompted_ = true;
  } else if (!waits && rollPrompted_) {
    banner_.release(gameNow());
    rollPrompted_ = false;
  }
}

// The dice: when the game waits for them, they're thrown into the glass
// container (DiceTray), stepped once a frame until they settle, shown for
// a moment, their faces sent to the table, and the container pulled off
// the stage
void GameShows::throwDice() {
  promptForYourRoll();
  // Yours when you say (Roll, or A); a bot's once its turn's banner has
  // gone (the user's)
  const bool ready = latest_ &&
                     (yourRollWaits(*latest_) ? rollAsked_ : !banner_.showing(gameNow())) &&
                     !homePay_.current() && !discovery_.waiting() && !cardPlay_.current() &&
                     !stageTaken(); // (and the shows before them first, even those still to come)
  if (roll_.idle() && latest_ && latest_->waitingForDice && latest_->diceSeed != thrownSeed_ &&
      ready) {
    rollAsked_ = false;
    // (Roll's button goes once they're thrown)
    if (bottomScreen_) {
      bottomScreen_->update(latest_->state, latest_->moves, playing_ && latest_->yourMove, false);
      touchChanged_ = true;
    }
    thrownSeed_ = latest_->diceSeed;
    roll_.throwFrom(thrownSeed_, gameNow());
  }
  // (stepped on the game's clock; the faces, once shown, to the table)
  if (const auto faces = roll_.update(gameNow()))
    if (hooks_.diceRolled)
      hooks_.diceRolled(faces->first, faces->second);
}

// ------------------------------------------------------------- Each frame

void GameShows::frame() {
  followInfo();
  updateStart();
  sendYourMove();
  throwDice();
  // (one show over the board at a time, in the order they came: each
  // waits while another has the stage)
  takeTurn(ShowKind::Discovery);
  updateDiscovery();
  takeTurn(ShowKind::HomePay);
  updateHomePay();
  updateRollPay();
  takeTurn(ShowKind::CardPlay);
  updateCardPlay();
  updatePick();
  updateHand();
  if (bannerWaiting_ && !stageTaken()) {
    banner_.start(gameNow(), {{bannerWaiting_->title, bannerWaiting_->note}},
                  bannerWaiting_->colour);
    if (bannerWaiting_->bringsTrade)
      tradeArrival_.after(gameNow());
    bannerWaiting_.reset();
  }
  updateHolds();
  if (outlineMovedAt_ && showsNow() - *outlineMovedAt_ >= kSecondsOutline)
    outlineMovedAt_.reset();
  log();
  lastFrame_ = now_;
}

// An info view opened or closed (Show resources): the game held while
// it's open, and let go when it closes, if the view was what held it; and
// another player chosen while their panel's open
void GameShows::followInfo() {
  if (!bottomScreen_)
    return;
  if (bottomScreen_->info() != shownInfo_) {
    // (Show resources: its cards in as it opens, back off as it closes)
    if (bottomScreen_->info() == BottomScreen::Info::Resources)
      cards_.open(showsNow());
    else if (shownInfo_ == BottomScreen::Info::Resources)
      cards_.close(showsNow());
    // (another player's panel: down as it opens, up as it closes)
    if (bottomScreen_->info() == BottomScreen::Info::Player) {
      others_.open(bottomScreen_->infoPlayer(), showsNow());
      othersShown_ = bottomScreen_->infoPlayer();
    } else if (shownInfo_ == BottomScreen::Info::Player) {
      others_.close(showsNow());
    }
    shownInfo_ = bottomScreen_->info();
    if (inGame_)
      holdFor(Hold::Info, shownInfo_ != BottomScreen::Info::None);
  }
  // (another player chosen while the panel's open: their view; the next
  // player round the table comes in from the right, the one before from
  // the left)
  if (shownInfo_ == BottomScreen::Info::Player && latest_) {
    const int which = bottomScreen_->infoPlayer(), players = latest_->state.players;
    const int ahead = othersShown_ < 0 ? 1 : (which - othersShown_ + players) % players;
    others_.choose(which, showsNow(), ahead <= players / 2 ? 1 : -1);
    othersShown_ = which;
  }
}

// The start of a game: animations skipped when they're off, the board
// looked at, then who goes first
void GameShows::updateStart() {
  if (animations_ == Animations::Off && skippable())
    skip(false);
  if (lookingSince_ &&
      std::chrono::duration<double>(now_ - *lookingSince_).count() >= kSecondsLookingAtBoard)
    startFirstRoll();
  firstRoll_.update(showsNow());
  if (firstRoll_.done(showsNow()))
    endFirstRoll();
}

// A roll's pay started once the dice have gone
void GameShows::updateRollPay() {
  // (once the dice have gone from the stage: not over the roll)
  if (rollPay_.waiting() && roll_.idle())
    rollPay_.start(showsNow());
  const RollPayShow::Step step = rollPay_.update(showsNow());
  // (the starting hands dealt: now whose placing it is)
  if (step.letGo && placingBannerAfterDeal_ && latest_) {
    placingBannerAfterDeal_ = false;
    startPlacingBanner(latest_->state);
  }
}

// While you choose a resource on the picker, the banner as each choice
// opens (two in a row: one each), then, once it's gone, the card in focus
// shown big on the stage (PickShow; the user's: not behind it)
void GameShows::updatePick() {
  // (a home's star: once the die has landed on it, not while it's still
  // to come out or spinning, as the picker's banner then swept by under it)
  const bool dieLanded = !homePay_.current() || homePay_.waitingForChoice(showsNow());
  const std::optional<Pickers::ResourceChoice> open =
      bottomScreen_ ? bottomScreen_->resourceChoice() : std::nullopt;
  const bool choosing = playing_ && latest_ && latest_->yourMove && open &&
                        bottomScreen_->mode() == BottomScreen::Mode::Resources && dieLanded;
  if (!choosing) {
    // (the choice made: its banner goes, its question answered, rather than
    // sweeping on under the card it brought)
    if (bannerFor_ && banner_.moving(gameNow()))
      banner_.hurry(gameNow());
    bannerFor_.reset();
    pick_.show(std::nullopt, showsNow());
    return;
  }
  const Pickers::ResourceChoice &choice = *open;
  if (!bannerFor_ || !bannerFor_->sameAs(choice)) {
    bannerFor_ = choice;
    pick_.show(std::nullopt, showsNow()); // (the last choice's card gone)
    showBanner("CHOOSE A RESOURCE", std::string(choice.title), Palette::kHighlight);
  }
  pick_.show(banner_.moving(gameNow()) ? std::nullopt : bottomScreen_->resourceInFocus(),
             showsNow());
}

// Your cards gained shown once nothing that brings them is still showing
// (the roll's pay, the home's, choosing one, a discovery)
void GameShows::updateHand() {
  const double t = showsNow();
  const bool busy =
      showsPlaying() || rollPay_.waiting() || pick_.at(t) || bannerFor_ || cards_.showing(t);
  if (hand_.waiting() && !hand_.showing(t) && !busy)
    hand_.start(t);
}

void GameShows::updateCardPlay() { cardPlay_.update(showsNow()); }

void GameShows::updateHomePay() { homePay_.update(showsNow()); }

// A discovery's close-up drawn before it starts, the map drawn again as
// its tile fades in
void GameShows::updateDiscovery() {
  const DiscoveryShow::Step step = discovery_.update(showsNow(), hooks_.closeUpTile);
  if (step.redrawMap)
    touchChanged_ = true;
}

// ------------------------------------------------------------ Holding

void GameShows::holdFor(Hold why, bool on) {
  const auto bit = static_cast<std::uint8_t>(why);
  const std::uint8_t before = holds_;
  holds_ = on ? static_cast<std::uint8_t>(holds_ | bit) : static_cast<std::uint8_t>(holds_ & ~bit);
  gameClock_.setPaused(youPaused_ || held(Hold::Menu) || held(Hold::Info), now_);
  if ((before == 0) != (holds_ == 0))
    sendHold();
}

void GameShows::togglePaused() {
  youPaused_ = !youPaused_;
  sendHold();
}

// The table held from the moment a move brings something to show until it's
// shown: a discovery, a home's pay (not while it waits for the game: the
// star's choice, the next roll), the cards flying (from when they're
// queued, as the dice go), a card played; and while the board's pieces
// move or a banner sweeps (or waits for the stage). Without it the table
// made its next move meanwhile, and that move's banner, pieces or show cut
// short or covered the last's
void GameShows::updateHolds() {
  const double t = showsNow();
  holdFor(Hold::Discovery, discovery_.waiting());
  holdFor(Hold::HomePay, homePay_.current() && !homePay_.waitingForGame(t));
  holdFor(Hold::RollPay, rollPay_.waiting() || rollPay_.moving(t));
  holdFor(Hold::CardPlay, cardPlay_.current() != nullptr);
  holdFor(Hold::Stage,
          (latest_ && piecesMoving()) || banner_.moving(gameNow()) || bannerWaiting_.has_value());
}

// Whether a show over the board has the stage now (a discovery, a home's
// pay that isn't waiting for the game, a card played)
bool GameShows::stageTaken() const {
  const double t = showsNow();
  return discovery_.moving(t) || (homePay_.moving(t) && !homePay_.waitingForGame(t)) ||
         cardPlay_.moving(t);
}

// A show still to start waits while another has the stage or a banner
// sweeps (they took turns before only by luck, each timed as it came: a
// home's die spun unseen under a discovery and then appeared half done)
void GameShows::takeTurn(ShowKind kind) {
  const double t = showsNow();
  const bool discovery = discovery_.moving(t);
  const bool home = homePay_.moving(t) && !homePay_.waitingForGame(t);
  const bool card = cardPlay_.moving(t);
  // (the dice too: thrown, they're seen till they've gone)
  const bool banner = banner_.moving(gameNow()) || !roll_.idle();
  const double soon = t + 1e-6; // (not this frame)
  switch (kind) {
  case ShowKind::Discovery:
    if (home || card || banner)
      discovery_.waitFor(soon);
    break;
  case ShowKind::HomePay:
    if (discovery || card || banner)
      homePay_.waitFor(soon);
    break;
  case ShowKind::CardPlay:
    if (discovery || home || banner)
      cardPlay_.waitFor(soon);
    break;
  default:
    break;
  }
}

// A banner: after the one sweeping, on the same band (never cutting it
// short); or, while a show has the stage, once it's done (drawn under it,
// it swept by unseen). One that `bringsTrade` has the trade screen come
// down after its words, whenever they come in (TradeArrival)
void GameShows::showBanner(const std::string &title, const std::string &note,
                           ScratchPadd::Scene::Colour colour, bool bringsTrade) {
  if (banner_.moving(gameNow())) {
    banner_.then(gameNow(), {title, note}, colour);
    // (the trade screen after this slide's words; after a slide that waits,
    // they've no time yet: in place at once)
    if (const auto wordsIn = banner_.lastEntersAt(); bringsTrade && wordsIn)
      tradeArrival_.after(*wordsIn - ScratchPadd::Banner::kWordsInAfter);
    else if (bringsTrade)
      tradeArrival_.skip();
  } else if (stageTaken()) {
    // (a waiting offer's banner replaced: its screen in place, not waiting
    // for words that won't come)
    if (bannerWaiting_ && bannerWaiting_->bringsTrade)
      tradeArrival_.skip();
    bannerWaiting_ = WaitingBanner{title, note, colour, bringsTrade};
    if (bringsTrade)
      tradeArrival_.waitForBanner();
  } else {
    banner_.start(gameNow(), title, note, colour); // (TradeArrival timed from now: saw)
  }
}

// The table told; and the game's own clock paused only when you've paused
// (A, the pause menu, a view you opened), not while a show holds the table:
// paused by every hold, a banner froze half across and sat under the show
// that held it, and the pieces stopped mid-flight
void GameShows::sendHold() {
  gameClock_.setPaused(youPaused_ || held(Hold::Menu) || held(Hold::Info), now_);
  if (hooks_.hold)
    hooks_.hold(holding());
}

// ------------------------------------------------------------- Your moves

// (sent once the board's pieces have landed: made while the last move's
// pieces still flew, it cut their flight and landing short)
void GameShows::play(const Action &move) {
  moveWaiting_ = move;
  sendYourMove();
  if (latest_)
    latest_->yourMove = false; // (until the game says it's yours again)
}

void GameShows::sendYourMove() {
  if (!moveWaiting_ || (latest_ && piecesMoving()))
    return;
  if (hooks_.play)
    hooks_.play(*moveWaiting_);
  moveWaiting_.reset();
}

bool GameShows::use(const BottomScreen::Outcome &outcome) {
  if (outcome.roll && yourRollWaits())
    askRoll();
  if (!outcome.play || !latest_ || !latest_->yourMove)
    return false;
  play(*outcome.play);
  return true;
}

// ------------------------------------------------------------ Skipping

// Whether something is animating that B (Back) can skip to its end: the
// dice, who goes first, the board's pieces moving, the turn's banner, the
// trade screen coming in, a tile turned over, a home's or a roll's pay, the
// cards dealt coming in
bool GameShows::skippable() const {
  if (!inGame_ || !latest_)
    return false;
  if (!roll_.idle() || firstRoll_.on() || lookingSince_ || celebration_)
    return true;
  if (piecesMoving())
    return true;
  if (banner_.moving(gameNow()) || // (not a prompt waiting for you)
      tradeArrival_.moving(gameNow()))
    return true;
  const double t = showsNow();
  const bool dealing =
      dealt_.on() && !dealt_.cards().empty() && dealt_.turned(dealt_.cards().size() - 1, t) < 1;
  return dealing || discovery_.moving(t) || homePay_.moving(t) || rollPay_.moving(t) ||
         cardPlay_.moving(t);
}

// Every animation straight to its end frame (the user's: B, or every
// frame when Animations is Off)
void GameShows::skip(bool pressed) {
  if (log_)
    log_->skipped(pressed);
  // The shows: their clock on past any of their ends. Those still to start
  // (a home's pay after the banner, a discovery after the ship's flight)
  // start afresh after it: B ends what's animating, not what's to come
  // (jumped past too, a home's pay skipped by B on the banner before it
  // showed its die at its end, or not at all)
  const double before = showsNow();
  const bool discoveryToCome = discovery_.waiting() && !discovery_.moving(before);
  const bool homeToCome = homePay_.current() && !homePay_.moving(before);
  const bool cardToCome = cardPlay_.current() && !cardPlay_.moving(before);
  static constexpr double kPastAnyShow = 30;
  showClock_.skipAhead(kPastAnyShow);
  if (discoveryToCome)
    discovery_.waitFor(showsNow());
  if (homeToCome)
    homePay_.waitFor(showsNow());
  if (cardToCome)
    cardPlay_.waitFor(showsNow());
  // Who goes first: each die set down and the order read
  if (lookingSince_)
    startFirstRoll();
  for (int i = 0; i < 16 && firstRoll_.on() && !firstRoll_.done(showsNow()); ++i)
    firstRoll_.hurry(showsNow());
  if (firstRoll_.done(showsNow()))
    endFirstRoll();
  // The board's pieces where they were going, the turn's banner gone
  motionStarted_ = std::min(motionStarted_, gameNow() - 2 * kSecondsBoardMoving);
  banner_.hurry(gameNow()); // (a prompt waiting for you stays)
  tradeArrival_.skip();     // (the trade screen in place)
  roll_.skip(gameNow());
}

// --------------------------------------------------------- The win

void GameShows::celebrate(const State &state) {
  std::vector<ScratchPadd::Scene::Vector3> ships, colonies;
  for (const Ship &ship : state.ships)
    if (ship.owner == state.winner)
      ships.push_back(BoardPieces::at(ship.corner));
  for (const Station &station : state.stations)
    if (station.owner == state.winner && station.colony)
      colonies.push_back(BoardPieces::at(station.at));
  celebration_.emplace(ships, colonies, showsNow());
}

bool GameShows::celebrationOver() const { return celebration_ && celebration_->over(showsNow()); }

// ------------------------------------------------------- What shows

GameShows::OverBoard GameShows::overBoard() const {
  const double t = showsNow();
  if (firstRoll_.on() && latest_)
    return OverBoard::FirstRoll;
  if (others_.showing(t) && latest_ && others_.which() >= 0)
    return OverBoard::OtherPlayers;
  if (discovery_.moving(t))
    return OverBoard::Discovery;
  if (pick_.at(t) && latest_ && bottomScreen_ && bottomScreen_->resourceChoice())
    return OverBoard::Pick;
  if (homePay_.moving(t) && latest_)
    return OverBoard::HomePay;
  if (cardPlay_.moving(t) && latest_)
    return OverBoard::CardPlay;
  if (dealt_.on())
    return OverBoard::Dealt;
  return OverBoard::Hud;
}

bool GameShows::showsPlaying() const {
  const double t = showsNow();
  const bool homePay = homePay_.moving(t) && !homePay_.waitingForGame(t);
  return !roll_.idle() || piecesMoving() || discovery_.moving(t) || homePay || rollPay_.moving(t) ||
         cardPlay_.moving(t) || dealt_.on() || firstRoll_.on();
}

bool GameShows::moving() const {
  const double t = showsNow();
  return showsPlaying() || banner_.moving(gameNow()) || cards_.moving(t) || others_.moving(t) ||
         outlineMovedAt_ || scorePops_.popping(t) || celebration_ || pick_.moving(t) ||
         hand_.moving(t);
}

std::optional<float> GameShows::outlineRow() const {
  if (!outlineMovedAt_ || !latest_)
    return std::nullopt;
  const float moved = ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::EaseInOut,
                                                 static_cast<float>(showsNow() - *outlineMovedAt_) /
                                                     kSecondsOutline);
  if (moved >= 1)
    return std::nullopt;
  const auto from = static_cast<float>(outlineFrom_);
  return from + (static_cast<float>(latest_->state.turn) - from) * moved;
}

bool GameShows::takeTouchChanged() { return std::exchange(touchChanged_, false); }

// --------------------------------------------------------- The log

std::array<ShowSeen, kShowKinds> GameShows::seen() const {
  const double t = showsNow(), g = gameNow();
  const double gameRate = gameClock_.paused() ? 0 : gameClock_.rate();
  const double showsRate = showClock_.rate();
  const OverBoard top = overBoard();
  const bool covered = others_.inPlace(t); // (the board itself under a panel)
  std::array<ShowSeen, kShowKinds> seen{};
  const auto at = [&](ShowKind kind) -> ShowSeen & {
    ShowSeen &one = seen[static_cast<std::size_t>(kind)];
    one.kind = kind;
    return one;
  };
  {
    ShowSeen &banner = at(ShowKind::Banner);
    banner.on = banner_.showing(g);
    banner.moving = banner.on;
    banner.visible = top == OverBoard::Hud || top == OverBoard::Pick ||
                     (top == OverBoard::HomePay && homePay_.waitingForGame(t));
    banner.key = banner_.startedAt().value_or(0);
    banner.elapsed = g - banner.key;
    banner.rate = gameRate;
  }
  {
    ShowSeen &dice = at(ShowKind::Dice);
    dice.on = dice.moving = !roll_.idle();
    // (under a show drawn over the board, a discovery's dimming, it's hidden)
    dice.visible = !others_.showing(t) && (top == OverBoard::Hud || top == OverBoard::Pick);
    dice.key = roll_.thrownAt();
    dice.elapsed = g - dice.key;
    dice.rate = gameRate;
  }
  {
    ShowSeen &pieces = at(ShowKind::Pieces);
    pieces.on = pieces.moving = latest_ && piecesMoving();
    pieces.visible = !covered;
    pieces.key = motionStarted_;
    pieces.elapsed = g - motionStarted_;
    pieces.rate = gameRate;
  }
  {
    ShowSeen &discovery = at(ShowKind::Discovery);
    discovery.on = discovery_.waiting();
    discovery.moving = discovery_.moving(t);
    discovery.visible = top == OverBoard::Discovery;
    if (const Discovery::Found *found = discovery_.current())
      discovery.key = (found->hex.q + 50) * 100 + (found->hex.r + 50);
    discovery.elapsed = t - discovery_.startsAt().value_or(t);
    discovery.rate = showsRate;
  }
  {
    ShowSeen &home = at(ShowKind::HomePay);
    home.on = homePay_.current() != nullptr;
    home.moving = homePay_.moving(t) && !homePay_.waitingForGame(t);
    home.visible = top == OverBoard::HomePay;
    home.key = homePay_.outAt();
    home.elapsed = t - homePay_.outAt();
    home.rate = showsRate;
  }
  {
    ShowSeen &flights = at(ShowKind::CardFlights);
    flights.moving = rollPay_.moving(t);
    flights.on = rollPay_.waiting() || flights.moving;
    flights.visible = flights.moving;
    flights.key = rollPay_.waiting() ? -1 : rollPay_.startAt();
    flights.elapsed = t - rollPay_.startAt();
    flights.rate = showsRate;
  }
  {
    ShowSeen &play = at(ShowKind::CardPlay);
    play.on = cardPlay_.current() != nullptr;
    play.moving = cardPlay_.moving(t);
    play.visible = top == OverBoard::CardPlay;
    play.key = cardPlay_.startedAt().value_or(-1);
    play.elapsed = t - cardPlay_.startedAt().value_or(t);
    play.rate = showsRate;
  }
  {
    ShowSeen &dealt = at(ShowKind::Dealt);
    dealt.on = dealt_.on();
    dealt.moving = dealt_.on();
    dealt.visible = top == OverBoard::Dealt;
    dealt.elapsed = dealt_.seconds(t);
    dealt.key = dealt_.thrownAt();
    dealt.rate = showsRate;
  }
  {
    ShowSeen &first = at(ShowKind::FirstRoll);
    first.on = first.moving = firstRoll_.on();
    first.visible = top == OverBoard::FirstRoll;
    first.key = firstRoll_.startedAt();
    first.elapsed = t - first.key;
    first.rate = showsRate;
  }
  {
    ShowSeen &hand = at(ShowKind::Hand);
    hand.moving = hand_.showing(t);
    hand.on = hand_.queued() || hand.moving;
    hand.visible = hand.moving && !cards_.showing(t);
    hand.key = hand_.startedAt().value_or(-1);
    hand.elapsed = t - hand_.startedAt().value_or(t);
    hand.rate = showsRate;
  }
  {
    ShowSeen &party = at(ShowKind::Celebration);
    party.on = party.moving = party.visible = celebration_.has_value();
    party.key = 1;
    party.rate = showsRate;
  }
  return seen;
}

void GameShows::log() {
  if (!log_)
    return;
  const auto seconds = [](RealClock::duration d) {
    return std::chrono::duration<double>(d).count();
  };
  const double now = seconds(now_.time_since_epoch());
  const double step = seconds(now_ - lastFrame_);
  ShowKind onTop = ShowKind::Banner;
  switch (overBoard()) {
  case OverBoard::FirstRoll:
    onTop = ShowKind::FirstRoll;
    break;
  case OverBoard::Discovery:
    onTop = ShowKind::Discovery;
    break;
  case OverBoard::HomePay:
    onTop = ShowKind::HomePay;
    break;
  case OverBoard::CardPlay:
    onTop = ShowKind::CardPlay;
    break;
  case OverBoard::Dealt:
    onTop = ShowKind::Dealt;
    break;
  case OverBoard::OtherPlayers:
  case OverBoard::Pick:
  case OverBoard::Hud:
    break;
  }
  const auto all = seen();
  log_->frame(now, step, all, onTop);
}

} // namespace CosmicHex::Handheld
