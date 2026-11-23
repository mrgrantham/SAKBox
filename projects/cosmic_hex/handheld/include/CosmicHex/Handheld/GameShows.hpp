// GameShows.hpp - what the stage shows of a game as it goes, apart from
// drawing it: each new state's shows started (the pieces' moves, the
// banners, a discovery, a home's pay, the cards flying, a card played, the
// cards dealt, your hand, the points' outline), each frame's shows moved
// on, the game held while they play, the dice thrown, B's skip, and which
// show is drawn over the board. The app (ScreenPadd) draws what it says
// and sends the table what it asks (Hooks); the scripted player's runs
// (ScriptedPlay) drive the same code on a clock of their own, with no
// screens, and read what it logged (ShowLog).
//
//   GameShows shows(stageWidth);
//   shows.setHooks({.hold = ..., .play = ..., .diceRolled = ...});
//   shows.startGame(playing, names, comingDown);
//   shows.setNow(now); shows.arrive(snapshot);    // each new state
//   shows.setNow(now); shows.frame();             // each frame
//   switch (shows.overBoard()) ...                // what to draw over the board
#pragma once

#include <CosmicHex/Handheld/BoardMotion.hpp>
#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/Pickers.hpp>
#include <CosmicHex/Handheld/PlayerNames.hpp>
#include <CosmicHex/Handheld/RollDice.hpp>
#include <CosmicHex/Handheld/ScorePops.hpp>
#include <CosmicHex/Handheld/SettingsScreen.hpp>
#include <CosmicHex/Handheld/ShowLog.hpp>
#include <CosmicHex/Handheld/ShowTimes.hpp>
#include <CosmicHex/Handheld/TableSnapshot.hpp>
#include <CosmicHex/Handheld/TopShows.hpp>
#include <CosmicHex/Handheld/TradeArrival.hpp>
#include <CosmicHex/Handheld/WinCelebration.hpp>
#include <ScratchPadd/Banner/Banner.hpp>
#include <ScratchPadd/GameClock.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

class GameShows {
public:
  using RealClock = ScratchPadd::GameClock::RealClock;

  // Why the game is held, if it is: one bit each. The table is told to hold
  // while any is set or you've paused, and to go on once none is
  enum class Hold : std::uint8_t {
    Start = 1,     // (the board coming down, who goes first)
    Menu = 2,      // (the pause menu)
    Info = 4,      // (Show resources, another player's panel)
    Discovery = 8, // (a tile shown big)
    HomePay = 16,  // (a home's pay)
    RollPay = 32,  // (a roll's pay)
    CardPlay = 64, // (an action card played)
    Stage = 128    // (the board's pieces still moving, a banner sweeping)
  };

  // What the app does for it: tell the table to hold or go on, make a
  // move, send the dice's faces; draw a discovered tile's face sharp before
  // it's shown big
  struct Hooks {
    std::function<void(bool held)> hold;
    std::function<void(const Action &move)> play;
    std::function<void(int first, int second)> diceRolled;
    std::function<void(Art::Face face)> closeUpTile;
  };

  // What's drawn over the board now (one at a time: on a DS words are on a
  // layer over every shape), else the HUD and the banner
  enum class OverBoard : std::uint8_t {
    FirstRoll,
    OtherPlayers,
    Discovery,
    Pick,
    Salvage,
    HomePay,
    CardPlay,
    Dealt,
    Hud
  };

  // `stageWidth`: the banner's screen, in pixels; `start`: the clocks' start
  explicit GameShows(float stageWidth, RealClock::time_point start = RealClock::now());

  void setHooks(Hooks hooks) { hooks_ = std::move(hooks); }
  // The touch panel's screen, followed (its picker, its views) and kept
  // following the game (BottomScreen::update)
  void setBottomScreen(BottomScreen *screen) { bottomScreen_ = screen; }
  // Each show's start, moves and end recorded (nothing: not recorded)
  void setLog(ShowLog *log) { log_ = log; }
  // The Animations setting: the clocks' pace, and Off skipping every show
  void setAnimations(Animations animations);
  // Whether the game's screen shows (not the pause menu, the title)
  void setInGame(bool inGame) { inGame_ = inGame; }

