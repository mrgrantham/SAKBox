// Cosmic Hex on a Nintendo 3DS or DS, in padds (one app, built for each:
// ../3ds, ../ds; the few differences in #if blocks): the agreed rules (the rules
// library's agreedRules, on R3), the bots as the simulator plays them,
// fewer iterations a move.
//
//   ScreenPadd (main thread)  draws the board on the top screen with the
//              scene layer (3DS backend, in stereo while the 3D slider is
//              up): the tiles' faces (art/), stations, colonies, ships
//              and the blockade in the players' colours, toon shaded, with
//              the round, points, resources and the last move over it; and
//              the board drawn flat on the bottom screen (BoardPicture), to
//              touch and zoom
//   TablePadd  (own thread)  plays the game (TableGame): the dealer's draws
//              and the bots' moves, one at a time, paced so they can be
//              watched, and sends ScreenPadd each new state
//
// At the start: A to play against three bots (you're player 1), B to watch
// four. On your move, the moves you can make are listed on the top screen:
// touch the board to list those at a spot, up and down to choose, A to make
// it, B to list them all again. Watching: A pauses or goes on.
// X held with R/L: zoom the bottom board in and out   L/R: left and right
// on a choosing screen   touch: a corner, a ship, a button (drag: move the
// board when zoomed)   START: pause
#if defined(__NDS__)
#include <ScratchPadd/Scene/DS/Display.hpp>
#include <nds.h>
#else
#include <3ds.h>
#include <ScratchPadd/Scene/3DS/Display.hpp>
#endif

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Describe.hpp>
#include <CosmicHex/Dice/DiceTray.hpp>
#include <CosmicHex/Handheld/BoardMotion.hpp>
#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/BoardView.hpp>
#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/ButtonMenu.hpp>
#include <CosmicHex/Handheld/CardChoices.hpp>
#include <CosmicHex/Handheld/CardsHeld.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/Discovery.hpp>
#include <CosmicHex/Handheld/MenuPicture.hpp>
#include <CosmicHex/Handheld/MoveSpots.hpp>
#include <CosmicHex/Handheld/OtherPlayers.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/PlatformProfile.hpp>
#include <CosmicHex/Handheld/PlayerNames.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>
#include <CosmicHex/Handheld/ScreenFlow.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>
#include <CosmicHex/Handheld/StartMenu.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>
#include <ScratchPadd/FrameProfile.hpp>
#include <ScratchPadd/GameClock.hpp>
#include <ScratchPadd/MemoryStats.hpp>
#include <ScratchPadd/Motion/Motion.hpp>
#include <ScratchPadd/TitleScene/TitleScene.hpp>
// The title screen's rocket, as the build cooked it for this console
#if defined(__NDS__)
#include "rocket_ds.h"
#else
#include "rocket_3ds.h"
#endif

#include <ScratchPadd/AboutPadd.hpp>
#include <ScratchPadd/AppEnded/AppEnded.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/Input/GamePadd.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <spdlog/spdlog.h>

#include <ScratchPadd/Banner/Banner.hpp>
#include <ScratchPadd/ConsoleScreens/ConsoleScreens.hpp>
#include <ScratchPadd/OwnerName/OwnerName.hpp>
#include <ScratchPadd/Scene/TextWrap.hpp>
#include <ScratchPadd/StartupSplash/StartupSplash.hpp>

#if defined(COSMIC_HEX_BENCH)
#include "CosmicHexBench.hpp"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace Scene = ScratchPadd::Scene;

// What differs between the two consoles: the screens' sizes, how much text
// fits a line, how hard the bots think (the DS's ARM9 has no floating-point
// hardware), and the main loop
#if defined(COSMIC_HEX_WATCH_AT_START)
constexpr bool kWatchAtStart = true;
#else
constexpr bool kWatchAtStart = false;
#endif

// The scripted-taps builds (COSMIC_HEX_SCRIPTED_TAPS): taps the
// app makes itself at set seconds after starting, as if the stylus had, so
// a screen reached by touch can be seen in an emulator without touching it
// (and photographed for screenshot tests). Each: when, where on the bottom
// screen (DS pixels, scaled to the 3DS's), held for a moment
struct ScriptedTap {
  double seconds;
  int x, y;
};
#ifdef COSMIC_HEX_SCRIPTED_TAPS
// Show resources opened, then closed again
constexpr std::array<ScriptedTap, 2> kScriptedTaps{{{12, 10, 10}, {18, 10, 10}}};
#else
constexpr std::array<ScriptedTap, 0> kScriptedTaps{};
#endif

// The title screen: the 3D scene, or the plain text title
// (false). Which one a player sees, and how it's chosen, is an open
// question on the roadmap page; the scene for now
// How long the title takes to fade in from black, once the start's work
// is done
// The points bars' outline moving to the next player's row
constexpr float kSecondsOutline = 0.35f;
// The two cards dealt after buying: each thrown in, then turned over
constexpr float kSecondsCardThrow = 0.45f, kSecondsCardTurn = 0.4f;
// How long the other players' view takes to slide down (or up)
constexpr float kSecondsPanel = 0.3f;
// How long who goes first is shown at the start (A goes on sooner)
// Show resources' cards: each eases in (and out) over kSecondsCard, each
// a little after the last
constexpr float kSecondsCard = 0.25f, kSecondsCardStagger = 0.05f;
// Who goes first: each round of dice thrown, tumbling then settling; then
// time to read the order
constexpr float kSecondsARollRound = 1.4f, kSecondsDiceTumble = 0.9f;
constexpr double kSecondsFirstRollRead = 2.6;
constexpr double kSecondsTitleFadeIn = 0.6;

constexpr bool kTitleScene = true;

// What this console does differently (PlatformProfile.hpp), under the names
// the app uses
using CosmicHex::Handheld::kThisPlatform;
constexpr int kTopWidth = kThisPlatform.topWidth, kTopHeight = kThisPlatform.topHeight;
constexpr int kBottomWidth = kThisPlatform.bottomWidth, kBottomHeight = kThisPlatform.bottomHeight;
constexpr std::size_t kCharsPerLine = kThisPlatform.charsPerLine;
constexpr float kLineHeight = kThisPlatform.lineHeight;
constexpr int kFaceSize = kThisPlatform.faceSize;
namespace Input = ScratchPadd::Input;
namespace ConsoleScreens = ScratchPadd::ConsoleScreens;
using CosmicHex::State;
namespace Dice = CosmicHex::Dice;
using namespace CosmicHex::Handheld;
using Clock = std::chrono::steady_clock;
// The seconds since `start`
double secondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

// How long each move stays on screen before the next, so a game can be
// watched (the dealer's draws go straight on), and how long its slide or
// flip takes
constexpr double kSecondsAMove = 0.6;
constexpr double kSecondsAnimating = 0.35;
// A planet's number chip, once it's turned over, drops this far onto it
// and settles in this long
constexpr double kSecondsChipDrop = 0.35;
constexpr float kChipDropHeight = 1.2f;

// The game as it stands (TablePadd to ScreenPadd), after each move; and on
// a person's move, the moves they can make, each in words
struct TableSnapshot {
  State state;
  std::vector<std::string> lastMoves; // the newest last
  double lastThinkSeconds = 0;        // the last bot move's search
  bool paused = false;
  bool yourMove = false;
  std::vector<CosmicHex::Action> moves;
  std::vector<std::string> moveText;
  // The game waits for the dice: the screen throws them with
  // this seed (a new one each roll) and sends back what they show
  bool waitingForDice = false;
  std::uint64_t diceSeed = 0;
  CosmicHex::Handheld::FirstRoll firstRoll; // (the dice that chose who goes first)
  int botGames = 0;                         // (the last bot move's simulated games, for measuring)
  GameBots::SearchTimes botSearch;          // (and where its time went)
};

// ScreenPadd to TablePadd: start a game (you against three bots, or four
// bots to watch, on a board from boardChoices() and a play style); pause or
// go on (watching); the move you chose
struct StartGame {
  bool withPerson = false;
  int players = 4;
  CosmicHex::Board board = CosmicHex::boardR3(); // (a StartMenu choice)
  bool finishRound = true; // (the round played out, or the first to the target wins)
  std::array<int, CosmicHex::kMaxPlayers> seatBonus{}; // (the board's, if chosen)
  // Held until the screen lets it go (SetHold off): the board coming down
  // from the setup first, so no move is made before it's in view
  bool held = false;
};
struct TogglePause {};
// The pause menu holds the game (true) or lets it go on (false); leaving a
// game stops it
struct SetHold {
  bool on;
};
struct StopGame {};
struct DiceRolled {
  int first, second; // (the faces the dice settled on)
};
struct PlayMove {
  CosmicHex::Action move;
};

class TablePadd : public ScratchPadd::Base {
public:
  TablePadd(ScratchPadd::System *system) : Base(system) {
    receive<StartGame>([this](const StartGame &start) {
      // Bots think up to 0.8 seconds a move (and at most PlatformProfile::botIterations)
      TableSettings settings{
          .botIterations = kThisPlatform.botIterations, .botSeconds = 0.8, .seed = startingSeed()};
      if (start.withPerson)
        settings.seats[0] = Seat::Person; // (you're player 1)
      settings.players = start.players;
      settings.board = start.board;
      settings.finishRound = start.finishRound;
      settings.seatBonus = start.seatBonus;
      settings.physicsDice = true; // (thrown on the top screen)
      // Each simulated game in a frame arena (the search made
      // 59,000 allocations a second; one game holds up to about 75 KB)
      settings.frameArenaBytes = kThisPlatform.frameArenaBytes;
      settings.botArithmetic = kThisPlatform.botArithmetic;
      settings.measureSearch = kWatchAtStart; // (the -watch builds show it)
      seed_ = settings.seed;
      paused_ = start.held;
      game_.emplace(settings);
      sendSnapshot();
    });
    receive<TogglePause>([this](const TogglePause &) {
      paused_ = !paused_;
      if (game_)
        sendSnapshot();
    });
    receive<SetHold>([this](const SetHold &hold) {
      paused_ = hold.on;
      if (game_)
        sendSnapshot();
    });
    receive<StopGame>([this](const StopGame &) {
      game_.reset();
      paused_ = false;
    });
    receive<DiceRolled>([this](const DiceRolled &dice) {
      if (game_ && game_->rollDice(dice.first, dice.second))
        sendSnapshot();
    });
    receive<PlayMove>([this](const PlayMove &chosen) {
      if (game_ && game_->play(chosen.move))
        sendSnapshot();
    });
  }
  std::string name() override { return "TablePadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::duration<double>(kSecondsAMove);
  }
  void repeat() override {
    if (!game_ || paused_ || game_->over() || game_->waitingForPerson())
      return;
    // The dealer's draws straight through; then one player's move
    for (;;) {
      const bool dealer = CosmicHex::Game::currentPlayer(game_->state()) == GameBots::kDealer;
      const auto started = Clock::now();
      if (!game_->step())
        break; // (over, a person's move, or a bug: the game waits)
      if (!dealer) {
        lastThinkSeconds_ = secondsSince(started);
        break;
      }
    }
    sendSnapshot();
  }

private:
  // A new game each time: the seed from the clock
  static std::uint64_t startingSeed() {
    return static_cast<std::uint64_t>(Clock::now().time_since_epoch().count());
  }
  void sendSnapshot() {
    const auto &history = game_->history();
    const std::size_t shown = std::min<std::size_t>(history.size(), 8);
    TableSnapshot snapshot{.state = game_->state(),
                           .lastMoves = std::vector<std::string>(
                               history.end() - static_cast<long>(shown), history.end()),
                           .lastThinkSeconds = lastThinkSeconds_,
                           .paused = paused_,
                           .firstRoll = game_->firstRoll(),
                           .botGames = game_->lastBotIterations(),
                           .botSearch = game_->lastSearch()};
    if (game_->waitingForDice()) {
      snapshot.waitingForDice = true;
      // (the game's seed and how far it's got: the same game throws the same)
      snapshot.diceSeed = seed_ * 1000003u + history.size();
    }
    if (game_->waitingForPerson()) {
      snapshot.yourMove = true;
      snapshot.moves = game_->personMoves();
      for (const CosmicHex::Action &move : snapshot.moves)
        snapshot.moveText.push_back(CosmicHex::describe(game_->state(), move));
    }
    send(std::move(snapshot));
  }

  std::optional<TableGame> game_;
  std::uint64_t seed_ = 0;
  bool paused_ = false;
  double lastThinkSeconds_ = 0;
};

