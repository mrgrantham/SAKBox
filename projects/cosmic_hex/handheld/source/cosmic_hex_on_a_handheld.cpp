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
#include <CosmicHex/Handheld/ScorePops.hpp>
#include <CosmicHex/Handheld/ScreenFlow.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>
#include <CosmicHex/Handheld/SettingsScreen.hpp>
#include <CosmicHex/Handheld/StartMenu.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>
#include <CosmicHex/Handheld/TopShows.hpp>
#include <CosmicHex/Handheld/TopViews.hpp>
#include <CosmicHex/Handheld/WinCelebration.hpp>
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
#include <ScratchPadd/Storage/Files.hpp>
#include <ScratchPadd/Storage/Folders.hpp>
#include <ScratchPadd/Storage/Messages.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <spdlog/spdlog.h>

#include <ScratchPadd/Banner/Banner.hpp>
#include <ScratchPadd/ConsoleScreens/ConsoleScreens.hpp>
#include <ScratchPadd/OwnerName/OwnerName.hpp>
#include <ScratchPadd/PictureSlot/PictureSlot.hpp>
#include <ScratchPadd/Scene/TextWrap.hpp>
#include <ScratchPadd/StartupSplash/StartupSplash.hpp>
#include <ScratchPadd/StillFrames/StillFrames.hpp>

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
// (0.6 s, the user's: at 0.35 s, a row's 5 pixels, as the turn's banner
// came in, it read as a jump)
constexpr float kSecondsOutline = 0.6f;
// How long the other players' view takes to slide down (or up)
// (long enough to see it come: at 0.3 s a DS's 20 or so frames a second
// showed it in four or five jumps)
constexpr float kSecondsPanel = 0.45f;
// a little after the last
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
// (a ship's flight: long enough to see its engine burn, within a move's
// time on screen)
constexpr double kSecondsAnimating = 0.5;
// A planet's number chip, once it's turned over, drops this far onto it
// and settles in this long
constexpr double kSecondsChipDrop = 0.35;
constexpr float kChipDropHeight = 1.2f;
// How long the board moves after a move: the pieces, a chip dropping, a
// building's dust fading
constexpr double kSecondsBoardMoving =
    std::max(kSecondsAnimating + kSecondsChipDrop, BoardMotion::kLandingEnds *kSecondsAnimating);

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
  // The moves made since the last snapshot, as made (the home die's face)
  std::vector<CosmicHex::Handheld::TableGame::Played> played;
  int botGames = 0;                // (the last bot move's simulated games, for measuring)
  GameBots::SearchTimes botSearch; // (and where its time went)
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
      playedSent_ = 0;
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
  // The table as it is, to the screen. Made in the message itself
  // (sendMade): a snapshot holds the whole game, over a kilobyte, and its
  // copies on this thread's stack overflowed it on a DS (the game froze)
  void sendSnapshot() {
    const auto &history = game_->history();
    const std::size_t shown = std::min<std::size_t>(history.size(), 8);
    const auto &played = game_->played();
    const std::size_t from = std::min(playedSent_, played.size());
    playedSent_ = played.size();
    const bool dice = game_->waitingForDice(), person = game_->waitingForPerson();
    sendMade<TableSnapshot>([&] {
      std::vector<CosmicHex::Action> moves;
      std::vector<std::string> moveText;
      if (person) {
        moves = game_->personMoves();
        for (const CosmicHex::Action &move : moves)
          moveText.push_back(CosmicHex::describe(game_->state(), move));
      }
      return TableSnapshot{.state = game_->state(),
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
                           .firstRoll = game_->firstRoll(),
                           // (the moves since the last snapshot)
                           .played = std::vector<CosmicHex::Handheld::TableGame::Played>(
                               played.begin() + static_cast<long>(from), played.end()),
                           .botGames = game_->lastBotIterations(),
                           .botSearch = game_->lastSearch()};
    });
  }

  std::optional<TableGame> game_;
  std::size_t playedSent_ = 0; // (moves already sent in a snapshot)
  std::uint64_t seed_ = 0;
  bool paused_ = false;
  double lastThinkSeconds_ = 0;
};