  // The time now (real time), before each frame and each new state
  void setNow(RealClock::time_point now) { now_ = now; }
  // The game's own time (moves animate and dice roll on it; it stops while
  // the game is held), and the shows' (real time at the Animations
  // setting's pace, jumping ahead when an animation's skipped; it runs
  // while the game's held)
  double gameNow() const { return gameClock_.seconds(now_); }
  double showsNow() const { return showClock_.seconds(now_); }

  // A game: playing (you're player 0) or watching, its names, and whether
  // the board is coming down first (holding the game till who goes first
  // is done)
  void startGame(bool playing, PlayerNames names, bool comingDown);
  void stopGame();
  // The board has come down: a moment to look at it, then who goes first
  void boardDown();
  // Who goes first, now (A while looking at the board)
  void startFirstRoll();
  // A: the next die of who goes first now; once all have landed, on
  void hurryFirstRoll();

  // A new state from the table: what moved animated, and the shows it
  // brings started, in this order (each show is timed after the ones
  // before it); the touch panel following it
  void arrive(const TableSnapshot &snapshot);
  // Each frame: the views you opened followed, the start's steps, the
  // dice, and every show moved on
  void frame();

  // Whether something is animating that B can take to its end, and B
  // itself (`pressed`: B; else the Animations setting, every frame)
  bool skippable() const;
  void skip(bool pressed);

  // You've paused a watched game (A), or let it go on
  void togglePaused();
  bool youPaused() const { return youPaused_; }
  void holdFor(Hold why, bool on);
  bool held(Hold why) const { return (holds_ & static_cast<std::uint8_t>(why)) != 0; }
  bool holding() const { return youPaused_ || holds_ != 0; }

  // Your move made (sent to the table: it isn't yours again until the
  // table says); what a touch or a button on the touch panel asked for
  // (the dice thrown, a move made): whether it made a move
  void play(const Action &move);
  bool use(const BottomScreen::Outcome &outcome);
  // Your roll waits for you to throw it (when you're playing); throw it
  bool yourRollWaits() const { return latest_ && yourRollWaits(*latest_); }
  void askRoll() { rollAsked_ = true; }

  // Someone's won: their ships fly round above the board, until it's over
  void celebrate(const State &state);
  bool celebrating() const { return celebration_.has_value(); }
  bool celebrationOver() const;
  void endCelebration() { celebration_.reset(); }

  // What's drawn over the board now
  OverBoard overBoard() const;
  // The shows the game waits on playing: the dice, the pieces moving, a
  // discovery, a home's pay (not while it waits for the game), cards
  // flying, a card played, the cards dealt, who goes first
  bool showsPlaying() const;
  // Anything the stage shows moving (the shows, banners, the views
  // opening, the points' outline, scores popping, your hand)
  bool moving() const;
  bool piecesMoving() const { return gameNow() - motionStarted_ < kSecondsBoardMoving; }
  // Whether the touch panel needs drawing again since the last asked
  bool takeTouchChanged();
  // How each show stands now (ShowLog)
  std::array<ShowSeen, kShowKinds> seen() const;