class ScreenPadd : public ScratchPadd::Base {
public:
  ScreenPadd(ScratchPadd::System *system) : Base(system) {
    receive<TableSnapshot>([this](const TableSnapshot &snapshot) {
      // What moved since the last, animated from now
      if (latest_) {
        motion_ = BoardMotion(latest_->state, snapshot.state);
        shownBig_.clear();
        // Tiles turned over: each shown big on the top screen, one after
        // another, once the ship's slide is done (Discovery)
        for (Discovery::Found &found : Discovery::between(latest_->state, snapshot.state))
          discoveries_.push_back(std::move(found));
        if (!discoveries_.empty() && !discoveryAt_)
          discoveryAt_ =
              Clock::now() + std::chrono::milliseconds(static_cast<int>(kSecondsAnimating * 1000));
      }
      // A new turn (after the setup's placing): its banner
      const CosmicHex::Phase phase = snapshot.state.phase;
      const bool placing =
          phase == CosmicHex::Phase::PlaceColony || phase == CosmicHex::Phase::PlaceShip;
      // (the start's placing: whose it is and what, once who goes first is
      // shown)
      if (placing && !firstRollShownAt_ && !descentStarted_ &&
          (!latest_ || snapshot.state.turn != latest_->state.turn || phase != latest_->state.phase))
        startPlacingBanner(snapshot.state);
      if (latest_ && snapshot.state.turn != latest_->state.turn && !placing &&
          snapshot.state.winner < 0) {
        const int turn = snapshot.state.turn;
        const bool yours = mode_ == Mode::Playing && turn == 0;
        banner_.start(gameClock_.seconds(), yours ? "YOUR TURN" : names_.upperOf(turn) + "'S TURN",
                      yours ? "roll the dice" : "thinking...", BoardColours::ofPlayer(turn));
      }
      // Your two cards dealt after buying: thrown onto the top screen
      {
        const bool keeping = mode_ == Mode::Playing && snapshot.yourMove &&
                             snapshot.state.phase == CosmicHex::Phase::KeepContract;
        if (keeping && !keepDealtAt_)
          keepDealtAt_ = Clock::now();
        else if (!keeping)
          keepDealtAt_.reset();
      }
      // A new turn: the points bars' outline moves to its row
      if (latest_ && snapshot.state.turn != latest_->state.turn) {
        outlineFrom_ = latest_->state.turn;
        outlineMovedAt_ = Clock::now();
      }
      // (pausing the game pauses its clock, so its animations and dice stop too)
      gameClock_.setPaused(snapshot.paused);
      motionStarted_ = gameClock_.seconds();
      latest_ = snapshot;
      ++snapshots_; // (the top screen has something new to show)
      bottomDirty_ = true;
      // The bottom screen's buttons and choosing screens follow the game
      // (BottomScreen: the resource picker opens by itself, and so on)
      if (bottomScreen_)
        bottomScreen_->update(snapshot.state, snapshot.moves,
                              mode_ == Mode::Playing && snapshot.yourMove, yourRollWaits(snapshot));
      // (a move the bottom screen queued, once the rules allow it: a bank
      // trade after ending the turn's offers to players)
      if (bottomScreen_ && mode_ == Mode::Playing && snapshot.yourMove)
        if (const auto next = bottomScreen_->nextQueued()) {
          send(PlayMove{*next});
          latest_->yourMove = false;
        }
      // Someone won: the title again, with who (ScreenFlow's Won)
      if (snapshot.state.winner >= 0)
        apply(flow_.on(ScreenFlow::Event::GameWon));
    });
  }
  std::string name() override { return "ScreenPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }

  void starting() override {
    const Clock::time_point startedAt = Clock::now();
    display_.emplace(); // the top screen
                        // The bottom screen: the flat board (BoardPicture), written straight
                        // into its one framebuffer when it changes
    bottom_.emplace();
    picture_.emplace(kBottomWidth, kBottomHeight, kThisPlatform.recipeSmoothing);
    picture_->setZoomBudget(kThisPlatform.boardZoomBytes);
    bottomScreen_.emplace(kBottomWidth, kBottomHeight);
#if !defined(NDEBUG)
    // (debug builds: the list of moves, behind a button, off at first)
    bottomScreen_->setMoveListButton(true);
#endif
    bottomCanvas_.emplace(kBottomWidth, kBottomHeight);
    renderer_.emplace(*display_);
    const BoardPieces pieces;
    hexTile_ = renderer_->load(pieces.hexTile.data());
    numberChip_ = renderer_->load(pieces.numberChip.data());
    station_ = renderer_->load(pieces.station.data());
    colony_ = renderer_->load(pieces.colony.data());
    ship_ = renderer_->load(pieces.ship.data());
    stars_ = renderer_->load(pieces.stars.data());
    hexRing_ = renderer_->load(pieces.hexRing.data());
    cornerRing_ = renderer_->load(pieces.cornerRing.data());
    die_ = renderer_->load(DicePieces(static_cast<float>(Dice::DiceTray::kDieSize)).die.data());
    // The title scene: the 3D title, the rocket, the night sky
    // (fewer stars on the DS: each is two of its 2048 polygons)
    title_.emplace(*renderer_, Assets::kRocket,
                   ScratchPadd::TitleScene::Settings{.title = "COSMIC HEX",
                                                     .aspect = static_cast<float>(kTopWidth) /
                                                               static_cast<float>(kTopHeight),
                                                     .stars = kThisPlatform.titleStars});
    // Everything drawn from recipes for the game ahead (the map's tile
    // faces and number chips, the bottom screen's cards and icons, the top
    // screen's faces, chips and cards), now, while the screens are dark,
    // then the title fades in (the user's: drawn a picture a
    // frame during the title, up to a tenth of a second each on a DS, they
    // made the rocket stutter for its first turn). The map's faces first:
    // the top screen's are made from them where they're no smaller.
    // (behind a splash, fading in and out while it runs)
    ScratchPadd::StartupSplash splash(*renderer_, {.width = kTopWidth,
                                                   .height = kTopHeight,
                                                   .title = "Vibe Coding by James",
                                                   .subtitle = "presents",
                                                   .words = Palette::kWords,
                                                   .softWords = Palette::kDimWords,
                                                   .markColour = Palette::kAccent});
    splash.run([this] {
      while (!picture_->prepare()) {
      }
      while (!bottomScreen_->prepare()) {
      }
      loadFaces();
      loadCardPictures();
    });
    titleStarted_ = Clock::now();
    // (how long the start took, shown on the -watch builds)
    startSeconds_ = secondsSince(startedAt);
    // The build that watches by itself (COSMIC_HEX_WATCH_AT_START: the
    // -watch apps), for unattended runs in an emulator or on a console
    if (kWatchAtStart) {
      for (int i = 0; i < 4; ++i) // (down to the Mode row: watch)
        menu_.press(StartMenu::Key::Down);
      menu_.press(StartMenu::Key::Right);
      apply(flow_.on(ScreenFlow::Event::Play));
      apply(flow_.on(ScreenFlow::Event::Start));
    }
    push([this] { frame(); });
  }

private:
  void frame() {
    if (!ConsoleScreens::keepRunning()) {
      system_->stop();
      return;
    }
    const Input::Gamepad pad = currentState<Input::Gamepad>().value;
    const Input::Edges pressed(lastPad_, pad);
    lastPad_ = pad;
    // START: the pause menu in a game (and out of it again)
    if (pressed.pressed(Input::Button::Start)) {
      if (flow_.screen() == ScreenFlow::Screen::Game)
        apply(flow_.on(ScreenFlow::Event::Pause));
      else if (flow_.screen() == ScreenFlow::Screen::Paused)
        apply(flow_.on(ScreenFlow::Event::Resume));
    }
    if (quitting_) {
      system_->stop();
      return;
    }
    // Holding X, R and L zoom the bottom board in and out (the
    // user's); without X, L and R are left and right on a choosing screen
    const bool zooming = pad.isHeld(Input::Button::FaceTop);
    if (zooming && pressed.pressed(Input::Button::R)) {
      picture_->setZoom(picture_->zoom() + 1);
      bottomDirty_ = true;
    }
    if (zooming && pressed.pressed(Input::Button::L)) {
      picture_->setZoom(picture_->zoom() - 1);
      bottomDirty_ = true;
    }
    handleButtons(pressed, zooming);
    handleTouch(scripted(pad.touch));
    // An info view opened or closed (Show resources): the game held while
    // it's open, and let go when it closes, if the view was what held it
    if (bottomScreen_ && bottomScreen_->info() != shownInfo_) {
      // (Show resources closing: its cards go back off as they came in)
      if (shownInfo_ != BottomScreen::Info::None &&
          bottomScreen_->info() == BottomScreen::Info::None) {
        infoClosingAt_ = Clock::now();
        closingInfo_ = shownInfo_;
      } else {
        infoClosingAt_.reset();
      }
      shownInfo_ = bottomScreen_->info();
      infoOpenedAt_ = Clock::now();
      if (flow_.screen() == ScreenFlow::Screen::Game && latest_) {
        if (shownInfo_ != BottomScreen::Info::None && !latest_->paused) {
          send(SetHold{true});
          heldForInfo_ = true;
        } else if (shownInfo_ == BottomScreen::Info::None && heldForInfo_) {
          send(SetHold{false});
          heldForInfo_ = false;
        }
      }
    }
    // A pressed button's glow (drawn again while it fades), and a tapped
    // button's message, for a second and a half
    if (bottomScreen_) {
      bottomScreen_->setTime(secondsSince(startedAt_));
      if (bottomScreen_->pressing() || pressDrawn_)
        bottomDirty_ = true;
      pressDrawn_ = bottomScreen_->pressing(); // (and once more, to clear it)
    }
    if (bottomScreen_ && bottomScreen_->showingFeedback()) {
      if (!feedbackSince_) {
        feedbackSince_ = Clock::now();
      } else if (Clock::now() - *feedbackSince_ > std::chrono::milliseconds(1500)) {
        bottomScreen_->clearFeedback();
        feedbackSince_.reset();
        bottomDirty_ = true;
      }
    }
    profile_.frameStarted();
    if (firstRollShownAt_ && latest_ &&
        secondsSince(*firstRollShownAt_) > firstRollSeconds(latest_->firstRoll))
      endFirstRoll();
    if (bottomDirty_) {
      auto timing = profile_.time(kProfileBottom);
      drawBottom();
    }
    {
      auto timing = profile_.time(kProfileDice);
      throwDice();
    }
    measureMemory();
    updateDiscovery();
    // The two cards dealt, drawn sharp before they're thrown in (on a DS
    // about a quarter of a second, while the screen is still)
    if (keepDealtAt_ && latest_) {
      if (const auto choice = keepChoiceOf(latest_->state, latest_->moves);
          choice && choice->cards != closeUpCardsShow_) {
        closeUpCards(choice->cards);
        keepDealtAt_ = Clock::now();
      }
    }
    // (the map's faces at the whole board's size, and the bottom screen's
    // pictures, are drawn at the start: starting())
    if (flow_.screen() == ScreenFlow::Screen::Game && !picture_->ready()) {
      // Zoomed in or out: the map shows the whole board's faces scaled at
      // once, and sharpens a face a frame
      auto timing = profile_.time(kProfileBottom);
      picture_->prepare();
      bottomDirty_ = true;
    }
    // (the top screen's drawing, its wait for the refresh included; ended
    // with the frame: drawTop's scope)
    auto drawing = std::optional<ScratchPadd::FrameProfile::Timing>();
    drawing.emplace(profile_, kProfileTop);
    // The top screen: the title scene (the title, and once a game's won);
    // the board chosen, turning slowly (setup); or the game's board
    const ScreenFlow::Screen screen = flow_.screen();
    if (kTitleScene && (screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Won)) {
      drawTitle();
      drawing.reset();
      profile_.frameEnded();
      push([this] { frame(); });
      return;
    }
    // (L and R don't turn the 3D board: the user's choice)
    if (screen == ScreenFlow::Screen::Setup) {
      drawSetup();
    } else if (kThisPlatform.holdStillFrames && topIsStill()) {
      // Nothing on the top screen has changed: send nothing and let the
      // DS's 3D hardware show the last frame again, which it does by itself
      // from what it was last sent (GBATEK: the rendering engine draws its
      // polygon memory every frame; only a new flush changes it). The time
      // goes to the bots' thread while this waits (on a DS,
      // drawing an unchanged board each frame left them 4% of the CPU)
      auto timing = profile_.time(kProfileWait);
      ConsoleScreens::waitForTheRefresh();
    } else {
      drawBoard(latest_ ? &latest_->state : nullptr);
      movedWhenDrawn_ = topMoving();
      drawnSnapshots_ = snapshots_;
      drawnTurn_ = turn_;
      drawnSelection_ = selected_;
      drawnAt_ = Clock::now();
    }
    drawing.reset();
    profile_.frameEnded();
    push([this] { frame(); });
  }

  // The title scene, turning; its words over it: before a game nothing (the
  // menu's below), after one who won
  void drawTitle() {
    renderer_->beginFrame(title_->camera(), title_->environment());
    title_->draw(*renderer_, static_cast<float>(secondsSince(titleStarted_)));
    // Fading in from black, once the start's work is done
    if (const float in = titleFadedIn(); in < 1)
      renderer_->overlay().rectangle(
          0, 0, kTopWidth, kTopHeight,
          {0, 0, 0, static_cast<std::uint8_t>(std::lround((1 - in) * 255))});
    if (flow_.screen() == ScreenFlow::Screen::Won && latest_) {
      char line[64];
      std::snprintf(line, sizeof line, "Player %d wins!", latest_->state.winner + 1);
      Scene::Overlay &overlay = renderer_->overlay();
      overlay.text(kTopWidth / 2.0f - 70, kTopHeight - 40, line, Palette::kHighlight, 2);
      overlay.text(kTopWidth / 2.0f - 60, kTopHeight - 16, "A: to the title", Palette::kSoftWords);
    }
    showProfile();
    {
      auto timing = profile_.time(kProfileWait);
      renderer_->endFrame();
    }
  }

  // Memory (measuring before optimising, Gregory 10.9): with the
  // counting allocator in the build, every 2 seconds the heap and the
  // allocations a second, logged (a debug build's log) and, in the -watch
  // builds, shown on the top screen
  void measureMemory() {
    if (!ScratchPadd::MemoryStats::linked())
      return;
    if (!memory_) {
      memory_.emplace();
      memoryAt_ = Clock::now();
      return;
    }
    const double seconds = secondsSince(memoryAt_);
    if (seconds < 2)
      return;
    memoryAt_ = Clock::now();
    const ScratchPadd::MemoryStats::Snapshot counts = memory_->take();
    const ScratchPadd::MemoryStats::Heap heap = ScratchPadd::MemoryStats::heap();
    char line[96];
    std::snprintf(line, sizeof line, "%zuK used %zuK free/%zu %.0f/s", heap.inUse / 1024,
                  heap.free / 1024, heap.freeChunks,
                  static_cast<double>(counts.all.allocations) / seconds);
    memoryLine_ = line;
    std::string busiest;
    for (std::size_t i = 0; i < counts.padds.size() && i < 3; ++i) {
      char padd[48];
      std::snprintf(padd, sizeof padd, " %s %.0f/s", counts.padds[i].name.c_str(),
                    static_cast<double>(counts.padds[i].counts.allocations) / seconds);
      busiest += padd;
    }
    spdlog::info("memory: {};{}", memoryLine_, busiest);
    memoryBusiest_ = busiest;
  }

  // The dice: when the game waits for them, they're thrown into
  // the glass container (DiceTray), stepped once a frame until they settle,
  // shown for a moment, their faces sent to the table, and the container
  // pulled off the screen
  // Whether the dice wait for you to throw them: your turn's roll, when
  // you're playing (the user's: never thrown for you)
  bool yourRollWaits(const TableSnapshot &snapshot) const {
    return mode_ == Mode::Playing && snapshot.waitingForDice && snapshot.state.turn == 0 &&
           snapshot.diceSeed != thrownSeed_;
  }

  void throwDice() {
    // Yours when you say (Roll, or A); a bot's once its turn's banner has
    // gone (the user's)
    const bool ready =
        latest_ && (yourRollWaits(*latest_) ? rollAsked_ : !banner_.showing(gameClock_.seconds()));
    if (dice_ == DiceStage::Idle && latest_ && latest_->waitingForDice &&
        latest_->diceSeed != thrownSeed_ && ready) {
      rollAsked_ = false;
      // (Roll's button goes once they're thrown)
      if (bottomScreen_) {
        bottomScreen_->update(latest_->state, latest_->moves,
                              mode_ == Mode::Playing && latest_->yourMove, false);
        bottomDirty_ = true;
      }
      tray_.throwDice(latest_->diceSeed);
      thrownSeed_ = latest_->diceSeed;
      dice_ = DiceStage::Rolling;
      diceOwed_ = 0;
      lastDiceTime_ = thrownAt_ = gameClock_.seconds();
    }
    if (dice_ == DiceStage::Idle)
      return;
    // Fixed steps of PlatformProfile::diceStep, as many as real time has passed (Gregory,
    // Game Engine Architecture 8.5: integrators want a constant step; Doom's
    // tics, Black Book 5.4): the same throw whatever the frame rate (the DS
    // runs 41 to 60), at most mostDiceSteps a frame so a stall slows the
    // dice rather than snowballing
    const double now = gameClock_.seconds();
    diceOwed_ += now - lastDiceTime_;
    lastDiceTime_ = now;
    int steps = 0;
    for (; diceOwed_ >= kThisPlatform.diceStep && steps < kThisPlatform.mostDiceSteps; ++steps) {
      tray_.step(kThisPlatform.diceStep);
      diceOwed_ -= kThisPlatform.diceStep;
    }
    if (steps == kThisPlatform.mostDiceSteps)
      diceOwed_ = 0; // (behind: let it go)
    // A watchdog (White, Making Embedded Systems, "Watchdog"): dice still
    // moving after kMostSecondsRolling are thrown again (the next seed), so
    // a roll can't hold the game up (none of 60,000 test throws needed it)
    if (dice_ == DiceStage::Rolling && !tray_.settled() && now - thrownAt_ > kMostSecondsRolling) {
      tray_.throwDice(++thrownSeed_);
      thrownAt_ = now;
      return;
    }
    if (dice_ == DiceStage::Rolling && tray_.settled()) {
      dice_ = DiceStage::Showing;
      shownSeconds_ = 0;
      rolled_ = tray_.faces();
    } else if (dice_ == DiceStage::Showing &&
               (shownSeconds_ += steps * kThisPlatform.diceStep) >= kSecondsShowingDice) {
      const auto [first, second] = tray_.faces();
      send(DiceRolled{first, second});
      tray_.startPull();
      dice_ = DiceStage::Pulling;
    } else if (dice_ == DiceStage::Pulling && tray_.offScreen()) {
      dice_ = DiceStage::Idle;
    }
  }

  // The dice on the glass, in front of the board
  void drawDice(const Scene::Camera &camera) {
    if (dice_ == DiceStage::Idle)
      return;
    const auto toFloat = [](cyclone::real r) { return static_cast<float>(static_cast<double>(r)); };
    const Scene::Transform glass = glassFrame(
        camera, static_cast<float>(kTopWidth) / static_cast<float>(kTopHeight),
        static_cast<float>(Dice::DiceTray::kWidth), static_cast<float>(Dice::DiceTray::kDepth));
    for (int d = 0; d < 2; ++d) {
      const cyclone::Vector3 at = tray_.position(d);
      const cyclone::Quaternion turn = tray_.orientation(d);
      renderer_->draw(
          die_,
          dieOnGlass(glass, {toFloat(at.x), toFloat(at.y), toFloat(at.z)},
                     {toFloat(turn.r), toFloat(turn.i), toFloat(turn.j), toFloat(turn.k)},
                     static_cast<float>(tray_.offset())),
          {.shading = Scene::Material::Shading::Lit});
    }
    // Once they've settled, the roll in big words in the middle of the
    // screen on a hexagon (the user's), going with the glass as
    // it's pulled away (its middle, wherever the glass has got to)
    if (dice_ == DiceStage::Rolling)
      return;
    const auto middle =
        onScreen(camera, static_cast<float>(kTopWidth), static_cast<float>(kTopHeight),
                 glass.apply({-static_cast<float>(tray_.offset()), 0, 0}));
    if (!middle)
      return;
    const auto [first, second] = rolled_;
    char total[8], pair[16];
    std::snprintf(total, sizeof total, "%d", first + second);
    std::snprintf(pair, sizeof pair, "%d + %d", first, second);
    Scene::Overlay &overlay = renderer_->overlay();
    // (the user's: a little smaller, so the words sit well inside)
    const float big = kTopWidth >= 400 ? 4 : 3; // (the 3DS's wider screen: bigger)
    const float small = kTopWidth >= 400 ? 1.5f : 1;
    const float radius = kTopWidth >= 400 ? 50.0f : 42.0f;
    const float x = (*middle)[0], y = (*middle)[1];
    // Both solid: a see-through hexagon's six triangles were blended twice
    // along the edges they share, which showed as dark lines
    overlay.hexagon(x, y, radius + 3, Palette::kHighlight); // (an amber edge)
    overlay.hexagon(x, y, radius, {12, 18, 30, 255});
    // The total over the two dice, the two lines together centred on the
    // hexagon by their letters' own pixels (Overlay::textCentred)
    const auto totalInk = Scene::Overlay::inkOf(total, big),
               pairInk = Scene::Overlay::inkOf(pair, small);
    constexpr float kGap = 6;
    const float height = totalInk.height + kGap + pairInk.height;
    overlay.textCentred(x, y - height / 2 + totalInk.height / 2, total, Palette::kHighlight, big);
    overlay.textCentred(x, y + height / 2 - pairInk.height / 2, pair, Palette::kWords, small);
  }

  // The board's light: the game's (a lamp above and to one side, toon
  // bands), or, `flat` of the way to it, bright and even (the setup's: the
  // board seen from above looks like the flat map, the user's)
  static Scene::Environment boardEnvironment(float flat = 0) {
    const auto mix = [&](float game, float even) { return game + (even - game) * flat; };
    Scene::Environment environment;
    environment.background = BoardColours::kSpace;
    environment.ambient = {static_cast<std::uint8_t>(mix(120, 235)),
                           static_cast<std::uint8_t>(mix(120, 235)),
                           static_cast<std::uint8_t>(mix(130, 235))};
    const float lamp = 1 - flat;
    environment.addLight(
        {.direction = {-0.4f, -1, -0.5f},
         .colour = {static_cast<std::uint8_t>(200 * lamp), static_cast<std::uint8_t>(200 * lamp),
                    static_cast<std::uint8_t>(190 * lamp)}});
    return environment;
  }

  // How far the board has come down from above to the game's view since the
  // game started (0 to 1, on the dice's curve: slideAcross), or 1 once there
  float descent() const {
    if (!descentStarted_)
      return 1;
    return slideAcross(static_cast<float>(secondsSince(*descentStarted_)));
  }
  // The game's board (or none yet), with the dice, the words over it and the
  // selection
  void drawBoard(const State *shown) {
    // (coming down from above at the start of a game: the user's idea)
    const float down = descent();
    if (down >= 1 && descentStarted_) {
      descentStarted_.reset();
      // (down: who goes first, shown, the game still held; then it goes on)
      if (flow_.screen() == ScreenFlow::Screen::Game)
        firstRollShownAt_ = Clock::now(); // (real time: the game's clock is held)
    }
    const Scene::Camera camera = cameraBetween(topDownCamera(), gameCamera(turn_), down);
    renderer_->beginFrame(camera, boardEnvironment(1 - down));
    if (!shown && descentStarted_)
      shown = &preview_; // (the game's first state on its way: the same board)
    renderer_->draw(stars_, Scene::Transform::identity(),
                    {.shading = Scene::Material::Shading::Unlit});
    if (shown) {
      auto timing = profile_.time(kProfilePieces);
      drawGaps(*shown, Scene::Transform::identity());
      drawPieces(*shown, Scene::Transform::identity());
    }
    // (behind another player's view, the dice wait unseen: the game's held)
    if (!showingPlayer())
      drawDice(camera);
    drawInfo(camera);
    // (who goes first in place of the usual words: on a DS words are on a
    // layer over every shape, so they'd show through it)
    if (firstRollShownAt_)
      drawFirstRoll(renderer_->overlay());
    else if (showingPlayer())
      drawOtherPlayer(renderer_->overlay(), camera);
    else if (discoveryMoment())
      drawDiscovery(renderer_->overlay(), camera);
    else if (keepDealtAt_)
      drawDealtCards(renderer_->overlay(), camera);
    else
      drawText();
    // The touched corner (or the blockade's hex), ringed on the 3D board too
    if (selected_) {
      const Scene::Material marked{.shading = Scene::Material::Shading::Unlit,
                                   .colour = Palette::kHighlight};
      if (choosingAHex())
        renderer_->draw(hexRing_, BoardPieces::onHex(selected_->hex), marked);
      else
        renderer_->draw(cornerRing_, BoardPieces::onCorner(selected_->corner), marked);
    }
    showProfile();
    {
      auto timing = profile_.time(kProfileWait);
      renderer_->endFrame(); // waits for the screen's refresh
    }
    const Scene::FrameTimes times = renderer_->lastFrameTimes();
    const auto spent = [](float milliseconds) {
      return std::chrono::duration_cast<ScratchPadd::FrameProfile::Clock::duration>(
          std::chrono::duration<float, std::milli>(milliseconds));
    };
    profile_.add(kProfileRenderer, spent(times.cpu));
    profile_.add(kProfileOverlay, spent(times.overlay));
  }

  // The setup's board, still (the user's: turning didn't help); on
  // a change of board, the old one slides out and the new one in, on the
  // dice's curve (slideAcross), the way the arrow pointed
  void drawSetup() {
    const Scene::Camera camera = topDownCamera();
    renderer_->beginFrame(camera, boardEnvironment(1));
    renderer_->draw(stars_, Scene::Transform::identity(),
                    {.shading = Scene::Material::Shading::Unlit});
    const float through =
        sliding_ ? slideAcross(static_cast<float>(secondsSince(slideStarted_))) : 1;
    if (through >= 1)
      sliding_ = false;
    if (!sliding_) {
      drawGaps(preview_, Scene::Transform::identity());
      drawPieces(preview_, Scene::Transform::identity());
    } else {
      // One screen's width at the board's distance, across the screen
      const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
      const Scene::Vector3 right = forward.cross(camera.up).normalised();
      const float away = (camera.target - camera.eye).length();
      const float wide = 2 * away * std::tan(camera.fieldOfView * 3.14159265f / 360) *
                         static_cast<float>(kTopWidth) / static_cast<float>(kTopHeight);
      const float sign = static_cast<float>(slideWay_); // (+1: the new one comes from the right)
      const auto before = Scene::Transform::translation(right * (-sign * through * wide));
      const auto after = Scene::Transform::translation(right * (sign * (1 - through) * wide));
      drawGaps(previewBefore_, before);
      drawPieces(previewBefore_, before);
      drawGaps(preview_, after);
      drawPieces(preview_, after);
    }
    // The board's name over it, and anything to know about it (// the user's: the four-cluster
    // boards' longer games)
    Scene::Overlay &overlay = renderer_->overlay();
    overlay.text(4, 4, menu_.boardName(), Palette::kWords);
    if (!menu_.boardNote().empty())
      overlay.text(4, 16, menu_.boardNote().substr(0, kCharsPerLine), Palette::kHighlight);
    showProfile();
    {
      auto timing = profile_.time(kProfileWait);
      renderer_->endFrame();
    }
  }

  // A dim, see-through grey in the gaps between the tiles (dimmer),
  // made when a board is first drawn (another game, or the setup's choice)
  void drawGaps(const State &state, const Scene::Transform &placement) {
    if (const Scene::Mesh gaps = gapsFor(state))
      renderer_->draw(
          gaps, placement,
          {.shading = Scene::Material::Shading::Unlit, .colour = {150, 160, 180}, .alpha = 4});
  }

  // A board's tiles, chips, stations and ships, placed by `placement`
  void drawPieces(const State &state, const Scene::Transform &placement) {
    // How far through the latest move's animation (0 to 1)
    const float t = static_cast<float>((gameClock_.seconds() - motionStarted_) / kSecondsAnimating);
    const auto toon = [](Scene::Colour colour) {
      return Scene::Material{.shading = Scene::Material::Shading::Toon, .colour = colour};
    };
    // A tile turning over: lifted a little, its face down side first, then
    // its face up, about the axis across the screen
    // A tile with its face (Art.hpp); turning over, lifted a little,
    // its face-down side first, then its face, about the axis across
    const auto drawTile = [&](CosmicHex::Hex hex, std::optional<float> turn,
                              CosmicHex::Art::Face down, CosmicHex::Art::Face up) {
      const auto showing = [&](CosmicHex::Art::Face face) {
        // (a face that didn't load: its colour)
        if (!faces_[static_cast<std::size_t>(face)])
          return Scene::Material{.shading = Scene::Material::Shading::Toon,
                                 .colour = BoardColours::ofFace(face)};
        return Scene::Material{.shading = Scene::Material::Shading::Toon,
                               .texture = faces_[static_cast<std::size_t>(face)]};
      };
      if (!turn) {
        renderer_->draw(hexTile_, placement * BoardPieces::onHex(hex), showing(up));
        return;
      }
      const float half = 1.5707963f;
      const bool upSide = *turn > half;
      const auto lift = Scene::Transform::translation({0, 0.6f * std::sin(*turn), 0});
      const auto spin = Scene::Transform::rotation({1, 0, 0}, upSide ? *turn - 2 * half : *turn);
      renderer_->draw(hexTile_, placement * BoardPieces::onHex(hex) * lift * spin,
                      showing(upSide ? up : down));
    };
    for (std::size_t i = 0; i < state.planets.size(); ++i) {
      // (one turned over is shown big first: face down here till it's
      // back, then simply face up: Discovery)
      const CosmicHex::Hex hex = state.planets[i].hex;
      if (stillDiscovering(hex)) {
        if (!flying(hex))
          drawTile(hex, std::nullopt, CosmicHex::Art::Face::PlanetFaceDown,
                   CosmicHex::Art::Face::PlanetFaceDown);
        continue;
      }
      const bool shownBig = wasShownBig(hex);
      const auto turn = shownBig ? std::nullopt : motion_.planetTurn(i, t);
      drawTile(hex, turn, CosmicHex::Art::Face::PlanetFaceDown, faceOf(state.planets[i]));
      // its number chip, once it's lying face up; one just
      // turned over drops onto it from above and settles (a
      // short bounce on a curve, the same every time)
      const int number = chipOf(state.planets[i]);
      const Scene::Texture chip = chips_[static_cast<std::size_t>(number)];
      if (number > 0 && !turn && chip) {
        float drop = 0;
        if (motion_.planetTurn(i, 0) && !shownBig) {
          const double since = gameClock_.seconds() - motionStarted_ - kSecondsAnimating;
          const float landed = ScratchPadd::Motion::along(
              ScratchPadd::Motion::Curve::Settle, static_cast<float>(since / kSecondsChipDrop));
          drop = since < 0 ? kChipDropHeight : (1 - landed) * kChipDropHeight;
        }
        renderer_->draw(
            numberChip_,
            placement * BoardPieces::onHex(state.planets[i].hex) *
                Scene::Transform::translation({0, drop, 0}),
            Scene::Material{.shading = Scene::Material::Shading::Toon, .texture = chip});
      }
    }
    for (std::size_t i = 0; i < state.zone.size(); ++i) {
      const CosmicHex::Hex hex = state.zone[i].hex;
      if (stillDiscovering(hex)) {
        if (!flying(hex))
          drawTile(hex, std::nullopt, CosmicHex::Art::Face::FindFaceDown,
                   CosmicHex::Art::Face::FindFaceDown);
      } else
        drawTile(hex, wasShownBig(hex) ? std::nullopt : motion_.siteTurn(i, t),
                 CosmicHex::Art::Face::FindFaceDown, faceOf(state.zone[i]));
    }
    for (const CosmicHex::Station &station : state.stations)
      renderer_->draw(station.colony ? colony_ : station_,
                      placement * BoardPieces::onCorner(station.at),
                      toon(BoardColours::ofPlayer(station.owner)));
    // Ships, sliding from where they were
    for (std::size_t i = 0; i < state.ships.size(); ++i)
      renderer_->draw(ship_, placement * Scene::Transform::translation(motion_.shipAt(i, t)),
                      toon(BoardColours::ofPlayer(state.ships[i].owner)));
    if (state.blockade >= 0)
      renderer_->draw(ship_,
                      placement * BoardPieces::onHex(
                                      state.planets[static_cast<std::size_t>(state.blockade)].hex),
                      toon(BoardColours::kBlockade));
  }

  // The bottom screen: each player's points and resources, and the last
  // moves in words (written when a move comes, not each frame)
  // Over the top screen: the round, each player's points and resources in
  // their colour, and the last move in words; before a game, the choice; on
  // your move, the moves you can make
  void drawText() {
    Scene::Overlay &overlay = renderer_->overlay();
    if (!latest_)
      return;
    const State &state = latest_->state;
    char line[64];
    // Each player's points as a row of dashes, the turn's row
    // outlined
    // (the turn's outline easing from the last turn's row to this one)
    std::optional<float> outlineRow;
    if (outlineMovedAt_) {
      const float moved = ScratchPadd::Motion::along(
          ScratchPadd::Motion::Curve::EaseInOut,
          static_cast<float>(secondsSince(*outlineMovedAt_)) / kSecondsOutline);
      if (moved >= 1)
        outlineMovedAt_.reset();
      else
        outlineRow = static_cast<float>(outlineFrom_) +
                     (static_cast<float>(state.turn) - static_cast<float>(outlineFrom_)) * moved;
    }
    for (const PointsBars::Bar &bar : PointsBars::of(state, kTopWidth, outlineRow))
      overlay.rectangle(bar.x, bar.y, bar.width, bar.height, bar.colour);
    drawCorners(overlay, state);
    // (the -watch builds: memory, measured, above the last move)
    if (kWatchAtStart && !memoryLine_.empty())
      overlay.text(4, kTopHeight - 26, memoryLine_.substr(0, kCharsPerLine), {120, 220, 160});
    if (kWatchAtStart && latest_->botGames > 0) {
      char games[32];
      std::snprintf(games, sizeof games, "bot: %d games a move", latest_->botGames);
      overlay.text(4, kTopHeight - 50, games, {120, 220, 160});
      // Where its time went, in ms: guessing, the tree, the rollouts,
      // judging; and rollout moves a game
      const GameBots::SearchTimes &t = latest_->botSearch;
      if (t.games > 0) {
        char where[64];
        std::snprintf(
            where, sizeof where, "g%lld t%lld r%lld j%lld m%lld",
            static_cast<long long>(t.guessing / 1000), static_cast<long long>(t.descending / 1000),
            static_cast<long long>(t.rollingOut / 1000), static_cast<long long>(t.judging / 1000),
            static_cast<long long>(t.rolloutActions / t.games));
        overlay.text(4, kTopHeight - 122, where, {120, 220, 160});
      }
    }
    if (kWatchAtStart && !memoryBusiest_.empty())
      overlay.text(4, kTopHeight - 38, memoryBusiest_.substr(0, kCharsPerLine), {120, 220, 160});
    if (!latest_->lastMoves.empty() && shownInfo_ == BottomScreen::Info::None && !infoClosingAt_)
      overlay.text(4, kTopHeight - 14, latest_->lastMoves.back().substr(0, kCharsPerLine),
                   Palette::kSoftWords);
    // (the list of moves: debug builds only, when turned on)
    if (mode_ == Mode::Playing && latest_->yourMove && shownInfo_ == BottomScreen::Info::None &&
        !infoClosingAt_ && bottomScreen_ && bottomScreen_->showingMoveList())
      drawMoves(overlay);
    drawInfoWords(overlay, state);
    drawBanner(overlay);
    if (state.winner >= 0) {
      std::snprintf(line, sizeof line, "Player %d wins!", state.winner + 1);
      overlay.text(kTopWidth / 2.0f - 60, kTopHeight / 2.0f - 10, line, Palette::kHighlight, 1.5f);
    }
  }

  // The top corners (the user's): the round top left, in a
  // turquoise hexagon with ROUND under it (calmer than the roll's amber: it's
  // always there), and the badges top right, towards the next point (3 make
  // one at once), the player's own when playing, the turn's when watching
  void drawCorners(Scene::Overlay &overlay, const State &state) {
    const float k = static_cast<float>(kTopWidth) / 256.0f;
    constexpr Scene::Colour kTurquoise = Palette::kAccent, kWhite = Palette::kWords,
                            kDim = Palette::kDimWords, kAmber = Palette::kHighlight,
                            kEmpty{90, 100, 120};
    const auto hexagonEdge = [&](float x, float y, float r, Scene::Colour colour) {
      for (int i = 0; i < 6; ++i) {
        const float a = 3.14159265f / 6 + static_cast<float>(i) * 3.14159265f / 3;
        const float b = a + 3.14159265f / 3;
        overlay.line(x + r * std::cos(a), y + r * std::sin(a), x + r * std::cos(b),
                     y + r * std::sin(b), colour, 1);
      }
    };
    const float rx = 22 * k, ry = 19 * k;
    hexagonEdge(rx, ry, 13 * k, kTurquoise);
    overlay.textCentred(rx, ry, std::to_string(state.round), kWhite,
                        kTopWidth >= 400 ? 2.0f : 1.5f);
    overlay.textCentred(rx, ry + 20 * k, "ROUND", kDim);
    const int whose = mode_ == Mode::Playing ? 0 : state.turn;
    const int badges = whose >= 0 ? state.player[static_cast<std::size_t>(whose)].badges : 0;
    const float bx = static_cast<float>(kTopWidth) - 36 * k;
    for (int i = 0; i < 3; ++i) {
      const float x = bx + static_cast<float>(i) * 12 * k, y = 16 * k;
      if (i < badges)
        overlay.hexagon(x, y, 5 * k, kAmber, true);
      else
        hexagonEdge(x, y, 5 * k, kEmpty);
    }
    overlay.textCentred(bx + 12 * k, 30 * k, "BADGES", kDim);
  }

  // Whose cards an info view shows: yours when playing, the turn's when
  // watching
  int infoPlayer(const State &state) const { return mode_ == Mode::Playing ? 0 : state.turn; }

  // Where a pile of resource `r` stands on the top screen; how far a card
  // has come in (0 to 1), `order` cards after the first
  struct PilePlace {
    float x, y;
  };
  PilePlace pilePlace(int r) const {
    // Five piles across with a little room between, their
    // bottoms just above the screen's edge
    const float k = static_cast<float>(kTopWidth) / 256.0f;
    const float gap = 6 * k, w = static_cast<float>(resourceWidth_);
    const float left = (static_cast<float>(kTopWidth) - (5 * w + 4 * gap)) / 2;
    return {left + static_cast<float>(r) * (w + gap),
            static_cast<float>(kTopHeight) - static_cast<float>(resourceWidth_ * 52 / 63) - 6 * k};
  }
  // How far card `order` has come in (0 off the screen, 1 in place): each
  // a little after the last; closing, each going back the same way
  float cardArrived(int order) const {
    const Clock::time_point from = infoClosingAt_ ? *infoClosingAt_ : infoOpenedAt_;
    const float since = std::chrono::duration<float>(Clock::now() - from).count() -
                        kSecondsCardStagger * static_cast<float>(order);
    const float moved =
        ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::EaseInOut, since / kSecondsCard);
    return infoClosingAt_ ? 1 - moved : moved;
  }
  // Show resources on the top screen: open, or its cards still going off
  bool showingResources() {
    if (shownInfo_ == BottomScreen::Info::Resources)
      return true;
    if (!infoClosingAt_ || closingInfo_ != BottomScreen::Info::Resources)
      return false;
    // (gone once the last card's off: at most 5 piles of 5 and 12 cards)
    constexpr int kMostCards = 5 * 5 + 12;
    if (std::chrono::duration<float>(Clock::now() - *infoClosingAt_).count() >
        kSecondsCard + kSecondsCardStagger * kMostCards) {
      infoClosingAt_.reset();
      return false;
    }
    return true;
  }