class ScreenPadd : public ScratchPadd::Base {
public:
  ScreenPadd(ScratchPadd::System *system) : Base(system) {
    paceAnimations();
    receive<TableSnapshot>([this](const TableSnapshot &snapshot) {
      // What moved since the last, animated from now
      // (only when something moves: a snapshot that moves nothing, a hold
      // or a choice, leaves what's moving going)
      bool moved = false;
      if (latest_) {
        if (BoardMotion next(latest_->state, snapshot.state, &motion_); next.anything()) {
          motion_ = std::move(next);
          moved = true;
        }
        // Tiles turned over: each shown big on the top screen, one after
        // another, once the ship's slide is done (DiscoveryShow)
        discovery_.arrived(latest_->state, snapshot.state, showsNow() + kSecondsAnimating);
      }
      // A new turn (after the setup's placing): its banner
      const CosmicHex::Phase phase = snapshot.state.phase;
      const bool placing =
          phase == CosmicHex::Phase::PlaceColony || phase == CosmicHex::Phase::PlaceShip;
      // (the start's placing: whose it is and what, once who goes first is
      // shown)
      if (placing && !firstRoll_.on() && !descentStarted_ &&
          (!latest_ || snapshot.state.turn != latest_->state.turn || phase != latest_->state.phase))
        startPlacingBanner(snapshot.state);
      if (latest_ && snapshot.state.turn != latest_->state.turn && !placing &&
          snapshot.state.winner < 0) {
        const int turn = snapshot.state.turn;
        const bool yours = mode_ == Mode::Playing && turn == 0;
        banner_.start(gameClock_.seconds(), yours ? "YOUR TURN" : names_.upperOf(turn) + "'S TURN",
                      yours ? "roll the dice" : "thinking...", BoardColours::ofPlayer(turn));
      }
      // A home's pay on its die: after the turn's banner (HomePayShow)
      homePay_.saw(snapshot.played, showsNow() + banner_.remaining(gameClock_.seconds()));
      // A roll's pay: once the dice are pulled away (RollPayShow)
      if (latest_)
        rollPay_.saw(snapshot.played, latest_->state);
      // An action card played, or a Charter revealed: shown big (CardPlayShow)
      cardPlay_.saw(snapshot.played, showsNow());
      // Your two cards dealt after buying: thrown onto the top screen
      {
        const bool keeping = mode_ == Mode::Playing && snapshot.yourMove &&
                             snapshot.state.phase == CosmicHex::Phase::KeepContract;
        const auto choice = keeping ? keepChoiceOf(snapshot.state, snapshot.moves) : std::nullopt;
        if (choice)
          dealt_.deal(choice->cards, showsNow());
        else
          dealt_.stop();
      }
      // A new turn: the points bars' outline moves to its row
      if (latest_ && snapshot.state.turn != latest_->state.turn) {
        outlineFrom_ = latest_->state.turn;
        outlineMovedAt_ = showsNow();
      }
      // (pausing the game pauses its clock, so its animations and dice stop too)
      gameClock_.setPaused(snapshot.paused);
      if (moved)
        motionStarted_ = gameClock_.seconds();
      // Points and badges won: popping in (ScorePops)
      {
        std::vector<int> points, badges;
        for (int p = 0; p < snapshot.state.players; ++p) {
          points.push_back(CosmicHex::points(snapshot.state, p));
          badges.push_back(snapshot.state.player[static_cast<std::size_t>(p)].badges);
        }
        scorePops_.saw(points, badges, showsNow());
      }
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
      // Someone won: their ships fly round above the board, then the
      // title again, with who (ScreenFlow's Won)
      if (snapshot.state.winner >= 0 && !celebration_ && flow_.screen() == ScreenFlow::Screen::Game)
        celebrate(snapshot.state);
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
    pictures_.hexTile = renderer_->load(pieces.hexTile.data());
    pictures_.numberChip = renderer_->load(pieces.numberChip.data());
    pictures_.column = renderer_->load(pieces.column.data());
    station_ = renderer_->load(pieces.station.data());
    colony_ = renderer_->load(pieces.colony.data());
    ship_ = renderer_->load(pieces.ship.data());
    flame_ = renderer_->load(pieces.flame.data());
    stars_ = renderer_->load(pieces.stars.data());
    hexRing_ = renderer_->load(pieces.hexRing.data());
    cornerRing_ = renderer_->load(pieces.cornerRing.data());
    die_ = renderer_->load(DicePieces(static_cast<float>(Dice::DiceTray::kDieSize)).die.data());
    pictures_.die = die_;
    {
      std::array<Scene::Colour, 5> sides{};
      for (int r = 0; r < CosmicHex::kResources; ++r)
        sides[static_cast<std::size_t>(r)] =
            BoardColours::ofKind(static_cast<CosmicHex::PlanetKind>(r));
      pictures_.homeDie = renderer_->load(homeDie(sides, Palette::kHighlight).data());
    }
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
      // (Show resources: its cards in as it opens, back off as it closes)
      if (bottomScreen_->info() == BottomScreen::Info::Resources)
        cards_.open(showsNow());
      else if (shownInfo_ == BottomScreen::Info::Resources)
        cards_.close(showsNow());
      // (another player's panel: down as it opens, up as it closes)
      if (bottomScreen_->info() == BottomScreen::Info::Player)
        others_.open(bottomScreen_->infoPlayer(), showsNow());
      else if (shownInfo_ == BottomScreen::Info::Player)
        others_.close(showsNow());
      shownInfo_ = bottomScreen_->info();
      if (flow_.screen() == ScreenFlow::Screen::Game)
        holdFor(Hold::Info, shownInfo_ != BottomScreen::Info::None);
    }
    // A pressed button's glow (drawn again while it fades), and a tapped
    // button's message, for a second and a half
    if (flow_.screen() == ScreenFlow::Screen::Settings && settings_.pressing(now()))
      bottomDirty_ = true; // (a key's glow fading)
    if (bottomScreen_) {
      bottomScreen_->setTime(secondsSince(startedAt_));
      if (bottomScreen_->pressing() || pressDrawn_)
        bottomDirty_ = true;
      pressDrawn_ = bottomScreen_->pressing(); // (and once more, to clear it)
    }
    // (another player chosen while the panel's open: their view)
    if (bottomScreen_ && shownInfo_ == BottomScreen::Info::Player)
      others_.choose(bottomScreen_->infoPlayer());
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
    if (animations_ == Animations::Off && skippable())
      skipAnimations();
    firstRoll_.update(showsNow());
    if (firstRoll_.done(showsNow()))
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
    updateHomePay();
    updateRollPay();
    updateCardPlay();
    updatePick();
    updateCelebration();
    // The two cards dealt, drawn sharp before they're thrown in (on a DS
    // about a quarter of a second, while the screen is still)
    if (dealt_.needsCloseUp()) {
      closeUpCards(dealt_.cards());
      dealt_.closeUpDrawn(showsNow());
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
    if (kTitleScene && (screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Won ||
                        screen == ScreenFlow::Screen::Settings)) {
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
      still_.drawn(topMoving());
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
      const int winner = latest_->state.winner;
      Scene::Overlay &overlay = renderer_->overlay();
      overlay.textCentred(kTopWidth / 2.0f, kTopHeight - 34,
                          (mode_ == Mode::Playing && winner == 0 ? std::string("You win")
                                                                 : names_[winner] + " wins") +
                              "!",
                          Palette::kHighlight, 2);
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
        latest_ &&
        (yourRollWaits(*latest_) ? rollAsked_ : !banner_.showing(gameClock_.seconds())) &&
        !homePay_.moving(showsNow()); // (and the home's pay shown first)
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
    // Popping in as the dice settle (the user's: as the home's card does):
    // the hexagon growing a little past its size and settling, the words
    // growing with it in whole steps (the overlay's sharpest), the pair's
    // line once it's full size
    const float popped = ScratchPadd::Motion::along(
        ScratchPadd::Motion::Curve::Settle,
        std::min(1.0f, static_cast<float>(shownSeconds_) / kSecondsRollPop));
    // Both solid: a see-through hexagon's six triangles were blended twice
    // along the edges they share, which showed as dark lines
    overlay.hexagon(x, y, (radius + 3) * popped, Palette::kHighlight); // (an amber edge)
    overlay.hexagon(x, y, radius * popped, {12, 18, 30, 255});
    if (popped < 0.3f)
      return;
    const float grown = std::max(1.0f, std::round(big * std::min(1.0f, popped)));
    if (popped < 0.95f && dice_ == DiceStage::Showing) {
      overlay.textCentred(x, y, total, Palette::kHighlight, grown);
      return;
    }
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
        if (latest_) // (real time: the game's clock is held)
          firstRoll_.start(latest_->firstRoll,
                           static_cast<std::uint64_t>(Clock::now().time_since_epoch().count()),
                           kThisPlatform.diceStep, showsNow());
    }
    const Scene::Camera camera = cameraBetween(topDownCamera(), gameCamera(turn_), down);
    renderer_->beginFrame(camera, boardEnvironment(1 - down));
    if (!shown && descentStarted_)
      shown = &preview_; // (the game's first state on its way: the same board)
    // (not under another player's panel once it covers the screen: the
    // DS's frame is then the panel's alone)
    const bool covered = others_.inPlace(showsNow());
    if (!covered)
      renderer_->draw(stars_, Scene::Transform::identity(),
                      {.shading = Scene::Material::Shading::Unlit});
    if (shown && !covered) {
      auto timing = profile_.time(kProfilePieces);
      drawGaps(*shown, Scene::Transform::identity());
      drawPieces(*shown, Scene::Transform::identity());
    }
    // (behind another player's view, the dice wait unseen: the game's held)
    if (!others_.showing(showsNow()))
      drawDice(camera);
    // What shows over the board for a while (TopViews; on a DS words are on
    // a layer over every shape, so only one at a time)
    const TopFrame frame{*renderer_,
                         renderer_->overlay(),
                         camera,
                         static_cast<float>(kTopWidth),
                         static_cast<float>(kTopHeight),
                         pictures_,
                         names_,
                         showsNow()};
    // (a roll's pay, over the board, the HUD's words still showing: the
    // bots' cards fly to their rows of the points bars)
    if (latest_ && rollPay_.moving(showsNow()))
      drawRollPay(frame, rollPay_, latest_->state, mode_ == Mode::Playing ? 0 : -1);
    // (Show resources, over the board, under the views below)
    if (latest_ && cards_.showing(showsNow()))
      drawResources(frame,
                    latest_->state.player[static_cast<std::size_t>(infoPlayer(latest_->state))],
                    cards_);
    if (firstRoll_.on() && latest_)
      drawFirstRoll(frame, latest_->firstRoll, firstRoll_);
    else if (others_.showing(showsNow()) && latest_ && others_.which() >= 0)
      drawOtherPlayer(frame, viewOf(latest_->state, others_.which()), others_.down(showsNow()));
    else if (discovery_.moving(showsNow()))
      drawDiscovery(frame, discovery_, closeUpFace_);
    else if (pick_.at(showsNow()) && latest_ && bottomScreen_ && bottomScreen_->resourceChoice()) {
      drawPick(frame, pick_, bottomScreen_->resourceChoice()->title,
               latest_->state.player[static_cast<std::size_t>(latest_->state.turn)].resources,
               latest_->state.settings, BoardColours::ofPlayer(latest_->state.turn));
      drawBanner(renderer_->overlay());
    } else if (homePay_.moving(showsNow()) && latest_)
      drawHomePay(frame, homePay_, latest_->state,
                  mode_ == Mode::Playing && homePay_.current()->player == 0, closeUpCards_[0]);
    else if (cardPlay_.moving(showsNow()) && latest_)
      drawCardPlay(frame, cardPlay_, latest_->state, mode_ == Mode::Playing ? 0 : -1);
    else if (dealt_.on())
      drawDealtCards(frame, dealt_, bottomScreen_ ? bottomScreen_->keepChosen() : -1,
                     closeUpCards_);
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

  // A ship at `pose` in `colour`, its engine's flame when it burns
  // (swivelled, flickering a little, from the middle of its tail)
  void drawShip(const ShipPose &pose, const Scene::Transform &placement, Scene::Colour colour,
                std::uint8_t alpha = 31) {
    if (alpha == 0)
      return;
    const Scene::Transform at = placement * Scene::Transform::translation(pose.at) *
                                Scene::Transform::rotation({0, 1, 0}, pose.heading);
    renderer_->draw(ship_, at,
                    {.shading = Scene::Material::Shading::Toon, .colour = colour, .alpha = alpha});
    if (pose.flame <= 0)
      return;
    const float flicker = 0.85f + 0.15f * std::sin(static_cast<float>(gameClock_.seconds()) * 47);
    const float length = 0.65f * pose.flame * flicker;
    renderer_->draw(
        flame_,
        at * Scene::Transform::translation({-0.27f, 0.21f, 0}) *
            Scene::Transform::rotation({0, 1, 0}, pose.gimbal) *
            Scene::Transform::scaling({length, 1, 1}),
        {.shading = Scene::Material::Shading::Unlit, .colour = {255, 110, 40}, .alpha = alpha});
  }

  // Someone's won: their ships (and, if fewer than three, more from their
  // colonies) fly round above the board (WinCelebration), then the title;
  // straight to the title when animations are off
  std::optional<WinCelebration> celebration_;
  void celebrate(const State &state) {
    if (animations_ == Animations::Off) {
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
  void updateCelebration() {
    if (celebration_ && celebration_->over(showsNow())) {
      celebration_.reset();
      apply(flow_.on(ScreenFlow::Event::GameWon));
    }
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
        if (!pictures_.faces[static_cast<std::size_t>(face)])
          return Scene::Material{.shading = Scene::Material::Shading::Toon,
                                 .colour = BoardColours::ofFace(face)};
        return Scene::Material{.shading = Scene::Material::Shading::Toon,
                               .texture = pictures_.faces[static_cast<std::size_t>(face)]};
      };
      if (!turn) {
        renderer_->draw(pictures_.hexTile, placement * BoardPieces::onHex(hex), showing(up));
        return;
      }
      const float half = 1.5707963f;
      const bool upSide = *turn > half;
      const auto lift = Scene::Transform::translation({0, 0.6f * std::sin(*turn), 0});
      const auto spin = Scene::Transform::rotation({1, 0, 0}, upSide ? *turn - 2 * half : *turn);
      renderer_->draw(pictures_.hexTile, placement * BoardPieces::onHex(hex) * lift * spin,
                      showing(upSide ? up : down));
    };
    for (std::size_t i = 0; i < state.planets.size(); ++i) {
      // (one turned over is shown big first: face down here till it's
      // back, then simply face up: Discovery)
      const CosmicHex::Hex hex = state.planets[i].hex;
      if (discovery_.stillToShow(hex, showsNow())) {
        if (!discovery_.flying(hex, showsNow()))
          drawTile(hex, std::nullopt, CosmicHex::Art::Face::PlanetFaceDown,
                   CosmicHex::Art::Face::PlanetFaceDown);
        continue;
      }
      const bool shownBig = discovery_.shownBig(hex);
      const auto turn = shownBig ? std::nullopt : motion_.planetTurn(i, t);
      drawTile(hex, turn, CosmicHex::Art::Face::PlanetFaceDown, faceOf(state.planets[i]));
      // its number chip, once it's lying face up; one just
      // turned over drops onto it from above and settles (a
      // short bounce on a curve, the same every time)
      const int number = chipOf(state.planets[i]);
      const Scene::Texture chip = pictures_.chips[static_cast<std::size_t>(number)];
      if (number > 0 && !turn && chip) {
        float drop = 0;
        if (motion_.planetTurn(i, 0) && !shownBig) {
          const double since = gameClock_.seconds() - motionStarted_ - kSecondsAnimating;
          const float landed = ScratchPadd::Motion::along(
              ScratchPadd::Motion::Curve::Settle, static_cast<float>(since / kSecondsChipDrop));
          drop = since < 0 ? kChipDropHeight : (1 - landed) * kChipDropHeight;
        }
        renderer_->draw(
            pictures_.numberChip,
            placement * BoardPieces::onHex(state.planets[i].hex) *
                Scene::Transform::translation({0, drop, 0}),
            Scene::Material{.shading = Scene::Material::Shading::Toon, .texture = chip});
      }
    }
    for (std::size_t i = 0; i < state.zone.size(); ++i) {
      const CosmicHex::Hex hex = state.zone[i].hex;
      if (discovery_.stillToShow(hex, showsNow())) {
        if (!discovery_.flying(hex, showsNow()))
          drawTile(hex, std::nullopt, CosmicHex::Art::Face::FindFaceDown,
                   CosmicHex::Art::Face::FindFaceDown);
      } else
        drawTile(hex, discovery_.shownBig(hex) ? std::nullopt : motion_.siteTurn(i, t),
                 CosmicHex::Art::Face::FindFaceDown, faceOf(state.zone[i]));
    }
    // Stations and colonies, one just built dropping in with a puff of
    // dust (BoardMotion's landing)
    for (std::size_t i = 0; i < state.stations.size(); ++i) {
      const CosmicHex::Station &station = state.stations[i];
      const Landing landing = motion_.landing(i, t);
      const Scene::Transform corner = placement * BoardPieces::onCorner(station.at);
      renderer_->draw(station.colony ? colony_ : station_,
                      corner * Scene::Transform::translation({0, landing.drop, 0}) *
                          Scene::Transform::scaling({1, landing.squash, 1}),
                      toon(BoardColours::ofPlayer(station.owner)));
      if (landing.dust) {
        // (from under it out to three times a ring's size)
        const float spread =
            1 + 2 * ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::EaseOut, *landing.dust);
        renderer_->draw(cornerRing_, corner * Scene::Transform::scaling({spread, 1, spread}),
                        {.shading = Scene::Material::Shading::Unlit,
                         .colour = {236, 226, 204},
                         .alpha = static_cast<std::uint8_t>(26 * (1 - *landing.dust))});
      }
    }
    // Ships, flying from where they were, nose first, their engines
    // burning as they speed up (BoardMotion)
    // (the winner's flying round above it instead, once someone's won)
    for (std::size_t i = 0; i < state.ships.size(); ++i)
      if (!celebration_ || state.ships[i].owner != state.winner)
        drawShip(motion_.ship(i, t), placement, BoardColours::ofPlayer(state.ships[i].owner));
    if (celebration_ && state.winner >= 0)
      for (std::size_t i = 0; i < celebration_->ships(); ++i)
        drawShip(celebration_->pose(i, showsNow()), placement, BoardColours::ofPlayer(state.winner),
                 static_cast<std::uint8_t>(31 * celebration_->fadedIn(i, showsNow())));
    // The blockade, swooping to its planet
    if (const auto pose = motion_.blockade(t))
      drawShip(*pose, placement, BoardColours::kBlockade);
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
          static_cast<float>(showsNow() - *outlineMovedAt_) / kSecondsOutline);
      if (moved >= 1)
        outlineMovedAt_.reset();
      else
        outlineRow = static_cast<float>(outlineFrom_) +
                     (static_cast<float>(state.turn) - static_cast<float>(outlineFrom_)) * moved;
    }
    for (const PointsBars::Bar &bar : PointsBars::of(state, kTopWidth, outlineRow))
      overlay.rectangle(bar.x, bar.y, bar.width, bar.height, bar.colour);
    drawPointsPopping(overlay, state);
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
    if (!latest_->lastMoves.empty() && shownInfo_ == BottomScreen::Info::None &&
        !cards_.showing(showsNow()) && !others_.showing(showsNow()))
      overlay.text(4, kTopHeight - 14, latest_->lastMoves.back().substr(0, kCharsPerLine),
                   Palette::kSoftWords);
    // (the list of moves: debug builds only, when turned on)
    if (mode_ == Mode::Playing && latest_->yourMove && shownInfo_ == BottomScreen::Info::None &&
        !cards_.showing(showsNow()) && bottomScreen_ && bottomScreen_->showingMoveList())
      drawMoves(overlay);
    drawBanner(overlay);
    if (state.winner >= 0)
      overlay.textCentred(kTopWidth / 2.0f, kTopHeight / 2.0f,
                          (mode_ == Mode::Playing && state.winner == 0
                               ? std::string("You win")
                               : names_[state.winner] + " wins") +
                              "!",
                          BoardColours::ofPlayer(state.winner), kTopWidth >= 400 ? 3.0f : 2.5f);
  }