  // For drawing
  const std::optional<TableSnapshot> &latest() const { return latest_; }
  bool playing() const { return playing_; }
  const PlayerNames &names() const { return names_; }
  bool lookingAtBoard() const { return lookingSince_.has_value(); }
  const BoardMotion &motion() const { return motion_; }
  double motionStarted() const { return motionStarted_; }
  const RollDice &roll() const { return roll_; }
  const ScratchPadd::Banner &banner() const { return banner_; }
  // An offer to you (or a counter to yours) arriving: the trade screen
  // coming down the touch panel after its banner's words, on the banner's
  // clock (gameNow)
  const TradeArrival &tradeArrival() const { return tradeArrival_; }
  const DiscoveryShow &discovery() const { return discovery_; }
  const SalvageShow &salvage() const { return salvage_; }
  const DealtShow &dealt() const { return dealt_; }
  const PanelShow &others() const { return others_; }
  const CardsShow &cards() const { return cards_; }
  const FirstRollShow &firstRoll() const { return firstRoll_; }
  const HomePayShow &homePay() const { return homePay_; }
  const RollPayShow &rollPay() const { return rollPay_; }
  const ScorePops &scorePops() const { return scorePops_; }
  const CardPlayShow &cardPlay() const { return cardPlay_; }
  const PickShow &pick() const { return pick_; }
  const HandShow &hand() const { return hand_; }
  const std::optional<WinCelebration> &celebration() const { return celebration_; }
  BottomScreen::Info shownInfo() const { return shownInfo_; }
  // The points bars' outline, easing from the last turn's row to this
  // one's (nothing once it's there)
  std::optional<float> outlineRow() const;

private:
  bool startMotion(const TableSnapshot &snapshot);
  void startBanners(const TableSnapshot &snapshot);
  void startShows(const TableSnapshot &snapshot);
  void showGains(const TableSnapshot &snapshot);
  void followOnTouchPanel(const TableSnapshot &snapshot);
  void announceTradeAnswer();
  void startPlacingBanner(const State &state);
  void endFirstRoll();
  bool yourRollWaits(const TableSnapshot &snapshot) const;
  void promptForYourRoll();
  void throwDice();
  void followInfo();
  void updateStart();
  void updateRollPay();
  void updatePick();
  void updateHand();
  void updateCardPlay();
  void updateHomePay();
  void updateDiscovery();
  void updateSalvage();
  void sendHold();
  void sendYourMove();
  void updateHolds();
  bool stageTaken() const;
  void takeTurn(ShowKind kind);
  void showBanner(const std::string &title, const std::string &note,
                  ScratchPadd::Scene::Colour colour, bool bringsTrade = false);
  void log();

  Hooks hooks_;
  BottomScreen *bottomScreen_ = nullptr;
  ShowLog *log_ = nullptr;
  RealClock::time_point now_, lastFrame_;
  Animations animations_ = Animations::Full;
  bool inGame_ = true;

  // The game as last sent, and how it's shown moving, on the game's own
  // time (Gregory 8.4: it stops while the game is held)
  std::optional<TableSnapshot> latest_;
  bool playing_ = false;
  PlayerNames names_;
  BoardMotion motion_;
  ScratchPadd::GameClock gameClock_, showClock_;
  double motionStarted_ = -1e9;
  // The roll's physics dice, and the table's seed last thrown
  RollDice roll_;
  std::uint64_t thrownSeed_ = 0;
  bool rollAsked_ = false;            // (you've said to throw the dice: Roll or A)
  std::optional<Action> moveWaiting_; // (your move, sent once the pieces have landed)
  bool rollPrompted_ = false;         // (the banner says ROLL THE DICE, until you do)
  // The start: the board coming down, then looked at a moment
  bool comingDown_ = false;
  std::optional<RealClock::time_point> lookingSince_;

  // The turn banner, and the choice its banner was for
  ScratchPadd::Banner banner_;
  // A banner waiting for the stage (a show over the board has it), and
  // whether the trade screen comes down after it
  struct WaitingBanner {
    std::string title, note;
    ScratchPadd::Scene::Colour colour;
    bool bringsTrade = false;
  };
  std::optional<WaitingBanner> bannerWaiting_;
  std::optional<Pickers::ResourceChoice> bannerFor_;
  bool placingBannerAfterDeal_ = false; // (the start's placing: after the hands' deal)
  TradeArrival tradeArrival_;           // (an offer to you: its banner, then the trade screen)
  // The views you open (Show resources, another player's panel)
  BottomScreen::Info shownInfo_ = BottomScreen::Info::None;
  int othersShown_ = -1; // (the other player last shown: which way the carousel turns)
  CardsShow cards_;
  PanelShow others_{kSecondsPanel};
  std::optional<double> outlineMovedAt_; // (the turn's outline moving: since)
  int outlineFrom_ = 0;
  // What shows over the board for a while, each on the shows' clock
  DiscoveryShow discovery_;
  SalvageShow salvage_; // (salvage's cards over its find, then off)
  DealtShow dealt_;
  FirstRollShow firstRoll_;
  HomePayShow homePay_;
  RollPayShow rollPay_;
  ScorePops scorePops_;
  CardPlayShow cardPlay_;
  PickShow pick_;
  HandShow hand_;
  std::optional<WinCelebration> celebration_;

  std::uint8_t holds_ = 0;
  bool youPaused_ = false;
  bool touchChanged_ = true;
};

} // namespace CosmicHex::Handheld