  // Show resources: your resources as piles along the bottom
  // of the top screen, a card for each you hold (5 at most), each under
  // the last showing its band; your action cards across the top half; each
  // card easing in on its own, the piles from below, the cards from above
  void drawInfo(const Scene::Camera &camera) {
    if (!latest_ || !showingResources() || !pictureSquare_)
      return;
    const State &state = latest_->state;
    const auto &player = state.player[static_cast<std::size_t>(infoPlayer(state))];
    const float k = static_cast<float>(kTopWidth) / 256.0f;
    const auto screenW = static_cast<float>(kTopWidth), screenH = static_cast<float>(kTopHeight);
    const auto rw = static_cast<float>(resourceWidth_),
               rh = static_cast<float>(resourceWidth_ * 52 / 63);
    int order = 0;
    for (int r = 0; r < CosmicHex::kResources; ++r) {
      const int held = player.resources[static_cast<std::size_t>(r)];
      const int shown = std::max(1, std::min(held, 5));
      const PilePlace place = pilePlace(r);
      for (int j = shown - 1; j >= 0; --j) {
        // (each card under the top one 5 pixels higher: its band shows)
        const float y = place.y - static_cast<float>(j) * kPileStep * k;
        const float in = cardArrived(order++);
        const float slid = y + (1 - in) * (screenH - y + 4);
        renderer_->draw(resourceSquare_,
                        onScreenAt(camera, screenW, screenH, place.x, slid, rw, rh,
                                   kPictureDistance - 0.02f * static_cast<float>(shown - j)),
                        {.shading = Scene::Material::Shading::Unlit,
                         .texture = resourcePictures_[static_cast<std::size_t>(r)],
                         .alpha = static_cast<std::uint8_t>(held == 0 ? 10 : 31)});
      }
    }
    // The action cards across the top, smaller, each easing down
    const auto w = static_cast<float>(cardWidth_), h = static_cast<float>(cardWidth_ * 52 / 63);
    const std::vector<CosmicHex::Art::Card> actions = actionCardsOf(player);
    const float gap = 8 * k, total = static_cast<float>(actions.size()) * (w + gap) - gap;
    for (std::size_t i = 0; i < actions.size(); ++i) {
      const float x = (screenW - total) / 2 + static_cast<float>(i) * (w + gap), y = 44 * k;
      const float in = cardArrived(order++);
      renderer_->draw(pictureSquare_,
                      onScreenAt(camera, screenW, screenH, x, y - (1 - in) * (y + h + 4), w, h),
                      {.shading = Scene::Material::Shading::Unlit,
                       .texture = cardPictures_[static_cast<std::size_t>(actions[i])]});
    }
  }