  // How big a pop draws something (ScorePops' size): its overshoot made
  // plain (Settle's is a tenth: on a dash 2 pixels high, nothing to see)
  static float popped(float size) { return size + 3 * std::max(0.0f, size - 1); }

  // Points just won popping in over their dashes (ScorePops): grey till
  // its turn, then growing past its size in white and settling in the
  // player's colour
  void drawPointsPopping(Scene::Overlay &overlay, const State &state) {
    const double now = showsNow();
    if (!scorePops_.popping(now))
      return;
    const int target = state.settings.winPoints;
    const float width = static_cast<float>(target) * PointsBars::kDashWidth +
                        static_cast<float>(target - 1) * PointsBars::kDashGap;
    const float left = (static_cast<float>(kTopWidth) - width) / 2;
    for (int p = 0; p < state.players; ++p)
      for (int d = 0; d < target; ++d) {
        const float size = scorePops_.dash(p, d, now);
        if (size == 1)
          continue;
        const float x =
            left + static_cast<float>(d) * (PointsBars::kDashWidth + PointsBars::kDashGap);
        const float y = PointsBars::kTop +
                        static_cast<float>(p) * (PointsBars::kRowHeight + PointsBars::kRowGap);
        overlay.rectangle(x, y, PointsBars::kDashWidth, PointsBars::kRowHeight,
                          PointsBars::kStillToWin);
        const float w = PointsBars::kDashWidth * popped(size);
        const float h = PointsBars::kRowHeight * (1 + 15 * std::max(0.0f, size - 1));
        overlay.rectangle(x + PointsBars::kDashWidth / 2 - w / 2,
                          y + PointsBars::kRowHeight / 2 - h / 2, w, h,
                          size > 1 ? Palette::kWords : BoardColours::ofPlayer(p));
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
      // (a badge just won popping in: ScorePops)
      const float size = scorePops_.badge(whose, i, showsNow());
      if (i < badges && size > 0)
        overlay.hexagon(x, y, 5 * k * popped(size), kAmber, true);
      if (i >= badges || size < 1)
        hexagonEdge(x, y, 5 * k, kEmpty);
    }
    overlay.textCentred(bx + 12 * k, 30 * k, "BADGES", kDim);
  }