  static std::vector<CosmicHex::Art::Card> actionCardsOf(const CosmicHex::Player &player) {
    std::vector<CosmicHex::Art::Card> out;
    for (int id : player.contracts)
      if (const auto card = cardOf(id))
        out.push_back(*card);
    return out;
  }

  // Show resources' words: each pile's title on its top card, a badge past
  // five; the action cards' titles under them
  void drawInfoWords(Scene::Overlay &overlay, const State &state) {
    if (!showingResources())
      return;
    const auto &player = state.player[static_cast<std::size_t>(infoPlayer(state))];
    const float k = static_cast<float>(kTopWidth) / 256.0f;
    const float u = static_cast<float>(resourceWidth_) / CosmicHex::Art::kCardWidth;
    int order = 0; // (the cards come in one after another, as drawInfo has them)
    for (int r = 0; r < CosmicHex::kResources; ++r) {
      const int held = player.resources[static_cast<std::size_t>(r)];
      const int shown = std::max(1, std::min(held, 5));
      const PilePlace place = pilePlace(r);
      const float top = place.y - static_cast<float>(shown - 1) * kPileStep * k;
      const float middle = place.x + static_cast<float>(resourceWidth_) / 2;
      // The front card: the lowest, at the pile's foot (those under it
      // show above it), coming in last of its pile; its title moves with it
      order += shown;
      const float in = cardArrived(order - 1);
      const float front = place.y + (1 - in) * (static_cast<float>(kTopHeight) - place.y + 4);
      // The title in the front card's band, dark on a light band (as the
      // cards are drawn); faint when there are none (it was
      // on the card at the back, too high)
      const Scene::Colour band = resourceBands_[static_cast<std::size_t>(r)];
      const float light = 0.2126f * band.r + 0.7152f * band.g + 0.0722f * band.b;
      const Scene::Colour ink = held == 0     ? Scene::Colour{120, 128, 145}
                                : light > 140 ? Scene::Colour{16, 20, 30}
                                              : Scene::Colour{242, 245, 251};
      overlay.textCentred(
          middle,
          front + (CosmicHex::Art::kCardTitle.y + CosmicHex::Art::kCardTitle.height / 2) * u,
          CosmicHex::Art::wordsOf(cardOf(static_cast<CosmicHex::Resource>(r))).shortTitle, ink);
      // Past five, how many in a badge centred above the pile
      if (held > 5) {
        const std::string count = std::to_string(held);
        const float bw = 14 * k, bh = 10 * k, by = top - 13 * k;
        overlay.rectangle(middle - bw / 2, by, bw, bh, {58, 74, 106});
        overlay.textCentred(middle, by + bh / 2, count, Palette::kWords);
      }
    }
    // The action cards' short titles, under each
    const std::vector<CosmicHex::Art::Card> actions = actionCardsOf(player);
    const auto w = static_cast<float>(cardWidth_), h = static_cast<float>(cardWidth_ * 52 / 63);
    const float gap = 8 * k, total = static_cast<float>(actions.size()) * (w + gap) - gap;
    for (std::size_t i = 0; i < actions.size(); ++i) {
      const float x =
          (static_cast<float>(kTopWidth) - total) / 2 + static_cast<float>(i) * (w + gap);
      overlay.textCentred(x + w / 2, 44 * k + h + 6 * k,
                          CosmicHex::Art::wordsOf(actions[i]).shortTitle, Palette::kWords);
    }
  }

  // The turn banner as it moves (ScratchPadd::Banner): a band softly in the player's
  // colour with turquoise edges, the words sweeping through the middle
  void drawBanner(Scene::Overlay &overlay) {
    // Paused: the banner still, in a neutral grey, rather than the turn's
    // (the user's: plainer to see than a word in the corner)
    // (only when paused by you, not held by a view or a discovery; its
    // own flag, as a hold's let go reaches the game a frame or two later)
    const bool pausedByYou = latest_ && latest_->paused && mode_ == Mode::Watching && youPaused_;
    if (flow_.screen() == ScreenFlow::Screen::Paused || pausedByYou) {
      static const std::string kTitle = "PAUSED";
      const std::string note =
          flow_.screen() == ScreenFlow::Screen::Paused ? "the menu is below" : "A to go on";
      drawBand(overlay, {.bandAlpha = 1,
                         .wordsX = static_cast<float>(kTopWidth) / 2,
                         .title = kTitle,
                         .note = note,
                         .colour = {140, 146, 158}});
      return;
    }
    if (const auto look = banner_.at(gameClock_.seconds()))
      drawBand(overlay, *look);
  }
  // A banner's band and words, as `look` says (ScratchPadd::Banner::draw,
  // in the game's colours)
  void drawBand(Scene::Overlay &overlay, const ScratchPadd::Banner::Look &look) {
    ScratchPadd::Banner::draw(overlay, look,
                              {.screenWidth = static_cast<float>(kTopWidth),
                               .screenHeight = static_cast<float>(kTopHeight),
                               .scale = static_cast<float>(kTopWidth) / 256.0f,
                               .accent = Palette::kAccent,
                               .titleScale = kTopWidth >= 400 ? 2.0f : 1.5f});
  }