  // Whose cards an info view shows: yours when playing, the turn's when
  // watching
  int infoPlayer(const State &state) const { return mode_ == Mode::Playing ? 0 : state.turn; }

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
    // (B: an animation straight to its end; pressed again, B's own job)
    if (pressed.pressed(back) && skippable()) {
      skipAnimations();
      return;
    }
    // (A: on from the winner's celebration)
    if (celebration_ && pressed.pressed(confirm)) {
      skipAnimations();
      return;
    }
    // (A: on from who goes first)
    if (firstRoll_.on() && pressed.pressed(confirm)) {
      firstRoll_.hurry(showsNow()); // (the next die now; once all have landed, on)
      if (firstRoll_.done(showsNow()))
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
    case ScreenFlow::Screen::Settings: {
      using Key = SettingsScreen::Key;
      for (const auto &[button, key] :
           {std::pair{Button::Up, Key::Up}, std::pair{Button::Down, Key::Down},
            std::pair{Button::Left, Key::Left}, std::pair{Button::Right, Key::Right},
            std::pair{confirm, Key::Confirm}, std::pair{back, Key::Back},
            std::pair{Button::Start, Key::Done}})
        if (pressed.pressed(button)) {
          settingsOutcome(settings_.press(key, now()));
          bottomDirty_ = true;
        }
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
        youPaused_ = !youPaused_;
        sendHold();
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
        } else if (screen == ScreenFlow::Screen::Settings) {
          settingsOutcome(settings_.touch(touch.x, touch.y, now()));
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
    } else if (screen == ScreenFlow::Screen::Settings) {
      settings_.draw(*bottomCanvas_, now());
      drawn = bottomCanvas_->pixels();
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
            marks.faceDown = discovery_.waitingAfterCurrent();
            if (const auto revealing = discovery_.revealing(showsNow()))
              marks.revealing =
                  BoardPicture::Marks::Revealing{.hex = revealing->hex, .shown = revealing->shown};
            picture_->draw(latest_->state, marks);
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
  // overlay's, sharp), pictures_.cardWidth across; once, as the DS hands out its
  // texture memory once (32 wide there: 2 KB a card)
  void loadCardPictures() {
    // Resources bigger (the user's: room for their titles, and
    // the cards under the top one easy to see), action cards smaller: on
    // the DS 44 and 32 pixels wide, 8 and 2 KB each, all of them fitting
    // beside the tile faces in the 128 KB the textures share
    pictures_.resourceWidth = kTopWidth >= 400 ? 64 : 44;
    pictures_.cardWidth = kTopWidth >= 400 ? 44 : 32;
    const auto load = [&](CosmicHex::Art::Card card, int width, float &uMax, float &vMax) {
      const int height = width * 52 / 63;
      const auto whole = recipePixels(CosmicHex::Art::cardRecipeOf(card), width, width * 88 / 63,
                                      kThisPlatform.recipeSmoothing);
      if (whole.empty())
        return Scene::Texture{};
      const std::vector<Scene::Colour> top(whole.begin(), whole.begin() + width * height);
      // (the band's colour, for its title's letters: dark on a light band)
      if (static_cast<int>(card) < CosmicHex::kResources)
        pictures_.resourceBands[static_cast<std::size_t>(card)] =
            top[static_cast<std::size_t>((width * 11 / 63) * width + width * 6 / 63)];
      const PaddedPicture picture = padded(top, width, height);
      uMax = picture.uMax;
      vMax = picture.vMax;
      return renderer_->load(Scene::TextureImage{picture.width, picture.height, picture.pixels});
    };
    float uMax = 1, vMax = 1;
    for (int r = 0; r < CosmicHex::kResources; ++r)
      pictures_.resources[static_cast<std::size_t>(r)] =
          load(static_cast<CosmicHex::Art::Card>(r), pictures_.resourceWidth, uMax, vMax);
    pictures_.resourceSquare = renderer_->load(pictureSquare(uMax, vMax).data());
    for (int c = CosmicHex::kResources; c < CosmicHex::Art::kCards; ++c)
      pictures_.cards[static_cast<std::size_t>(c)] =
          load(static_cast<CosmicHex::Art::Card>(c), pictures_.cardWidth, uMax, vMax);
    pictures_.pictureSquare = renderer_->load(pictureSquare(uMax, vMax).data());
    // The close-ups' slots, blank until one's shown (closeUpTile and
    // closeUpCards draw into them: PictureSlot)
    const int size = kThisPlatform.closeUpSize;
    closeUpFace_.load(*renderer_, size, size);
    for (ScratchPadd::PictureSlot &card : closeUpCards_)
      card.load(*renderer_, size, size);
    pictures_.closeUpCardSquare = renderer_->load(
        pictureSquare(1, static_cast<float>(size * 52 / 63) / static_cast<float>(size)).data());
    // The two backs, whole cards (the planets for resource cards, the rings
    // for action cards: the agreed designs), for cards held face down
    for (const CosmicHex::Art::CardBack design :
         {CosmicHex::Art::CardBack::Rings, CosmicHex::Art::CardBack::Planets}) {
      const auto whole = recipePixels(CosmicHex::Art::cardBackRecipe(design), pictures_.cardWidth,
                                      pictures_.cardWidth * 88 / 63, kThisPlatform.recipeSmoothing);
      if (whole.empty())
        continue;
      const PaddedPicture picture =
          padded(whole, pictures_.cardWidth, pictures_.cardWidth * 88 / 63);
      pictures_.backs[static_cast<std::size_t>(design)] =
          renderer_->load(Scene::TextureImage{picture.width, picture.height, picture.pixels});
      pictures_.backSquare = renderer_->load(pictureSquare(picture.uMax, picture.vMax).data());
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
      pictures_.faces[static_cast<std::size_t>(i)] =
          renderer_->load(Scene::TextureImage{kFaceSize, kFaceSize, pixels});
    }
    // and the number chips, half a face's size
    constexpr int kChipSize = kFaceSize / 2;
    for (int n = CosmicHex::Art::kLowestChip; n <= CosmicHex::Art::kHighestChip; ++n) {
      const std::vector<Scene::Colour> pixels =
          recipePixels(CosmicHex::Art::chipRecipeOf(n), kChipSize, kThisPlatform.recipeSmoothing);
      if (pixels.empty())
        continue;
      pictures_.chips[static_cast<std::size_t>(n)] =
          renderer_->load(Scene::TextureImage{kChipSize, kChipSize, pixels});
    }
  }

  Scene::Mesh station_, colony_, ship_, flame_, stars_, hexRing_, cornerRing_;
  // The pictures the board and the views over it draw with (TopViews)
  TopPictures pictures_;
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

  // A close-up's picture drawn into its slot (PlatformProfile::closeUpSize;
  // PictureSlot: drawn only when it isn't there already)
  void closeUpTile(CosmicHex::Art::Face face) {
    const int size = kThisPlatform.closeUpSize;
    closeUpFace_.show(*renderer_, static_cast<int>(face), [&] {
      return recipePixels(CosmicHex::Art::recipeOf(face), size, kThisPlatform.recipeSmoothing);
    });
  }
  void closeUpCards(const std::vector<CosmicHex::Art::Card> &cards) {
    const int size = kThisPlatform.closeUpSize, height = size * 52 / 63;
    for (std::size_t i = 0; i < cards.size() && i < closeUpCards_.size(); ++i)
      closeUpCards_[i].show(*renderer_, static_cast<int>(cards[i]), [&] {
        // (the card's picture: its drawing down to the picture's foot)
        const auto whole = recipePixels(CosmicHex::Art::cardRecipeOf(cards[i]), size,
                                        size * 88 / 63, kThisPlatform.recipeSmoothing);
        std::vector<Scene::Colour> square(static_cast<std::size_t>(size * size));
        if (whole.empty())
          return std::vector<Scene::Colour>{};
        std::copy(whole.begin(), whole.begin() + size * height, square.begin());
        return square;
      });
  }
  // Each frame: the game held while a roll's pay flies
  void updateRollPay() {
    // (once the dice have gone from the screen: not over the roll)
    if (rollPay_.waiting() && dice_ == DiceStage::Idle)
      rollPay_.start(showsNow());
    const RollPayShow::Step step = rollPay_.update(showsNow());
    if (step.hold || step.letGo)
      holdFor(Hold::RollPay, step.hold);
  }
  // Each frame: the home's pay held over the game; its card drawn sharp
  // (into the first dealt card's slot: the two never show together, and
  // the DS has no texture memory for another)
  // Each frame: while you choose a resource on the picker, the card in
  // focus shown big on top (PickShow), the banner as the choice opens
  void updatePick() {
    const bool choosing = mode_ == Mode::Playing && latest_ && latest_->yourMove && bottomScreen_ &&
                          bottomScreen_->mode() == BottomScreen::Mode::Resources;
    const auto focus = choosing ? bottomScreen_->resourceInFocus() : std::nullopt;
    if (focus && !pick_.at(showsNow()) && bottomScreen_->resourceChoice())
      banner_.start(gameClock_.seconds(), "CHOOSE A RESOURCE",
                    std::string(bottomScreen_->resourceChoice()->title), Palette::kHighlight);
    pick_.show(focus, showsNow());
  }
  void updateCardPlay() {
    const CardPlayShow::Step step = cardPlay_.update(showsNow());
    if (step.hold || step.letGo)
      holdFor(Hold::CardPlay, step.hold);
  }
  void updateHomePay() {
    if (const HomePayShow::Pay *pay = homePay_.current(); pay && pay->resource) {
      const auto card = static_cast<CosmicHex::Art::Card>(*pay->resource);
      if (!closeUpCards_[0].showing(static_cast<int>(card)))
        closeUpCards({card});
    }
    const HomePayShow::Step step = homePay_.update(showsNow());
    if (step.hold || step.letGo)
      holdFor(Hold::HomePay, step.hold);
  }
  // Each frame: a discovery's close-up drawn before it starts, the game
  // held while one shows and let go after the last (unless a view holds
  // it too), the bottom map drawn again as its tile fades in
  void updateDiscovery() {
    const DiscoveryShow::Step step =
        discovery_.update(showsNow(), [this](CosmicHex::Art::Face face) { closeUpTile(face); });
    if (step.hold || step.letGo)
      holdFor(Hold::Discovery, step.hold);
    if (step.redrawMap)
      bottomDirty_ = true;
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
    firstRoll_.stop();
    if (flow_.screen() == ScreenFlow::Screen::Game)
      holdFor(Hold::Start, false);
    if (latest_)
      startPlacingBanner(latest_->state);
  }
  // Whether anything on the top screen moves now
  bool topMoving() const {
    if (flow_.screen() != ScreenFlow::Screen::Game || !latest_ || descentStarted_ ||
        dice_ != DiceStage::Idle || firstRoll_.moving())
      return true;
    if (gameClock_.seconds() - motionStarted_ < kSecondsBoardMoving)
      return true; // (pieces still moving, or a number chip dropping)
    if (banner_.showing(gameClock_.seconds()))
      return true; // (the turn's banner moving)
    if (shownInfo_ != BottomScreen::Info::None || cards_.moving(showsNow()) || outlineMovedAt_)
      return true; // (an info view coming in or going, the points' outline)
    // (the views over the board: each says)
    if (scorePops_.popping(showsNow()) || cardPlay_.moving(showsNow()) || celebration_ ||
        pick_.moving(showsNow()))
      return true; // (points or badges popping in, a card played)
    if (dealt_.moving() || others_.moving(showsNow()) || discovery_.moving(showsNow()) ||
        homePay_.moving(showsNow()) || rollPay_.moving(showsNow()))
      return true;
    return false;
  }
  bool topIsStill() const {
    // (StillFrames: after something moved, one frame more, as its last
    // drawn frame may still show a trace of it: a banner's band fading)
    const bool changed =
        snapshots_ != drawnSnapshots_ || turn_ != drawnTurn_ || selected_ != drawnSelection_;
    if (!still_.canHold(topMoving(), changed))
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
    return screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Setup ||
           screen == ScreenFlow::Screen::Settings;
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
    if (firstRoll_.on() || others_.showing(showsNow()) || dealt_.on() ||
        discovery_.moving(showsNow()) || homePay_.moving(showsNow()))
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
  static constexpr float kSecondsRollPop = 0.3f; // (the roll's number popping in)
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
  // The app's real clock, in seconds (menus, buttons)
  double now() const { return secondsSince(startedAt_); }
  // The shows' clock (what shows over the board): real time at the
  // Animations setting's pace (twice as fast when Quick), jumping ahead
  // when an animation's skipped; it runs while the game's held
  ScratchPadd::GameClock showClock_;
  double showsNow() const { return showClock_.seconds(); }
  ScratchPadd::Banner banner_{static_cast<float>(kTopWidth)};
  // The info views (Show resources): which is shown, since when, and
  // whether it holds the game; the cards' pictures for them
  BottomScreen::Info shownInfo_ = BottomScreen::Info::None;
  CardsShow cards_;                      // (Show resources' cards in and off)
  std::optional<double> outlineMovedAt_; // (the turn's outline moving: since, on the shows' clock)
  int outlineFrom_ = 0;                  // (from this player's row)
  // What shows over the board for a while (TopShows; drawn by TopViews),
  // each on the shows' clock (showsNow())
  DiscoveryShow discovery_;         // (tiles turned over)
  DealtShow dealt_;                 // (the two cards dealt)
  PanelShow others_{kSecondsPanel}; // (another player's panel)
  FirstRollShow firstRoll_;         // (who goes first)
  HomePayShow homePay_;             // (a home's pay on its die)
  RollPayShow rollPay_;             // (a roll's pay, card by card)
  ScorePops scorePops_;             // (points and badges won, popping in)
  CardPlayShow cardPlay_;           // (an action card played, a Charter revealed)
  PickShow pick_;                   // (choosing a resource: the card in focus, big)
  ScratchPadd::StillFrames still_;  // (when the top screen can hold its last frame)
  bool youPaused_ = false;          // (watching: paused with A, so the PAUSED band shows)
  // Why the game is held, if it is: one bit each. The table is told to hold
  // while any is set or you've paused, and to go on once none is
  enum class Hold : std::uint8_t {
    Start = 1,     // (the board coming down, who goes first)
    Menu = 2,      // (the pause menu)
    Info = 4,      // (Show resources, another player's panel)
    Discovery = 8, // (a tile shown big)
    HomePay = 16,  // (a home's pay)
    RollPay = 32,  // (a roll's pay)
    CardPlay = 64  // (an action card played)
  };
  std::uint8_t holds_ = 0;
  void holdFor(Hold why, bool on) {
    const auto bit = static_cast<std::uint8_t>(why);
    const std::uint8_t before = holds_;
    holds_ =
        on ? static_cast<std::uint8_t>(holds_ | bit) : static_cast<std::uint8_t>(holds_ & ~bit);
    if ((before == 0) != (holds_ == 0))
      sendHold();
  }
  void sendHold() { send(SetHold{youPaused_ || holds_ != 0}); }
  // Who's who: your name (the console's owner's), the bots' (PlayerNames),
  // made as each game starts
  // (yours: as you set it in the settings, kept on the card, or else the
  // console's owner name)
  std::string consoleName_ = ScratchPadd::OwnerName::get();
  std::string yourName_ = savedName().value_or(consoleName_);
  static constexpr const char *kNameFile = "name.txt";
  static std::optional<std::string> savedName() {
    const auto bytes =
        ScratchPadd::Storage::loadFile(ScratchPadd::Storage::Folders::forApp().files + kNameFile);
    if (!bytes || bytes->empty())
      return std::nullopt;
    return std::string(bytes->begin(), bytes->end());
  }
  void saveName(const std::string &name) {
    sendTo<ScratchPadd::Storage::StoragePadd>(ScratchPadd::Storage::SaveFile{
        .path = kNameFile,
        .bytes = std::make_shared<const ScratchPadd::Storage::Bytes>(name.begin(), name.end())});
  }
  PlayerNames names_;
  // How much the game animates (the user's: Full, Quick or Off), kept on
  // the card
  static constexpr const char *kAnimationsFile = "animations.txt";
  Animations animations_ = savedAnimations();
  static Animations savedAnimations() {
    const auto bytes = ScratchPadd::Storage::loadFile(
        ScratchPadd::Storage::Folders::forApp().files + kAnimationsFile);
    if (!bytes)
      return Animations::Full;
    return animationsFrom(std::string(bytes->begin(), bytes->end())).value_or(Animations::Full);
  }
  void saveAnimations() {
    const std::string_view word = wordOf(animations_);
    sendTo<ScratchPadd::Storage::StoragePadd>(ScratchPadd::Storage::SaveFile{
        .path = kAnimationsFile,
        .bytes = std::make_shared<const ScratchPadd::Storage::Bytes>(word.begin(), word.end())});
  }
  // The board's moves, the dice and the shows at the setting's pace
  void paceAnimations() {
    gameClock_.setRate(rateOf(animations_));
    showClock_.setRate(rateOf(animations_));
  }

  // Whether something is animating that B (Back) can skip to its end: the
  // dice, who goes first, the board's pieces moving, the turn's banner, a
  // tile turned over, a home's or a roll's pay, the cards dealt coming in
  bool skippable() const {
    if (flow_.screen() != ScreenFlow::Screen::Game || !latest_)
      return false;
    if (dice_ != DiceStage::Idle || firstRoll_.on() || celebration_)
      return true;
    if (gameClock_.seconds() - motionStarted_ < kSecondsBoardMoving)
      return true;
    if (banner_.showing(gameClock_.seconds()))
      return true;
    const double t = showsNow();
    const bool dealing =
        dealt_.on() && !dealt_.cards().empty() && dealt_.turned(dealt_.cards().size() - 1, t) < 1;
    return dealing || discovery_.moving(t) || homePay_.moving(t) || rollPay_.moving(t) ||
           cardPlay_.moving(t);
  }
  // Every animation straight to its end frame (the user's: B, or every
  // frame when Animations is Off)
  void skipAnimations() {
    // The shows: their clock on past any of their ends
    static constexpr double kPastAnyShow = 30;
    showClock_.skipAhead(kPastAnyShow);
    // Who goes first: each die set down and the order read
    for (int i = 0; i < 16 && firstRoll_.on() && !firstRoll_.done(showsNow()); ++i)
      firstRoll_.hurry(showsNow());
    if (firstRoll_.done(showsNow()))
      endFirstRoll();
    // The board's pieces where they were going, the turn's banner gone
    motionStarted_ = std::min(motionStarted_, gameClock_.seconds() - kSecondsBoardMoving);
    banner_.stop();
    skipDice();
  }
  // The dice a stage on: rolling, the physics run on to where they land
  // (all at once: a moment on a DS) and the roll shown; showing, sent and
  // pulled away; being pulled, gone
  void skipDice() {
    if (dice_ == DiceStage::Rolling) {
      const auto most = static_cast<int>(kMostSecondsRolling / kThisPlatform.diceStep);
      for (int i = 0; i < most && !tray_.settled(); ++i)
        tray_.step(kThisPlatform.diceStep);
      if (!tray_.settled())
        return; // (the watchdog throws them again)
      dice_ = DiceStage::Showing;
      shownSeconds_ = 0;
      rolled_ = tray_.faces();
      diceOwed_ = 0;
      lastDiceTime_ = gameClock_.seconds();
    } else if (dice_ == DiceStage::Showing) {
      shownSeconds_ = kSecondsShowingDice; // (sent and pulled next frame)
    } else if (dice_ == DiceStage::Pulling) {
      dice_ = DiceStage::Idle;
    }
  }

  // Close-ups (PlatformProfile::closeUpSize): a discovered tile's face and
  // the two cards dealt, drawn sharp when they're shown, into slots loaded
  // once and reused (PictureSlot)
  ScratchPadd::PictureSlot closeUpFace_;
  std::array<ScratchPadd::PictureSlot, 2> closeUpCards_;
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
  ButtonMenu titleMenu_{{"Play", "Settings", "Quit"}};
  // The settings (from the title): your name, changed on a keyboard
  SettingsScreen settings_{kBottomWidth, kBottomHeight};
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
      static constexpr std::array<Event, 3> kTitle{Event::Play, Event::Settings, Event::Quit};
      if (button < kTitle.size())
        apply(flow_.on(kTitle[button]));
      if (flow_.screen() == ScreenFlow::Screen::Settings)
        settings_.open(yourName_, yourName_ == consoleName_ && !consoleName_.empty(), animations_);
      updatePreview();
      return;
    }
    static constexpr std::array<Event, 4> kPause{Event::Resume, Event::Restart, Event::ToTitle,
                                                 Event::Quit};
    if (button < kPause.size())
      apply(flow_.on(kPause[button]));
  }

  // The settings' answer: your name changed (kept on the card), or back to
  // the title
  void settingsOutcome(SettingsScreen::Outcome outcome) {
    if (outcome == SettingsScreen::Outcome::NameChanged) {
      yourName_ = settings_.name();
      saveName(yourName_);
    } else if (outcome == SettingsScreen::Outcome::AnimationsChanged) {
      animations_ = settings_.animations();
      paceAnimations();
      saveAnimations();
    } else if (outcome == SettingsScreen::Outcome::Back) {
      apply(flow_.on(ScreenFlow::Event::Back));
    }
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
      celebration_.reset();
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
      holdFor(Hold::Menu, step.holdGame);
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
    holds_ = descentStarted_ ? static_cast<std::uint8_t>(Hold::Start) : 0;
    names_ =
        PlayerNames::forGame(menu_.players(), play ? 0 : -1, yourName_,
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