  // The buttons, by screen (ScreenFlow): the title's and the pause menu's
  // buttons (up, down, A), the setup's menu (StartMenu: up, down, left,
  // right, A to start, B back), any button once a game's won; in a game,
  // watching: A pauses or goes on; playing, on your move: up and down choose
  // from the list (the moves at the touched spot, or all of them), A makes
  // it, B shows them all again
  void handleButtons(const Input::Edges &pressed, bool zooming = false) {
    using Input::Button;
    // Confirm and Back by the pad's own convention (Input's menu roles: on
    // a DS and 3DS, A and B)
    const Button confirm = lastPad_.buttonFor(Input::Role::Confirm);
    const Button back = lastPad_.buttonFor(Input::Role::Back);
    // (A: on from who goes first)
    if (firstRollShownAt_ && pressed.pressed(confirm)) {
      endFirstRoll();
      return;
    }
    switch (flow_.screen()) {
    case ScreenFlow::Screen::Title:
    case ScreenFlow::Screen::Paused: {
      ButtonMenu &menu = flow_.screen() == ScreenFlow::Screen::Title ? titleMenu_ : pauseMenu_;
      std::optional<std::size_t> chosen;
      for (const auto &[button, key] : {std::pair{Button::Up, ButtonMenu::Key::Up},
                                        std::pair{Button::Down, ButtonMenu::Key::Down},
                                        std::pair{confirm, ButtonMenu::Key::Confirm}})
        if (pressed.pressed(button)) {
          chosen = menu.press(key);
          bottomDirty_ = true;
        }
      if (chosen)
        choose(*chosen);
      return;
    }
    case ScreenFlow::Screen::Setup: {
      using Key = StartMenu::Key;
      StartMenu::Outcome outcome = StartMenu::Outcome::None;
      for (const auto &[button, key] :
           {std::pair{Button::Up, Key::Up}, std::pair{Button::Down, Key::Down},
            std::pair{Button::Left, Key::Left}, std::pair{Button::Right, Key::Right},
            std::pair{confirm, Key::Confirm}, std::pair{back, Key::Back}})
        if (pressed.pressed(button) && outcome == StartMenu::Outcome::None) {
          if (key == Key::Left || key == Key::Right)
            slideWay_ = key == Key::Right ? 1 : -1;
          outcome = menu_.press(key);
          bottomDirty_ = true;
          updatePreview();
        }
      setupOutcome(outcome);
      return;
    }
    case ScreenFlow::Screen::Won:
      if (pressed.pressed(confirm) || pressed.pressed(back))
        apply(flow_.on(ScreenFlow::Event::Continue));
      return;
    case ScreenFlow::Screen::Game:
      break;
    }
    if (mode_ == Mode::Watching) {
      if (pressed.pressed(confirm)) {
        send(TogglePause{});
        youPaused_ = !youPaused_;
      }
      return;
    }
    // A throws your dice when your roll waits (as Roll does)
    if (latest_ && yourRollWaits(*latest_) && pressed.pressed(confirm)) {
      rollAsked_ = true;
      return;
    }
    if (!latest_ || !latest_->yourMove)
      return;
    // A choosing screen open on the bottom: its buttons (BottomScreen::press:
    // the D-pad moves, A picks then confirms, B goes back)
    if (bottomScreen_) {
      using Key = BottomScreen::Key;
      std::optional<Key> key;
      if (pressed.pressed(Button::Up))
        key = Key::Up;
      else if (pressed.pressed(Button::Down))
        key = Key::Down;
      else if (pressed.pressed(Button::Left) || (!zooming && pressed.pressed(Button::L)))
        key = Key::Left;
      else if (pressed.pressed(Button::Right) || (!zooming && pressed.pressed(Button::R)))
        key = Key::Right;
      else if (pressed.pressed(confirm))
        key = Key::Confirm;
      else if (pressed.pressed(back))
        key = Key::Back;
      if (key) {
        const BottomScreen::Outcome outcome = bottomScreen_->press(*key);
        if (outcome.play) {
          send(PlayMove{*outcome.play});
          latest_->yourMove = false;
          selected_.reset();
        }
        if (outcome.used) {
          bottomDirty_ = true;
          return;
        }
      }
    }
    // The list of moves' keys, only while it shows (debug builds)
    if (!bottomScreen_ || !bottomScreen_->showingMoveList())
      return;
    const std::vector<std::size_t> shown = movesShown();
    if (pressed.pressed(Button::Down) && cursor_ + 1 < shown.size())
      ++cursor_;
    if (pressed.pressed(Button::Up) && cursor_ > 0)
      --cursor_;
    if (pressed.pressed(back)) {
      selected_.reset();
      cursor_ = 0;
      bottomDirty_ = true;
    }
    if (pressed.pressed(confirm) && cursor_ < shown.size()) {
      send(PlayMove{latest_->moves[shown[cursor_]]});
      latest_->yourMove = false; // (until the game says it's yours again)
      selected_.reset();
      cursor_ = 0;
      bottomDirty_ = true;
    }
  }

  // Whether your move is on a hex (the blockade's, after a 7), not a corner
  bool choosingAHex() const {
    return latest_ && latest_->yourMove && latest_->state.phase == CosmicHex::Phase::Blockade;
  }

  // The moves listed on your move: those at the touched spot, or all of
  // them (their places in the snapshot's list)
  std::vector<std::size_t> movesShown() const {
    std::vector<std::size_t> shown;
    if (!latest_)
      return shown;
    for (std::size_t i = 0; i < latest_->moves.size(); ++i)
      if (!selected_ || concerns(latest_->state, latest_->moves[i], *selected_))
        shown.push_back(i);
    return shown;
  }

  // A touch: a tap selects the corner (and hex) under it; a drag moves the
  // board, when it's zoomed in
  void handleTouch(const Input::Touch &touch) {
    const ScreenFlow::Screen screen = flow_.screen();
    if (screen != ScreenFlow::Screen::Game) {
      // (a tap, when the stylus comes down: the screen's menu)
      if (touch.down && !touchWasDown_) {
        if (screen == ScreenFlow::Screen::Setup) {
          slideWay_ = touch.x > kBottomWidth / 2 ? 1 : -1; // (the right arrow, or the left)
          setupOutcome(menu_.touch(touch.x, touch.y, kBottomWidth, kBottomHeight));
          updatePreview();
        } else if (screen == ScreenFlow::Screen::Won) {
          apply(flow_.on(ScreenFlow::Event::Continue));
        } else {
          ButtonMenu &menu = screen == ScreenFlow::Screen::Title ? titleMenu_ : pauseMenu_;
          if (const auto chosen = menu.touch(touch.x, touch.y, kBottomWidth, kBottomHeight))
            choose(*chosen);
        }
        bottomDirty_ = true;
      }
      touchWasDown_ = touch.down;
      return;
    }
    if (touch.down && !touchWasDown_) {
      touchStart_ = touchLast_ = {touch.x, touch.y};
      dragged_ = false;
    } else if (touch.down) {
      const int dx = touch.x - touchLast_[0], dy = touch.y - touchLast_[1];
      // (dragging moves the board only while the board is showing)
      if (std::abs(touch.x - touchStart_[0]) + std::abs(touch.y - touchStart_[1]) > 6 &&
          onTheBoard())
        dragged_ = true;
      if (dragged_ && (dx != 0 || dy != 0)) {
        picture_->pan(static_cast<float>(dx), static_cast<float>(dy));
        bottomDirty_ = true;
      }
      touchLast_ = {touch.x, touch.y};
    } else if (touchWasDown_ && !dragged_) {
      const auto spot = picture_->at(touchStart_[0], touchStart_[1]);
      // The bottom screen's first: its buttons, its choosing screens, a
      // place for what's being built; what it doesn't use selects a corner
      const BottomScreen::Outcome outcome =
          bottomScreen_ ? bottomScreen_->tap(touchStart_[0], touchStart_[1],
                                             onTheBoard() ? spot : std::nullopt)
                        : BottomScreen::Outcome{};
      if (outcome.roll && latest_ && yourRollWaits(*latest_))
        rollAsked_ = true;
      if (outcome.play && latest_ && latest_->yourMove) {
        send(PlayMove{*outcome.play});
        latest_->yourMove = false; // (until the game says it's yours again)
        selected_.reset();
      } else if (!outcome.used) {
        selected_ = spot;
      }
      cursor_ = 0;
      bottomDirty_ = true;
    }
    touchWasDown_ = touch.down;
  }

  // The stylus as the scripted-taps builds have it: each tap down for a few
  // frames at its time (else the stylus as it is)
  Input::Touch scripted(const Input::Touch &touch) {
    const double now = std::chrono::duration<double>(Clock::now() - startedAt_).count();
    for (const ScriptedTap &tap : kScriptedTaps)
      if (now >= tap.seconds && now < tap.seconds + 0.1) {
        const float k = static_cast<float>(kBottomWidth) / 256.0f;
        Input::Touch down = touch;
        down.down = true;
        down.x = static_cast<std::int16_t>(static_cast<float>(tap.x) * k);
        down.y = static_cast<std::int16_t>(static_cast<float>(tap.y) * k);
        return down;
      }
    return touch;
  }

  // Whether the bottom screen shows the board (rather than a choosing screen)
  bool onTheBoard() const {
    return !bottomScreen_ || bottomScreen_->mode() == BottomScreen::Mode::Board ||
           bottomScreen_->mode() == BottomScreen::Mode::Placing;
  }

  // Your moves, under the scores: a window of the list round the cursor,
  // the chosen one highlighted
  void drawMoves(Scene::Overlay &overlay) {
    const std::vector<std::size_t> shown = movesShown();
    overlay.rectangle(0, 60, kTopWidth, kTopHeight - 76, {12, 18, 30, 210});
    const char *heading =
        selected_ ? "Your moves here (B: all moves)" : "Your moves (touch the board to narrow)";
    overlay.text(6, 64, heading, Palette::kHighlight);
    if (shown.empty()) {
      overlay.text(6, 80, "None here: touch elsewhere, or B for all", Palette::kSoftWords);
      return;
    }
    const auto kLines = static_cast<std::size_t>((kTopHeight - 96) / kLineHeight);
    const std::size_t first = cursor_ >= kLines ? cursor_ - kLines + 1 : 0;
    for (std::size_t line = 0; line < kLines && first + line < shown.size(); ++line) {
      const std::size_t i = first + line;
      std::string text = (i == cursor_ ? "> " : "  ") + latest_->moveText[shown[i]];
      if (text.size() > kCharsPerLine)
        text = text.substr(0, kCharsPerLine - 2) + "..";
      overlay.text(6, 78 + kLineHeight * static_cast<float>(line), text,
                   i == cursor_ ? Palette::kHighlight : Palette::kSoftWords);
    }
  }

  // The bottom screen: the board drawn flat, the selection marked, copied
  // into its framebuffer (240 x 320, a column at a time from the bottom
  // left: the screen is turned on its side; three bytes a pixel, B G R)
  void drawBottom() {
    bottomDirty_ = false;
    std::span<const Scene::Colour> drawn;
    bool showingMap = false; // (the map, rather than a menu or a choosing screen)
    const ScreenFlow::Screen screen = flow_.screen();
    if (screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Paused) {
      menuPicture_.draw(screen == ScreenFlow::Screen::Title ? titleMenu_ : pauseMenu_);
      drawn = menuPicture_.pixels();
    } else if (screen == ScreenFlow::Screen::Setup) {
      menuPicture_.draw(menu_);
      drawn = menuPicture_.pixels();
    } else {
      if (!latest_)
        return;
      // (the map drawn when the game changes, not again each frame of the
      // fade)
      // A choosing screen in the board's place, or the board with its
      // buttons drawn straight onto it (no copy of the map)
      if (bottomScreen_->coversBoard()) {
        bottomScreen_->draw(*bottomCanvas_);
        drawn = bottomCanvas_->pixels();
      } else {
        if (!fading_) {
          // The touched corner (or the blockade's hex), and where what's
          // being built may go
          BoardPicture::Marks marks;
          if (selected_ && choosingAHex())
            marks.hex = selected_->hex;
          else if (selected_)
            marks.corners.push_back(selected_->corner);
          // (and at the start, what's been touched, shown as the piece)
          const std::optional<CosmicHex::Action> chosen = bottomScreen_->startPlacement();
          std::optional<CosmicHex::Corner> chosenAt;
          if (chosen)
            chosenAt = CosmicHex::boardCorners()[static_cast<std::size_t>(chosen->a)];
          for (const CosmicHex::Corner &corner : bottomScreen_->placingCorners())
            if (corner != chosenAt)
              marks.corners.push_back(corner);
          if (chosen)
            marks.preview = BoardPicture::Marks::Preview{
                *chosenAt, latest_->state.turn, chosen->kind == CosmicHex::Action::Kind::PlaceShip};
          // The map under the buttons, where they were and are, drawn
          // again, so they're drawn over it once (the map's
          // draw changes only what's changed)
          for (const BoardPicture::Area &area : overBoardAreas_)
            picture_->redrawNext(area);
          overBoardAreas_ = bottomScreen_->overBoardAreas();
          for (const BoardPicture::Area &area : overBoardAreas_)
            picture_->redrawNext(area);
          {
            auto timing = profile_.time(kProfileMap);
            // Tiles turned over still face down on the map until the top
            // screen has shown them, then each fades in (Discovery)
            if (discoveries_.empty()) {
              picture_->draw(latest_->state, marks);
            } else {
              CosmicHex::State waiting = latest_->state;
              for (std::size_t i = 1; i < discoveries_.size(); ++i) {
                for (CosmicHex::Planet &planet : waiting.planets)
                  if (planet.hex == discoveries_[i].hex)
                    planet.flipped = false;
                for (CosmicHex::ZoneSite &site : waiting.zone)
                  if (site.hex == discoveries_[i].hex)
                    site.flipped = false;
              }
              const auto moment = discoveryMoment();
              marks.revealing = BoardPicture::Marks::Revealing{
                  .hex = discoveries_.front().hex, .shown = moment ? moment->fade : 0.0f};
              picture_->draw(waiting, marks);
            }
          }
          auto timing = profile_.time(kProfileButtons);
          Canvas board = Canvas::over(picture_->pixelsToDrawOver(), kBottomWidth, kBottomHeight);
          bottomScreen_->drawOverBoard(board);
        }
        drawn = picture_->pixels();
        showingMap = true;
      }
    }
    // Fading from the menu to the map as the board comes down: the setup
    // menu's picture (menuPicture_, not drawn again in a game) and the map,
    // mixed as they go to the screen, in whole numbers (no floating point
    // and no buffers of their own: the DS has neither the hardware nor the
    // memory)
    const float down = descent();
    std::optional<std::span<const Scene::Colour>> from;
    int weight = 256; // (of the map, out of 256)
    if (fadeFromMenu_ && down < 1 && menuPicture_.pixels().size() == drawn.size()) {
      from = menuPicture_.pixels();
      weight = static_cast<int>(down * 256);
      bottomDirty_ = true; // (and again next frame, until it's done)
    } else if (fadeFromMenu_ && down >= 1) {
      fadeFromMenu_ = false;
      bottomDirty_ = true; // (once more: the map as the game is now)
    }
    fading_ = from.has_value();
    // The title fading in from black (the menu below it too)
    const bool fromBlack = !from.has_value() && onAMenuScreen() && titleFadedIn() < 1;
    if (fromBlack) {
      weight = static_cast<int>(titleFadedIn() * 256);
      bottomDirty_ = true; // (and again next frame, until it's done)
    }
    const std::span<const Scene::Colour> pixels = drawn;
    auto copying = profile_.time(kProfileOut); // (the picture to the screen)
    // A pixel's colour on its way to the screen: the picture's, or mid-fade
    // the menu's moved `weight` / 256 of the way to it. (Plain arithmetic on
    // the bytes: as a lambda a pixel this took 120 ms a frame on the DS,
    // the copy alone 2 to 6)
    const Scene::Colour *to = pixels.data();
    const Scene::Colour *mixFrom = from ? from->data() : nullptr;
    // (the map shown again: only its rows that changed; else all of it)
    const bool mapAgain = showingMap && screenShowsMap_;
    screenShowsMap_ = showingMap && !mixFrom && !fromBlack;
    if (!mixFrom && !fromBlack) {
      if (!mapAgain) {
        bottom_->present(kBottomWidth, kBottomHeight, to); // (as it is: the quick copy)
        return;
      }
      int fromRow = kBottomHeight, toRow = 0;
      for (const BoardPicture::Area &area : picture_->changed()) {
        fromRow = std::min(fromRow, area.y);
        toRow = std::max(toRow, area.y + area.height);
      }
      if (fromRow < toRow)
        bottom_->present(kBottomWidth, kBottomHeight, to, fromRow, toRow);
      return;
    }
    const auto colourAt = [to, mixFrom, weight](std::size_t i) -> Scene::Colour {
      const Scene::Colour b = to[i];
      if (!mixFrom) // (from black)
        return {static_cast<std::uint8_t>((b.r * weight) >> 8),
                static_cast<std::uint8_t>((b.g * weight) >> 8),
                static_cast<std::uint8_t>((b.b * weight) >> 8), 255};
      const Scene::Colour a = mixFrom[i];
      return {static_cast<std::uint8_t>(a.r + (((b.r - a.r) * weight) >> 8)),
              static_cast<std::uint8_t>(a.g + (((b.g - a.g) * weight) >> 8)),
              static_cast<std::uint8_t>(a.b + (((b.b - a.b) * weight) >> 8)), 255};
    };
    bottom_->present(kBottomWidth, kBottomHeight, colourAt);
  }

  std::optional<Scene::Display> display_;
  std::optional<Scene::Renderer> renderer_;
  // Each tile face (Art.hpp) drawn into a texture from its recipe (kFaceSize a side),
  // once, when the game starts
  // The cards' pictures for the top screen (ScreenPictures): each
  // card's drawing down to its picture's foot, no words (titles are the
  // overlay's, sharp), cardWidth_ across; once, as the DS hands out its
  // texture memory once (32 wide there: 2 KB a card)
  void loadCardPictures() {
    // Resources bigger (the user's: room for their titles, and
    // the cards under the top one easy to see), action cards smaller: on
    // the DS 44 and 32 pixels wide, 8 and 2 KB each, all of them fitting
    // beside the tile faces in the 128 KB the textures share
    resourceWidth_ = kTopWidth >= 400 ? 64 : 44;
    cardWidth_ = kTopWidth >= 400 ? 44 : 32;
    const auto load = [&](CosmicHex::Art::Card card, int width, float &uMax, float &vMax) {
      const int height = width * 52 / 63;
      const auto whole = recipePixels(CosmicHex::Art::cardRecipeOf(card), width, width * 88 / 63,
                                      kThisPlatform.recipeSmoothing);
      if (whole.empty())
        return Scene::Texture{};
      const std::vector<Scene::Colour> top(whole.begin(), whole.begin() + width * height);
      // (the band's colour, for its title's letters: dark on a light band)
      if (static_cast<int>(card) < CosmicHex::kResources)
        resourceBands_[static_cast<std::size_t>(card)] =
            top[static_cast<std::size_t>((width * 11 / 63) * width + width * 6 / 63)];
      const PaddedPicture picture = padded(top, width, height);
      uMax = picture.uMax;
      vMax = picture.vMax;
      return renderer_->load(Scene::TextureImage{picture.width, picture.height, picture.pixels});
    };
    float uMax = 1, vMax = 1;
    for (int r = 0; r < CosmicHex::kResources; ++r)
      resourcePictures_[static_cast<std::size_t>(r)] =
          load(static_cast<CosmicHex::Art::Card>(r), resourceWidth_, uMax, vMax);
    resourceSquare_ = renderer_->load(pictureSquare(uMax, vMax).data());
    for (int c = CosmicHex::kResources; c < CosmicHex::Art::kCards; ++c)
      cardPictures_[static_cast<std::size_t>(c)] =
          load(static_cast<CosmicHex::Art::Card>(c), cardWidth_, uMax, vMax);
    pictureSquare_ = renderer_->load(pictureSquare(uMax, vMax).data());
    // The close-ups' textures, blank until one's shown (closeUpTile and
    // closeUpCards draw into them)
    const int size = kThisPlatform.closeUpSize;
    const Scene::TextureImage blank{
        size, size, std::vector<Scene::Colour>(static_cast<std::size_t>(size * size))};
    closeUpFace_ = renderer_->load(blank);
    for (Scene::Texture &card : closeUpCards_)
      card = renderer_->load(blank);
    closeUpCardSquare_ = renderer_->load(
        pictureSquare(1, static_cast<float>(size * 52 / 63) / static_cast<float>(size)).data());
    // The two backs, whole cards (the planets for resource cards, the rings
    // for action cards: the agreed designs), for cards held face down
    for (const CosmicHex::Art::CardBack design :
         {CosmicHex::Art::CardBack::Rings, CosmicHex::Art::CardBack::Planets}) {
      const auto whole = recipePixels(CosmicHex::Art::cardBackRecipe(design), cardWidth_,
                                      cardWidth_ * 88 / 63, kThisPlatform.recipeSmoothing);
      if (whole.empty())
        continue;
      const PaddedPicture picture = padded(whole, cardWidth_, cardWidth_ * 88 / 63);
      backPictures_[static_cast<std::size_t>(design)] =
          renderer_->load(Scene::TextureImage{picture.width, picture.height, picture.pixels});
      backSquare_ = renderer_->load(pictureSquare(picture.uMax, picture.vMax).data());
    }
  }

  void loadFaces() {
    for (int i = 0; i < CosmicHex::Art::kFaces; ++i) {
      const auto face = static_cast<CosmicHex::Art::Face>(i);
      // The map's, shrunk, when it's no smaller (the DS: 35 pixels to 32,
      // Half the start's drawing); else from its recipe
      const std::vector<Scene::Colour> pixels =
          picture_->wholeFaceSize() >= kFaceSize
              ? shrunk(picture_->wholeFace(face), picture_->wholeFaceSize(), kFaceSize)
              : recipePixels(CosmicHex::Art::recipeOf(face), kFaceSize,
                             kThisPlatform.recipeSmoothing);
      if (pixels.empty())
        continue; // (the tile shows white: a broken recipe, tested against on the Mac)
      faces_[static_cast<std::size_t>(i)] =
          renderer_->load(Scene::TextureImage{kFaceSize, kFaceSize, pixels});
    }
    // and the number chips, half a face's size
    constexpr int kChipSize = kFaceSize / 2;
    for (int n = CosmicHex::Art::kLowestChip; n <= CosmicHex::Art::kHighestChip; ++n) {
      const std::vector<Scene::Colour> pixels =
          recipePixels(CosmicHex::Art::chipRecipeOf(n), kChipSize, kThisPlatform.recipeSmoothing);
      if (pixels.empty())
        continue;
      chips_[static_cast<std::size_t>(n)] =
          renderer_->load(Scene::TextureImage{kChipSize, kChipSize, pixels});
    }
  }

  Scene::Mesh hexTile_, numberChip_, station_, colony_, ship_, stars_, hexRing_, cornerRing_;
  std::array<Scene::Texture, CosmicHex::Art::kFaces> faces_{};
  std::array<Scene::Texture, CosmicHex::Art::kHighestChip + 1> chips_{}; // (by number)
  // Memory, measured (with the counting allocator in the build)
  std::optional<ScratchPadd::MemoryStats::Reader> memory_;
  Clock::time_point memoryAt_;
  std::string memoryLine_, memoryBusiest_;
  // Where each frame's time goes (the DS's stutter), shown on
  // the -watch builds: the bottom screen, the dice, the top screen (its
  // wait for the refresh included), and that wait
  // (and, the board's pieces, the renderer's own CPU time and
  // its overlay's, from Scene::FrameTimes, and of the bottom screen, the
  // map and the buttons over it)
  enum : std::size_t {
    kProfileBottom,
    kProfileOut,
    kProfileDice,
    kProfileTop,
    kProfileWait,
    kProfilePieces,
    kProfileRenderer,
    kProfileOverlay,
    kProfileMap,
    kProfileButtons
  };
  ScratchPadd::FrameProfile profile_{"btm", "out", "dice", "top", "wait",
                                     "pcs", "3d",  "ovl",  "map", "btn"};
  std::string profileLine_;
  double startSeconds_ = 0; // (the start's work: starting())

  // Whether the top screen would look as it did when last drawn: a game's
  // board, nothing new from the table, nothing moving (the pieces, the
  // dice, the board coming down), the camera and the selection the same.
  // (The -watch builds redraw once a second, for their measurements.)
  // After buying, the two cards dealt (the user's): thrown onto the top
  // screen face down, one after the other, then turned over one after the
  // other (each narrowing to nothing, its back showing, then widening, its
  // front), their words on them once turned; the one chosen on the bottom
  // screen outlined. The pictures are the cards' textures, larger; the
  // words are the overlay's, sharp at any size (full-size textures of the
  // cards wouldn't fit beside the tile faces in the DS's texture memory)
  // A close-up's picture drawn into its texture (PlatformProfile::closeUpSize)
  void closeUpTile(CosmicHex::Art::Face face) {
    const int size = kThisPlatform.closeUpSize;
    const auto pixels =
        recipePixels(CosmicHex::Art::recipeOf(face), size, kThisPlatform.recipeSmoothing);
    closeUpFaceShows_ = face;
    if (pixels.empty() || !renderer_->replace(closeUpFace_, {size, size, pixels}))
      closeUpFaceShows_.reset(); // (the usual face, then)
  }
  void closeUpCards(const std::vector<CosmicHex::Art::Card> &cards) {
    const int size = kThisPlatform.closeUpSize, height = size * 52 / 63;
    closeUpCardsShow_ = cards;
    for (std::size_t i = 0; i < cards.size() && i < closeUpCards_.size(); ++i) {
      const auto whole = recipePixels(CosmicHex::Art::cardRecipeOf(cards[i]), size, size * 88 / 63,
                                      kThisPlatform.recipeSmoothing);
      std::vector<Scene::Colour> square(static_cast<std::size_t>(size * size));
      if (!whole.empty())
        std::copy(whole.begin(), whole.begin() + size * height, square.begin());
      if (whole.empty() || !renderer_->replace(closeUpCards_[i], {size, size, square}))
        closeUpCardsShow_.clear(); // (the usual pictures, then)
    }
  }

  // The discovery showing (Discovery): where it is, once the ship's slide
  // is done
  std::optional<Discovery::Moment> discoveryMoment() const {
    if (discoveries_.empty() || !discoveryAt_ || Clock::now() < *discoveryAt_)
      return std::nullopt;
    return Discovery::at(static_cast<float>(secondsSince(*discoveryAt_)));
  }
  // Whether a tile is turned over but not yet back in its place on the top
  // board: the one showing, until it's back down, or one waiting
  bool stillDiscovering(CosmicHex::Hex hex) const {
    for (std::size_t i = 0; i < discoveries_.size(); ++i) {
      if (discoveries_[i].hex != hex)
        continue;
      const auto moment = i == 0 ? discoveryMoment() : std::nullopt;
      return !moment || moment->down < 1;
    }
    return false;
  }
  // Whether it's off the board, flying (drawDiscovery draws it, not the
  // board)
  bool flying(CosmicHex::Hex hex) const {
    const auto moment = discoveryMoment();
    return moment && discoveries_.front().hex == hex && moment->down < 1;
  }
  bool wasShownBig(CosmicHex::Hex hex) const {
    return std::find(shownBig_.begin(), shownBig_.end(), hex) != shownBig_.end() ||
           (!discoveries_.empty() && discoveries_.front().hex == hex);
  }
  // Each frame: the game held while a discovery shows, let go after the
  // last; the bottom screen drawn again as its tile fades in
  void updateDiscovery() {
    if (discoveries_.empty() || !discoveryAt_ || Clock::now() < *discoveryAt_)
      return;
    // Its face drawn sharp first (about 45 ms on a DS at 64), while
    // nothing moves: the ship's slide is done, the lift not begun
    if (closeUpFaceShows_ != discoveries_.front().face) {
      closeUpTile(discoveries_.front().face);
      discoveryAt_ = Clock::now();
    }
    if (!heldForDiscovery_ && latest_ && !latest_->paused) {
      send(SetHold{true});
      heldForDiscovery_ = true;
    }
    const Discovery::Moment moment = *discoveryMoment();
    if (moment.fade > 0)
      bottomDirty_ = true;
    if (!moment.done)
      return;
    shownBig_.push_back(discoveries_.front().hex);
    discoveries_.erase(discoveries_.begin());
    if (!discoveries_.empty()) {
      discoveryAt_ = Clock::now();
      return;
    }
    discoveryAt_.reset();
    if (heldForDiscovery_) {
      heldForDiscovery_ = false;
      if (!heldForInfo_)
        send(SetHold{false});
    }
  }
  // The tile turned over, the board's own 3D tile: lifted off its place
  // and flown up to face you, face down; turned over; its number token
  // dropped on from above; held, its name and what it gives written above
  // and below; then flown back down to lie flat where it was
  void drawDiscovery(Scene::Overlay &overlay, const Scene::Camera &camera) {
    const auto moment = discoveryMoment();
    if (!moment)
      return;
    const Discovery::Found &found = discoveries_.front();
    const float k = static_cast<float>(kTopWidth) / 256;
    const auto screenW = static_cast<float>(kTopWidth), screenH = static_cast<float>(kTopHeight);
    const float lifted = moment->lifted();
    // A dim backdrop behind it while it faces you: in as it turns over,
    // gone early on its way back (the tile passes through its plane on the
    // way up and down, so not then)
    const float dim = moment->turn * std::max(0.0f, 1 - 4 * moment->down);
    if (dim > 0 && pictureSquare_)
      renderer_->draw(
          pictureSquare_,
          onScreenAt(camera, screenW, screenH, 0, 0, screenW, screenH, kPictureDistance * 1.2f),
          {.shading = Scene::Material::Shading::Unlit,
           .colour = {8, 12, 22},
           .alpha = static_cast<std::uint8_t>(24 * dim)});
    // On the board: where it lies, flat (its x, y and z the world's)
    const Scene::Vector3 onBoard = BoardPieces::centre(found.hex);
    const float radius = (BoardPieces::at({found.hex, 0}) - onBoard).length();
    // Facing you: in front of the camera, as tall as about 55% of the
    // screen, its face towards you, its top up the screen
    const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
    const Scene::Vector3 right = forward.cross(camera.up).normalised();
    const Scene::Vector3 screenUp = right.cross(forward);
    const float distance =
        2 * radius / (0.55f * 2 * std::tan(camera.fieldOfView * 3.14159265f / 360));
    const Scene::Vector3 facing = camera.eye + forward * distance;
    // Between: its position and axes carried from one to the other (the
    // axes made square again: Gram-Schmidt)
    const auto between = [lifted](const Scene::Vector3 &a, const Scene::Vector3 &b) {
      return a + (b - a) * lifted;
    };
    const Scene::Vector3 y = between({0, 1, 0}, forward * -1).normalised();
    Scene::Vector3 z = between({0, 0, 1}, screenUp * -1);
    z = (z - y * z.dot(y)).normalised();
    const Scene::Vector3 x = y.cross(z);
    const Scene::Transform pose = Scene::Transform::fromAxes(x, y, z, between(onBoard, facing));
    // Turning over about its x axis, as on the board: face down until half
    // way
    const float half = 1.5707963f, turn = moment->turn * 2 * half;
    const bool faceUp = turn > half;
    const auto spin = Scene::Transform::rotation({1, 0, 0}, faceUp ? turn - 2 * half : turn);
    const CosmicHex::Art::Face face = faceUp ? found.face : found.faceDown;
    const Scene::Texture texture = closeUpFaceShows_ == face && closeUpFace_
                                       ? closeUpFace_
                                       : faces_[static_cast<std::size_t>(face)];
    renderer_->draw(
        hexTile_, pose * spin,
        texture ? Scene::Material{.shading = Scene::Material::Shading::Toon, .texture = texture}
                : Scene::Material{.shading = Scene::Material::Shading::Toon,
                                  .colour = BoardColours::ofFace(face)});
    // Its number token, dropped on from above once it's face up (from up
    // the screen while it faces you), riding it back down
    const Scene::Texture chip = chips_[static_cast<std::size_t>(found.number)];
    if (found.number > 0 && moment->turn >= 1 && chip) {
      const float drop = (1 - moment->chip) * 2.5f * radius;
      renderer_->draw(numberChip_, pose * Scene::Transform::translation({0, 0, -drop}),
                      Scene::Material{.shading = Scene::Material::Shading::Toon, .texture = chip});
    }
    // Its words, while it faces you
    if (lifted > 0.98f) {
      overlay.textCentred(screenW / 2, 10 * k, names_[found.by] + " discovers",
                          BoardColours::ofPlayer(found.by), k);
      if (moment->turn >= 1) {
        overlay.textCentred(screenW / 2, screenH / 2 - 66 * k, found.name, Palette::kWords, 2 * k);
        overlay.textCentred(screenW / 2, screenH / 2 + 64 * k, found.gives, Palette::kSoftWords,
                            1.5f * k);
      }
    }
  }

  void drawDealtCards(Scene::Overlay &overlay, const Scene::Camera &camera) {
    if (!latest_ || !pictureSquare_)
      return;
    const auto choice = keepChoiceOf(latest_->state, latest_->moves);
    if (!choice)
      return;
    const float k = static_cast<float>(kTopWidth) / 256;
    const auto screenW = static_cast<float>(kTopWidth), screenH = static_cast<float>(kTopHeight);
    const float since = static_cast<float>(secondsSince(*keepDealtAt_));
    const float w = 100 * k, pictureH = w * 52 / 63, cardH = w * 88 / 63, gap = 16 * k;
    const auto count = static_cast<float>(choice->cards.size());
    const float left = (screenW - (count * w + (count - 1) * gap)) / 2, top = 8 * k;
    const int chosen = bottomScreen_ ? bottomScreen_->keepChosen() : -1;
    overlay.textCentred(screenW / 2, screenH - 6 * k, "Keep one: touch it below",
                        Palette::kDimWords, k);
    for (std::size_t i = 0; i < choice->cards.size(); ++i) {
      const auto n = static_cast<float>(i);
      // Thrown in from the bottom left, eased to its place
      const float flown = ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::EaseOut,
                                                     (since - 0.15f * n) / kSecondsCardThrow);
      const float homeX = left + n * (w + gap);
      const float x = -w + (homeX + w) * flown, y = screenH + (top - screenH) * flown;
      // Then turned over: its width through |cos|, the back until halfway
      const float turned =
          std::clamp((since - kSecondsCardThrow - 0.2f - 0.35f * n) / kSecondsCardTurn, 0.0f, 1.0f);
      const float across = std::abs(std::cos(turned * 3.14159265f));
      const bool front = turned >= 0.5f;
      if (!front) {
        renderer_->draw(
            backSquare_,
            onScreenAt(camera, screenW, screenH, x + w * (1 - across) / 2, y, w * across, cardH),
            {.shading = Scene::Material::Shading::Unlit,
             .texture = backPictures_[static_cast<std::size_t>(CosmicHex::Art::kActionBack)]});
        continue;
      }
      const auto card = choice->cards[i];
      const float cw = w * across, cx = x + w * (1 - across) / 2;
      const bool sharp =
          closeUpCardsShow_ == choice->cards && i < closeUpCards_.size() && closeUpCardSquare_;
      renderer_->draw(
          sharp ? closeUpCardSquare_ : pictureSquare_,
          onScreenAt(camera, screenW, screenH, cx, y, cw, pictureH),
          {.shading = Scene::Material::Shading::Unlit,
           .texture = sharp ? closeUpCards_[i] : cardPictures_[static_cast<std::size_t>(card)]});
      // The card's body under its picture, and once flat, its words
      overlay.rectangle(cx, y + pictureH, cw, cardH - pictureH, {236, 232, 220, 255});
      if (turned >= 1) {
        const CosmicHex::Art::CardWords words = CosmicHex::Art::wordsOf(card);
        // (the title white, centred on the label band, 3 to 19 of the
        // card's 88 units, as large as fits its width: the user's)
        float titleScale = 2 * k;
        while (titleScale > k &&
               Scene::Overlay::textWidth(words.shortTitle, titleScale) > w * 54 / 63)
          titleScale -= 0.25f * k;
        overlay.textCentred(x + w / 2, y + w * 11 / 63, words.shortTitle, Palette::kWords,
                            titleScale);
        // (wrapped by the width the letters take as drawn, spacing included)
        const auto lines = Scene::wrapText(words.words, w - 8 * k, [k](std::string_view line) {
          return Scene::Overlay::textWidth(line, k);
        });
        for (std::size_t l = 0; l < lines.size() && l < 6; ++l)
          overlay.text(x + 4 * k, y + pictureH + (3 + 9 * static_cast<float>(l)) * k, lines[l],
                       {40, 40, 48}, k);
        if (static_cast<int>(i) == chosen) {
          overlay.rectangle(x - 3 * k, y - 3 * k, w + 6 * k, 2 * k, Palette::kHighlight);
          overlay.rectangle(x - 3 * k, y + cardH + k, w + 6 * k, 2 * k, Palette::kHighlight);
          overlay.rectangle(x - 3 * k, y - 3 * k, 2 * k, cardH + 6 * k, Palette::kHighlight);
          overlay.rectangle(x + w + k, y - 3 * k, 2 * k, cardH + 6 * k, Palette::kHighlight);
        }
      }
    }
  }

  // Another player's view on the top screen (the other players' tabs):
  // open, or still sliding back up
  bool showingPlayer() {
    if (shownInfo_ == BottomScreen::Info::Player) {
      shownPlayer_ = bottomScreen_->infoPlayer();
      return true;
    }
    if (!infoClosingAt_ || closingInfo_ != BottomScreen::Info::Player)
      return false;
    if (std::chrono::duration<float>(Clock::now() - *infoClosingAt_).count() > kSecondsPanel) {
      infoClosingAt_.reset();
      return false;
    }
    return true;
  }

  // What everyone can see of a player (OtherPlayers.hpp), over the board,
  // sliding down from the top (and back up): their name and points, their
  // cards face down, badges, ships, stations and colonies, the planets
  // their stations touch with each one's number, their revealed Charters
  void drawOtherPlayer(Scene::Overlay &overlay, const Scene::Camera &camera) {
    if (!latest_ || shownPlayer_ < 0)
      return;
    const PlayerView view = viewOf(latest_->state, shownPlayer_);
    const float k = static_cast<float>(kTopWidth) / 256;
    const Clock::time_point from = infoClosingAt_ ? *infoClosingAt_ : infoOpenedAt_;
    const float moved = ScratchPadd::Motion::along(
        ScratchPadd::Motion::Curve::EaseInOut,
        std::chrono::duration<float>(Clock::now() - from).count() / kSecondsPanel);
    const float down = infoClosingAt_ ? 1 - moved : moved;
    const float top = -static_cast<float>(kTopHeight) * (1 - down);
    const Scene::Colour colour = BoardColours::ofPlayer(view.player);
    const auto screenW = static_cast<float>(kTopWidth), screenH = static_cast<float>(kTopHeight);
    // The panel: a dark, slightly see-through backdrop in 3D, behind the
    // cards' pictures (the overlay's shapes would cover them)
    renderer_->draw(
        pictureSquare_,
        onScreenAt(camera, screenW, screenH, 0, top, screenW, screenH, kPictureDistance * 1.2f),
        {.shading = Scene::Material::Shading::Unlit, .colour = {12, 18, 32}, .alpha = 29});
    overlay.rectangle(3 * k, top + 3 * k, kTopWidth - 6 * k, 1, colour);
    overlay.rectangle(3 * k, top + kTopHeight - 4 * k, kTopWidth - 6 * k, 1, colour);
    // Name and points
    overlay.text(10 * k, top + 10 * k, names_.shortOf(view.player), colour, 1.5f * k);
    overlay.text(10 * k, top + 32 * k,
                 std::to_string(view.points) + (view.points == 1 ? " point" : " points"),
                 Palette::kWords, k);
    // Their cards, face down, in the agreed backs: fanned, six a row, a
    // second row under the first when there are more (up to 12 shown), the
    // count by the label. What's under them moves down to make room
    constexpr int kBacksARow = 6, kBacksShown = 12;
    const float backW = 28 * k, backH = backW * 88 / 63, backStep = 9 * k, rowStep = 18 * k;
    const auto rowsOf = [&](int count) {
      return std::max(1, (std::min(count, kBacksShown) + kBacksARow - 1) / kBacksARow);
    };
    const int rows = std::max(rowsOf(view.resourceCards), rowsOf(view.actionCards));
    const float extra = backH - 34 * k + rowStep * static_cast<float>(rows - 1);
    const auto backs = [&](float x, float y, int count, std::string_view label,
                           CosmicHex::Art::CardBack design) {
      overlay.text(x, y, std::string(label) + " " + std::to_string(count), Palette::kDimWords, k);
      const int shown = std::min(count, kBacksShown);
      for (int i = 0; i < shown; ++i) {
        const auto column = static_cast<float>(i % kBacksARow);
        const auto row = static_cast<float>(i / kBacksARow);
        if (backSquare_ && backPictures_[static_cast<std::size_t>(design)])
          renderer_->draw(backSquare_,
                          onScreenAt(camera, screenW, screenH, x + column * backStep,
                                     y + 11 * k + row * rowStep, backW, backH,
                                     kPictureDistance * (1 - 0.002f * static_cast<float>(i))),
                          {.shading = Scene::Material::Shading::Unlit,
                           .texture = backPictures_[static_cast<std::size_t>(design)]});
      }
    };
    backs(84 * k, top + 10 * k, view.resourceCards, "RESOURCES", CosmicHex::Art::kResourceBack);
    backs(170 * k, top + 10 * k, view.actionCards, "CARDS", CosmicHex::Art::kActionBack);
    // Badges, ships, stations and colonies as the board draws them (the
    // user's): each a group of its pieces, up to four of them, or one and
    // how many; its name small under it
    const auto pieces = [&](float x, int count, std::string_view name, auto &&drawOne) {
      const float y = top + 66 * k + extra;
      constexpr int kMostDrawn = 4;
      const int drawn = count > kMostDrawn ? 1 : count;
      for (int i = 0; i < drawn; ++i)
        drawOne(x + 5 * k + static_cast<float>(i) * 11 * k, y);
      if (count > kMostDrawn)
        overlay.text(x + 12 * k, y - 4 * k, "x" + std::to_string(count), Palette::kWords, k);
      if (count == 0)
        overlay.text(x + 1 * k, y - 4 * k, "none", Palette::kDimWords, k);
      overlay.text(x, y + 8 * k, name, Palette::kDimWords, k);
    };
    pieces(10 * k, view.badges, "badges",
           [&](float x, float y) { overlay.hexagon(x, y, 4.5f * k, Palette::kHighlight, true); });
    pieces(72 * k, view.ships, "ships", [&](float x, float y) {
      overlay.triangle(x, y - 5 * k, x - 4.5f * k, y + 4 * k, x + 4.5f * k, y + 4 * k, colour);
    });
    pieces(134 * k, view.stations, "stations", [&](float x, float y) {
      overlay.rectangle(x - 3.5f * k, y - 3.5f * k, 7 * k, 7 * k, colour);
    });
    pieces(196 * k, view.colonies, "colonies", [&](float x, float y) {
      overlay.rectangle(x - 4.5f * k, y - 4.5f * k, 9 * k, 9 * k, colour);
      overlay.rectangle(x - 1.5f * k, y - 1.5f * k, 3 * k, 3 * k, {20, 22, 30});
    });
    // Their planets: a hexagon each in its kind's colour, its number on it
    overlay.text(10 * k, top + 88 * k + extra, "PLANETS", Palette::kDimWords, k);
    if (view.planets.empty())
      overlay.text(64 * k, top + 88 * k + extra, "none yet", Palette::kDimWords, k);
    for (std::size_t i = 0; i < view.planets.size() && i < 16; ++i) {
      const float x = (22 + 26 * static_cast<float>(i % 8)) * k;
      const float y = top + (110 + 25 * static_cast<float>(i / 8)) * k + extra;
      const PlayerView::Planet &planet = view.planets[i];
      overlay.hexagon(x, y, 12 * k, BoardColours::ofKind(planet.kind), true);
      if (planet.colony)
        overlay.hexagon(x, y, 8 * k, colour, true);
      overlay.textCentred(x, y, std::to_string(planet.number), {20, 22, 30}, k);
    }
    // Their revealed Charters
    if (!view.charters.empty()) {
      std::string charters = "Charters:";
      for (const CosmicHex::Art::Card card : view.charters)
        charters += " " + std::string(CosmicHex::Art::wordsOf(card).shortTitle);
      overlay.text(10 * k, std::min(top + 156 * k + extra, top + kTopHeight - 14 * k), charters,
                   Palette::kWords, k);
    }
  }

  // The start's banner: what's being placed and by whom
  void startPlacingBanner(const State &state) {
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

  // Who goes first, over the board at the start (each player's die, those
  // tied rolling again), for firstRollSeconds or until A; then the game
  // goes on
  void endFirstRoll() {
    firstRollShownAt_.reset();
    if (flow_.screen() == ScreenFlow::Screen::Game)
      send(SetHold{false});
    if (latest_)
      startPlacingBanner(latest_->state);
  }
  // How long who goes first shows: each round's dice tumbling then
  // settling, then time to read the order
  static double firstRollSeconds(const CosmicHex::Handheld::FirstRoll &roll) {
    return kSecondsARollRound * static_cast<double>(std::min<std::size_t>(roll.rounds.size(), 3)) +
           kSecondsFirstRollRead;
  }
  // A square turned by `angle` (radians) about its middle: two triangles
  // (the overlay's rectangles don't turn)
  static void turnedSquare(Scene::Overlay &overlay, float x, float y, float half, float angle,
                           Scene::Colour colour) {
    std::array<std::array<float, 2>, 4> c{};
    for (int i = 0; i < 4; ++i) {
      const float a = angle + 0.7853982f + static_cast<float>(i) * 1.5707963f;
      c[static_cast<std::size_t>(i)] = {x + half * 1.4142136f * std::cos(a),
                                        y + half * 1.4142136f * std::sin(a)};
    }
    overlay.triangle(c[0][0], c[0][1], c[1][0], c[1][1], c[2][0], c[2][1], colour);
    overlay.triangle(c[0][0], c[0][1], c[2][0], c[2][1], c[3][0], c[3][1], colour);
  }
  // Who goes first, the user's way: every player's die thrown at once in
  // their colour, tumbling (spinning, its number flickering) as it drops
  // in, then settling; those tied for the highest throw again on the row
  // below; then the order, by name
  void drawFirstRoll(Scene::Overlay &overlay) {
    if (!latest_ || latest_->firstRoll.rounds.empty() || !firstRollShownAt_)
      return;
    const CosmicHex::Handheld::FirstRoll &roll = latest_->firstRoll;
    const float k = static_cast<float>(kTopWidth) / 256;
    const auto since = static_cast<float>(secondsSince(*firstRollShownAt_));
    overlay.rectangle(0, 0, kTopWidth, kTopHeight, {10, 15, 26, 240});
    overlay.textCentred(kTopWidth / 2.0f, 14 * k, "WHO GOES FIRST", Palette::kWords, 2 * k);
    const int players = static_cast<int>(roll.rounds.front().size());
    const float gap = 58 * k;
    const float left = kTopWidth / 2.0f - gap * static_cast<float>(players - 1) / 2;
    for (int p = 0; p < players; ++p)
      overlay.textCentred(left + gap * static_cast<float>(p), 32 * k, names_.shortOf(p),
                          Palette::player(p), k);
    const std::size_t rounds = std::min<std::size_t>(roll.rounds.size(), 3);
    float y = 54 * k;
    for (std::size_t r = 0; r < rounds; ++r) {
      const float startsAt = kSecondsARollRound * static_cast<float>(r);
      if (since < startsAt)
        break;
      const float t = std::min(1.0f, (since - startsAt) / kSecondsDiceTumble);
      const float half = (r == 0 ? 14 : 10) * k;
      if (r > 0)
        overlay.textCentred(kTopWidth / 2.0f, y - half - 7 * k, "tied: they throw again",
                            Palette::kHighlight, k);
      for (int p = 0; p < players; ++p) {
        const int face = roll.rounds[r][static_cast<std::size_t>(p)];
        if (face == 0)
          continue;
        const float x = left + gap * static_cast<float>(p);
        // Dropping in from above and settling (Settle: a small bounce),
        // turning less and less, its number flickering until it lands
        const float landed = ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::Settle, t);
        const float dy = (1 - landed) * -24 * k; // (from just under the title)
        const float angle = (1 - t) * (1 - t) * (5.0f + static_cast<float>(p));
        const bool settled = t >= 1;
        const bool winner =
            settled && r + 1 == rounds && p == roll.first &&
            since >= kSecondsARollRound * static_cast<float>(rounds - 1) + kSecondsDiceTumble;
        turnedSquare(overlay, x, y + dy, half + 2 * k, angle,
                     winner ? Palette::kHighlight : Palette::player(p));
        turnedSquare(overlay, x, y + dy, half, angle, {242, 242, 242, 255});
        const int shown =
            settled ? face : 1 + (p * 7 + static_cast<int>(since * 14)) % 6; // (flickering)
        overlay.textCentred(x, y + dy, std::to_string(shown), {30, 30, 36},
                            (r == 0 ? 2 : 1.5f) * k);
      }
      y += 2 * half + 22 * k;
    }
    // Once the last throw has settled: the order, by name
    if (since < kSecondsARollRound * static_cast<float>(rounds - 1) + kSecondsDiceTumble + 0.3f)
      return;
    std::string homes = "Homes: ", ships = "Ships: ";
    for (int i = 0; i < players; ++i) {
      homes += (i ? ", " : "") + names_[(roll.first + i) % players];
      ships += (i ? ", " : "") + names_[(roll.first + players - 1 - i) % players];
    }
    const float bottom = static_cast<float>(kTopHeight) - 44 * k;
    overlay.textCentred(kTopWidth / 2.0f, bottom, names_[roll.first] + " goes first",
                        Palette::player(roll.first), 1.5f * k);
    overlay.textCentred(kTopWidth / 2.0f, bottom + 16 * k, homes, Palette::kWords, k);
    overlay.textCentred(kTopWidth / 2.0f, bottom + 28 * k, ships + " (back again)",
                        Palette::kSoftWords, k);
  }

  // Whether anything on the top screen moves now
  bool topMoving() const {
    if (flow_.screen() != ScreenFlow::Screen::Game || !latest_ || descentStarted_ ||
        dice_ != DiceStage::Idle || firstRollShownAt_)
      return true;
    if (gameClock_.seconds() - motionStarted_ < kSecondsAnimating + kSecondsChipDrop)
      return true; // (pieces still moving, or a number chip dropping)
    if (banner_.showing(gameClock_.seconds()))
      return true; // (the turn's banner moving)
    if (shownInfo_ != BottomScreen::Info::None || infoClosingAt_ || keepDealtAt_ ||
        outlineMovedAt_ || discoveryMoment())
      return true; // (an info view, dealt cards, a discovery: they come in or go)
    return false;
  }
  bool topIsStill() const {
    // (after something moved, one frame more: its last drawn frame may
    // still show a trace of it, a banner's band fading, say)
    if (topMoving() || movedWhenDrawn_)
      return false;
    if (snapshots_ != drawnSnapshots_ || turn_ != drawnTurn_ || selected_ != drawnSelection_)
      return false;
    return !kWatchAtStart || Clock::now() - drawnAt_ < std::chrono::seconds(1);
  }

  // How far the title has faded in from black, 0 to 1, over
  // kSecondsTitleFadeIn from when the start's work was done
  float titleFadedIn() const {
    return std::min(1.0f, static_cast<float>(secondsSince(titleStarted_) / kSecondsTitleFadeIn));
  }

  bool onAMenuScreen() const {
    const ScreenFlow::Screen screen = flow_.screen();
    return screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Setup;
  }

  void showProfile() {
    if (!kWatchAtStart)
      return;
    if (profile_.secondDone()) {
      char start[24];
      std::snprintf(start, sizeof start, "start %.1fs ", startSeconds_);
      profileLine_ = start + profile_.line();
      spdlog::info("frames: {}", profileLine_);
    }
    // (not over a view that fills the screen: its words would be lost)
    if (firstRollShownAt_ || shownInfo_ == BottomScreen::Info::Player || keepDealtAt_ ||
        discoveryMoment())
      return;
    // (three lines where the screen's narrow)
    for (std::size_t from = 0, row = 0; from < profileLine_.size() && row < 4;
         from += kCharsPerLine, ++row)
      renderer_->overlay().text(4, kTopHeight - 110 + 12 * static_cast<float>(row),
                                profileLine_.substr(from, kCharsPerLine), {240, 200, 120});
  }
  // The title scene
  std::optional<ScratchPadd::TitleScene::TitleScene> title_;
  Clock::time_point titleStarted_;
  // The physics dice
  enum class DiceStage : std::uint8_t { Idle, Rolling, Showing, Pulling };
  static constexpr double kSecondsShowingDice = 0.75;
  Dice::DiceTray tray_;
  Scene::Mesh die_;
  DiceStage dice_ = DiceStage::Idle;
  std::uint64_t thrownSeed_ = 0;
  double shownSeconds_ = 0, diceOwed_ = 0;
  std::pair<int, int> rolled_{};           // (what they settled on, shown over them)
  double lastDiceTime_ = 0, thrownAt_ = 0; // (game seconds)
  static constexpr double kMostSecondsRolling = 8;
  std::optional<TableSnapshot> latest_;
  BoardMotion motion_;
  // The game's own time (Gregory 8.4): moves animate and dice roll on it,
  // and it stops while a watched game is paused
  ScratchPadd::GameClock gameClock_;
  double motionStarted_ = 0;
  Input::Gamepad lastPad_;
  float turn_ = 0;
  // The bottom board, its touch and what's selected on it
  std::optional<BoardPicture> picture_;
  std::optional<BottomScreen> bottomScreen_; // (its buttons and choosing screens)
  std::optional<Canvas> bottomCanvas_;
  std::optional<Clock::time_point> feedbackSince_;
  bool pressDrawn_ = false;                    // (a button's press glow drawn last frame)
  bool rollAsked_ = false;                     // (you've said to throw the dice: Roll or A)
  Clock::time_point startedAt_ = Clock::now(); // (the scripted taps' times)
  ScratchPadd::Banner banner_{static_cast<float>(kTopWidth)};
  // The info views (Show resources): which is shown, since when, and
  // whether it holds the game; the cards' pictures for them
  BottomScreen::Info shownInfo_ = BottomScreen::Info::None;
  Clock::time_point infoOpenedAt_{};
  std::optional<Clock::time_point> infoClosingAt_;            // (an info view going off)
  BottomScreen::Info closingInfo_ = BottomScreen::Info::None; // (which)
  int shownPlayer_ = -1; // (the other players' view: whose, kept while it goes off)
  std::optional<Clock::time_point> keepDealtAt_;    // (your two cards dealt: since)
  std::optional<Clock::time_point> outlineMovedAt_; // (the turn's outline moving: since)
  int outlineFrom_ = 0;                             // (from this player's row)
  bool heldForInfo_ = false;
  // Tiles turned over, still to show big (Discovery), the first showing
  // since discoveryAt_ (real time: the game's held meanwhile)
  std::vector<Discovery::Found> discoveries_;
  std::optional<Clock::time_point> discoveryAt_;
  bool heldForDiscovery_ = false;
  bool movedWhenDrawn_ = true; // (the top screen's last frame drawn while something moved)
  bool youPaused_ = false;
  // Who's who: your name (the console's owner's), the bots' (PlayerNames),
  // made as each game starts
  std::string ownerName_ = ScratchPadd::OwnerName::get();
  PlayerNames names_;                    // (watching: paused with A, so the PAUSED band shows)
  std::vector<CosmicHex::Hex> shownBig_; // (shown big since the last move: no flip)
  // Close-ups (PlatformProfile::closeUpSize): a discovered tile's face and
  // the two cards dealt, drawn sharp when they're shown, into textures
  // loaded once and reused
  Scene::Texture closeUpFace_;
  std::optional<CosmicHex::Art::Face> closeUpFaceShows_;
  std::array<Scene::Texture, 2> closeUpCards_{};
  std::vector<CosmicHex::Art::Card> closeUpCardsShow_;
  Scene::Mesh closeUpCardSquare_;
  std::array<Scene::Texture, CosmicHex::Art::kCards> cardPictures_{};
  std::array<Scene::Texture, CosmicHex::kResources> resourcePictures_{};
  std::array<Scene::Colour, CosmicHex::kResources> resourceBands_{};
  Scene::Mesh pictureSquare_, resourceSquare_, backSquare_;
  std::array<Scene::Texture, 2> backPictures_{}; // (by Art::CardBack)
  int cardWidth_ = 32, resourceWidth_ = 44;
  static constexpr float kPileStep = 5; // (pixels between a pile's cards: their bands show)
  std::optional<ConsoleScreens::BottomScreen> bottom_;
  bool bottomDirty_ = true, touchWasDown_ = false, dragged_ = false;
  std::array<int, 2> touchStart_{}, touchLast_{};
  std::optional<BoardPicture::Spot> selected_;
  // What the top screen last showed (topIsStill): the table's snapshots
  // drawn, the camera's turn and the selected spot, and when
  std::uint64_t snapshots_ = 0, drawnSnapshots_ = 0;
  float drawnTurn_ = 0;
  std::optional<BoardPicture::Spot> drawnSelection_;
  Clock::time_point drawnAt_{};
  // Which screen (ScreenFlow); in a game, watching or playing
  ScreenFlow flow_;
  enum class Mode : std::uint8_t { Watching, Playing };
  Mode mode_ = Mode::Watching;
  bool quitting_ = false;
  // The title's and the pause menu's buttons, and the setup's menu
  ButtonMenu titleMenu_{{"Play", "Quit"}};
  ButtonMenu pauseMenu_{{"Resume", "Restart", "Title", "Quit"}};
  StartMenu menu_;
  MenuPicture menuPicture_{kBottomWidth, kBottomHeight};
  // The board the setup has chosen, as a game would start on it (shown on
  // the top screen, turning slowly)
  State preview_ = previewOf(menu_);
  State previewBefore_ = preview_; // (the board sliding out)
  // The start of a game: the board coming down, the bottom screen fading
  std::optional<Clock::time_point> descentStarted_;
  bool fadeFromMenu_ = false; // (the bottom screen fading from the setup's menu)
  std::optional<Clock::time_point> firstRollShownAt_; // (who goes first, on the screen since)
  // What the buttons cover on the map, as last drawn; and whether the
  // bottom screen shows the map as last drawn (so only what changes needs
  // copying to it)
  std::vector<BoardPicture::Area> overBoardAreas_;
  bool screenShowsMap_ = false;
  bool fading_ = false; // (mid-fade: the map already drawn)
  bool sliding_ = false;
  int slideWay_ = 1; // (+1: right was pressed, the new board comes in from the right)
  Clock::time_point slideStarted_;
  // The gaps' meshes for the last two boards drawn: while the setup's
  // boards slide, both show at once (the user's: the old one's
  // outlines went missing during the slide), each with the tiles it's for
  struct BoardGaps {
    std::vector<CosmicHex::Hex> tiles;
    Scene::Mesh mesh;
  };
  std::array<BoardGaps, 2> gaps_;
  std::size_t olderGaps_ = 0; // (which of gaps_ goes next)

  static State previewOf(const StartMenu &menu) {
    TableSettings settings{.players = menu.players(), .board = menu.boardItself()};
    return CosmicHex::newGame(settings.players, rulesFor(settings));
  }
  // The preview, made again; a slide when its board changed (the old one
  // out, the new one in, the way the arrow pointed: slideWay_)
  void updatePreview() {
    State next = previewOf(menu_);
    const bool changed = next.settings.homeHexes != preview_.settings.homeHexes ||
                         next.settings.planetHexes != preview_.settings.planetHexes;
    if (changed && flow_.screen() == ScreenFlow::Screen::Setup) {
      previewBefore_ = preview_;
      sliding_ = true;
      slideStarted_ = Clock::now();
    }
    preview_ = std::move(next);
  }

  // The gaps between a state's tiles, made again when its tiles aren't the
  // ones they were made for
  Scene::Mesh gapsFor(const State &state) {
    std::vector<CosmicHex::Hex> tiles;
    for (const CosmicHex::Planet &planet : state.planets)
      tiles.push_back(planet.hex);
    for (const CosmicHex::ZoneSite &site : state.zone)
      tiles.push_back(site.hex);
    for (const BoardGaps &kept : gaps_)
      if (kept.mesh && kept.tiles == tiles)
        return kept.mesh;
    // (in place of the older of the two)
    BoardGaps &replaced = gaps_[olderGaps_];
    olderGaps_ = 1 - olderGaps_;
    if (replaced.mesh)
      renderer_->unload(replaced.mesh);
    replaced.mesh = renderer_->load(BoardPieces::gapsBetween(tiles).data());
    replaced.tiles = std::move(tiles);
    return replaced.mesh;
  }

  // A button on the title (Play, Quit) or the pause menu (Resume, Restart,
  // Title, Quit) chosen
  void choose(std::size_t button) {
    using Event = ScreenFlow::Event;
    if (flow_.screen() == ScreenFlow::Screen::Title) {
      apply(flow_.on(button == 0 ? Event::Play : Event::Quit));
      updatePreview();
      return;
    }
    static constexpr std::array<Event, 4> kPause{Event::Resume, Event::Restart, Event::ToTitle,
                                                 Event::Quit};
    if (button < kPause.size())
      apply(flow_.on(kPause[button]));
  }

  // The setup's menu: Start or Back
  void setupOutcome(StartMenu::Outcome outcome) {
    if (outcome == StartMenu::Outcome::Start)
      apply(flow_.on(ScreenFlow::Event::Start));
    else if (outcome == StartMenu::Outcome::Back)
      apply(flow_.on(ScreenFlow::Event::Back));
  }

  // What a step of the flow asks for: stop the game, start one (as the setup
  // has it), hold or release it, or end the app
  void apply(const ScreenFlow::Step &step) {
    if (step.changed)
      bottomDirty_ = true;
    if (step.quit)
      quitting_ = true;
    if (step.stopGame) {
      send(StopGame{});
      latest_.reset();
      motion_ = BoardMotion();
      dice_ = DiceStage::Idle;
      selected_.reset();
      cursor_ = 0;
    }
    if (step.startGame)
      startGame();
    if (step.holdChanged)
      send(SetHold{step.holdGame});
    if (step.changed)
      pauseMenu_.setFocus(0);
  }

  void startGame() {
    // (from the setup: the board comes down to the game's view, and the
    // bottom screen fades from the menu to the map)
    if (flow_.screen() == ScreenFlow::Screen::Game) {
      descentStarted_ = Clock::now();
      fadeFromMenu_ = true;
      fading_ = false; // (the map drawn once, at the fade's first frame)
    }
    const bool play = !menu_.watching();
    mode_ = play ? Mode::Playing : Mode::Watching;
    latest_.reset();
    dice_ = DiceStage::Idle;
    bottomDirty_ = true;
    youPaused_ = false;
    names_ =
        PlayerNames::forGame(menu_.players(), play ? 0 : -1, ownerName_,
                             static_cast<std::uint64_t>(Clock::now().time_since_epoch().count()));
    if (bottomScreen_)
      bottomScreen_->setNames(names_);
    send(StartGame{.withPerson = play,
                   .players = menu_.players(),
                   .board = menu_.boardItself(),
                   .finishRound = menu_.finishRound(),
                   .seatBonus = menu_.seatBonus(),
                   .held = descentStarted_.has_value()});
  }
  std::size_t cursor_ = 0;
};

} // namespace

int main() {
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
#if defined(COSMIC_HEX_BENCH) && defined(__NDS__)
  // The -bench build: its timings on the bottom screen, nothing else
  // running, then wait to be switched off
  consoleDemoInit();
  CosmicHexBench::run();
  while (ConsoleScreens::keepRunning())
    ConsoleScreens::waitForTheRefresh();
  return 0;
#endif
  // (black until the splash fades in: the setup below shows nothing)
  ScratchPadd::StartupSplash::darkenScreens();
  const ConsoleScreens::Graphics graphics; // (before ScreenPadd's Display)
#if defined(SAKBOX_DEBUG)
  ScratchPadd::DebugLog::Session log; // a debug build's log, over Wi-Fi
#endif
  {
    ScratchPadd::SystemImplementation<ScratchPadd::AboutPadd, ScratchPadd::Input::GamePadd,
                                      ScreenPadd, TablePadd, ScratchPadd::Storage::StoragePadd>
        system;
    system.instantiate();
    system.start(); // returns when ScreenPadd stops it (START)
  }
  ScratchPadd::AppEnded::show(); // ("<its title> has ended.")
  return 0;
}
