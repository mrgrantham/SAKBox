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
// L/R: turn the board   X/Y: zoom the bottom board in
// and out   touch: select a hex (drag: move the board when zoomed)   START: quit
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
#include <CosmicHex/Handheld/ButtonMenu.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/MenuPicture.hpp>
#include <CosmicHex/Handheld/MoveSpots.hpp>
#include <CosmicHex/Handheld/ScreenFlow.hpp>
#include <CosmicHex/Handheld/StartMenu.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>
#include <ScratchPadd/FrameProfile.hpp>
#include <ScratchPadd/GameClock.hpp>
#include <ScratchPadd/MemoryStats.hpp>
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

// The title screen: the 3D scene (round 87), or the plain text title
// (false). Which one a player sees, and how it's chosen, is an open
// question on the roadmap page (round 87); the scene for now
constexpr bool kTitleScene = true;

// An unchanged top screen isn't drawn again (topIsStill): on the DS, whose
// 3D hardware shows the last frame by itself; elsewhere (the 3DS has a GPU
// of its own, and its frames must be sent) it's drawn as ever
#if defined(__NDS__)
constexpr bool kHoldStillFrames = true;
#else
constexpr bool kHoldStillFrames = false;
#endif

// The bots' frame arena (round 90), and none if 0. One simulated game held
// up to about 75 KB (64-bit, in the test); the DS has under 200 KB free, so
// it takes less (a game that outgrows the arena carries on from the heap)
#if defined(__NDS__)
constexpr std::size_t kFrameArenaBytes = 64 * 1024;
// How smooth the tile faces drawn from recipes are: 2 x 2 samples a pixel
// on the DS (a quarter of the work; its faces are about 30 pixels), 4 x 4
// elsewhere (round 92)
constexpr auto kRecipeSmoothing = ScratchPadd::TextureRecipe::Smoothing::Coarse;
// How the bots score moves: in fixed point on the DS, which has no
// floating-point hardware (round 92)
constexpr auto kBotArithmetic = CosmicHex::Handheld::TableSettings::Arithmetic::FixedPoint;
#else
constexpr std::size_t kFrameArenaBytes = 128 * 1024;
constexpr auto kRecipeSmoothing = ScratchPadd::TextureRecipe::Smoothing::Fine;
constexpr auto kBotArithmetic = CosmicHex::Handheld::TableSettings::Arithmetic::Double;
#endif
#if defined(__NDS__)
constexpr int kTopWidth = 256, kTopHeight = 192, kBottomWidth = 256, kBottomHeight = 192;
constexpr std::size_t kCharsPerLine = 31;
constexpr float kLineHeight = 10;
constexpr int kBotIterations = 200;
bool mainLoop() { return pmMainLoop(); }
constexpr int kFaceSize = 32;    // (the faces' textures: VRAM bank B is small)
constexpr int kTitleStars = 110; // (two polygons each, of the DS's 2048)
#else
constexpr int kTopWidth = 400, kTopHeight = 240, kBottomWidth = 320, kBottomHeight = 240;
constexpr std::size_t kCharsPerLine = 64;
constexpr float kLineHeight = 11;
constexpr int kBotIterations = 400; // (the simulations' bots)
bool mainLoop() { return aptMainLoop(); }
constexpr int kFaceSize = 64;
constexpr int kTitleStars = 260;
#endif
namespace Input = ScratchPadd::Input;
using CosmicHex::State;
namespace Dice = CosmicHex::Dice;
using namespace CosmicHex::Handheld;
using Clock = std::chrono::steady_clock;

// How long each move stays on screen before the next, so a game can be
// watched (the dealer's draws go straight on), and how long its slide or
// flip takes
constexpr double kSecondsAMove = 0.6;
constexpr double kSecondsAnimating = 0.35;

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
  // The game waits for the dice (round 87): the screen throws them with
  // this seed (a new one each roll) and sends back what they show
  bool waitingForDice = false;
  std::uint64_t diceSeed = 0;
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
};
struct TogglePause {};
// The pause menu holds the game (true) or lets it go on (false); leaving a
// game stops it (round 90)
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
      // Bots think up to 0.8 seconds a move (and at most kBotIterations)
      TableSettings settings{
          .botIterations = kBotIterations, .botSeconds = 0.8, .seed = startingSeed()};
      if (start.withPerson)
        settings.seats[0] = Seat::Person; // (you're player 1)
      settings.players = start.players;
      settings.board = start.board;
      settings.finishRound = start.finishRound;
      settings.seatBonus = start.seatBonus;
      settings.physicsDice = true; // (thrown on the top screen; round 87)
      // Each simulated game in a frame arena (round 90: the search made
      // 59,000 allocations a second; one game holds up to about 75 KB)
      settings.frameArenaBytes = kFrameArenaBytes;
      settings.botArithmetic = kBotArithmetic;
      settings.measureSearch = kWatchAtStart; // (the -watch builds show it)
      seed_ = settings.seed;
      paused_ = false;
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
        lastThinkSeconds_ = std::chrono::duration<double>(Clock::now() - started).count();
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
      if (latest_)
        motion_ = BoardMotion(latest_->state, snapshot.state);
      // (pausing the game pauses its clock, so its animations and dice stop too)
      gameClock_.setPaused(snapshot.paused);
      motionStarted_ = gameClock_.seconds();
      latest_ = snapshot;
      ++snapshots_; // (the top screen has something new to show)
      bottomDirty_ = true;
      // Someone won: the title again, with who (ScreenFlow's Won)
      if (snapshot.state.winner >= 0)
        apply(flow_.on(ScreenFlow::Event::GameWon));
    });
  }
  std::string name() override { return "ScreenPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }

  void starting() override {
    display_.emplace(); // the top screen
                        // The bottom screen: the flat board (BoardPicture), written straight
                        // into its one framebuffer when it changes
#if defined(__NDS__)
    // The DS's bottom screen: a 16-bit bitmap in VRAM bank C (the scene layer
    // has B, D and E)
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    bottomPixels_ = bgGetGfxPtr(bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0));
#else
    gfxSetDoubleBuffering(GFX_BOTTOM, false);
#endif
    picture_.emplace(kBottomWidth, kBottomHeight, kRecipeSmoothing);
    renderer_.emplace(*display_);
    const BoardPieces pieces;
    hexTile_ = renderer_->load(pieces.hexTile.data());
    numberChip_ = renderer_->load(pieces.numberChip.data());
    station_ = renderer_->load(pieces.station.data());
    colony_ = renderer_->load(pieces.colony.data());
    ship_ = renderer_->load(pieces.ship.data());
    stars_ = renderer_->load(pieces.stars.data());
    highlight_ = renderer_->load(pieces.highlight.data());
    die_ = renderer_->load(DicePieces(static_cast<float>(Dice::DiceTray::kDieSize)).die.data());
    // The title scene (round 87): the 3D title, the rocket, the night sky
    // (fewer stars on the DS: each is two of its 2048 polygons)
    title_.emplace(*renderer_, Assets::kRocket,
                   ScratchPadd::TitleScene::Settings{.title = "COSMIC HEX",
                                                     .aspect = static_cast<float>(kTopWidth) /
                                                               static_cast<float>(kTopHeight),
                                                     .stars = kTitleStars});
    titleStarted_ = Clock::now();
    loadFaces();
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
    if (!mainLoop()) {
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
    handleButtons(pressed);
    // X and Y zoom the bottom board in and out
    if (pressed.pressed(Input::Button::FaceTop)) {
      picture_->setZoom(picture_->zoom() + 1);
      bottomDirty_ = true;
    }
    if (pressed.pressed(Input::Button::FaceLeft)) {
      picture_->setZoom(picture_->zoom() - 1);
      bottomDirty_ = true;
    }
    handleTouch(pad.touch);
    profile_.frameStarted();
    if (bottomDirty_) {
      auto timing = profile_.time(kProfileBottom);
      drawBottom();
    }
    {
      auto timing = profile_.time(kProfileDice);
      throwDice();
    }
    measureMemory();
    // The map's tile faces, drawn from their recipes one a frame while
    // there's nothing else to do (the title, the setup), so the first map
    // of a game doesn't stall for seconds on the DS (round 92)
    if (onAMenuScreen()) {
      auto timing = profile_.time(kProfileBottom);
      picture_->prepare();
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
    // L and R turn the board while held
    if (pad.isHeld(Input::Button::L))
      turn_ -= 0.02f;
    if (pad.isHeld(Input::Button::R))
      turn_ += 0.02f;
    if (screen == ScreenFlow::Screen::Setup) {
      drawSetup();
    } else if (kHoldStillFrames && topIsStill()) {
      // Nothing on the top screen has changed: send nothing and let the
      // DS's 3D hardware show the last frame again, which it does by itself
      // from what it was last sent (GBATEK: the rendering engine draws its
      // polygon memory every frame; only a new flush changes it). The time
      // goes to the bots' thread while this waits (round 92: on a DS,
      // drawing an unchanged board each frame left them 4% of the CPU)
      auto timing = profile_.time(kProfileWait);
      waitForTheRefresh();
    } else {
      drawBoard(latest_ ? &latest_->state : nullptr);
      drawnSnapshots_ = snapshots_;
      drawnTurn_ = turn_;
      drawnSelection_ = selected_ ? std::optional<HexGrid::Hex>(selected_->hex) : std::nullopt;
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
    title_->draw(
        *renderer_,
        static_cast<float>(std::chrono::duration<double>(Clock::now() - titleStarted_).count()));
    if (flow_.screen() == ScreenFlow::Screen::Won && latest_) {
      char line[64];
      std::snprintf(line, sizeof line, "Player %d wins!", latest_->state.winner + 1);
      Scene::Overlay &overlay = renderer_->overlay();
      overlay.text(kTopWidth / 2.0f - 70, kTopHeight - 40, line, {255, 214, 90}, 2);
      overlay.text(kTopWidth / 2.0f - 60, kTopHeight - 16, "A: to the title", {200, 208, 222});
    }
    showProfile();
    {
      auto timing = profile_.time(kProfileWait);
      renderer_->endFrame();
    }
  }

  // Memory (round 89: measuring before optimising, Gregory 10.9): with the
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
    const double seconds = std::chrono::duration<double>(Clock::now() - memoryAt_).count();
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

  // The dice (round 87): when the game waits for them, they're thrown into
  // the glass container (DiceTray), stepped once a frame until they settle,
  // shown for a moment, their faces sent to the table, and the container
  // pulled off the screen
  void throwDice() {
    if (dice_ == DiceStage::Idle && latest_ && latest_->waitingForDice &&
        latest_->diceSeed != thrownSeed_) {
      tray_.throwDice(latest_->diceSeed);
      thrownSeed_ = latest_->diceSeed;
      dice_ = DiceStage::Rolling;
      diceOwed_ = 0;
      lastDiceTime_ = thrownAt_ = gameClock_.seconds();
    }
    if (dice_ == DiceStage::Idle)
      return;
    // Fixed steps of kDiceStep, as many as real time has passed (Gregory,
    // Game Engine Architecture 8.5: integrators want a constant step; Doom's
    // tics, Black Book 5.4): the same throw whatever the frame rate (the DS
    // runs 41 to 60), at most kMostDiceSteps a frame so a stall slows the
    // dice rather than snowballing
    const double now = gameClock_.seconds();
    diceOwed_ += now - lastDiceTime_;
    lastDiceTime_ = now;
    int steps = 0;
    for (; diceOwed_ >= kDiceStep && steps < kMostDiceSteps; ++steps) {
      tray_.step(kDiceStep);
      diceOwed_ -= kDiceStep;
    }
    if (steps == kMostDiceSteps)
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
               (shownSeconds_ += steps * kDiceStep) >= kSecondsShowingDice) {
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
    // screen on a hexagon (round 90, the user's), going with the glass as
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
    const float big = kTopWidth >= 400 ? 5 : 4; // (the 3DS's wider screen: bigger)
    const float radius = kTopWidth >= 400 ? 50.0f : 42.0f;
    const float x = (*middle)[0], y = (*middle)[1];
    overlay.hexagon(x, y, radius + 3, {255, 214, 90, 255}); // (an amber edge)
    overlay.hexagon(x, y, radius, {12, 18, 30, 230});
    // Centred on the hexagon (round 90, the user's): across, by the letters'
    // own width (textWidth counts the gap after the last letter, a point at
    // the scale), and down, the total's middle a little above the centre and
    // the two dice under it, by the letters' height (7 points at the scale)
    constexpr float kLetterHeight = 7, kSmall = 1.5f;
    const auto across = [&](const char *text, float scale) {
      return x - (Scene::Overlay::textWidth(text, scale) - scale) / 2;
    };
    const float totalTop = y - kLetterHeight * big / 2 - 8;
    overlay.text(across(total, big), totalTop, total, {255, 214, 90}, big);
    overlay.text(across(pair, kSmall), totalTop + kLetterHeight * big + 8, pair, {226, 232, 244},
                 kSmall);
  }

  // The board from above and in front, turned by L and R (looking a little
  // in front of the centre, so the near hexes stay on screen)
  // The board's camera: from above and in front, turned by L and R (looking
  // a little in front of the centre, so the near hexes stay on screen)
  Scene::Camera boardCamera() const {
    const float distance = 9.5f, height = 9.5f;
    return Scene::Camera{.eye = {distance * std::sin(turn_), height, distance * std::cos(turn_)},
                         .target = {1.2f * std::sin(turn_), 0, 1.2f * std::cos(turn_)},
                         .up = {0, 1, 0},
                         .fieldOfView = 50,
                         .nearPlane = 1,
                         .farPlane = 60};
  }
  // The board's light: the game's (a lamp above and to one side, toon
  // bands), or, `flat` of the way to it, bright and even (the setup's: the
  // board seen from above looks like the flat map; round 90, the user's)
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

  // The board from straight above, all of it on the screen (the setup's
  // view, like the flat map; up the screen is away from the game's camera)
  static Scene::Camera topDownCamera() {
    return Scene::Camera{.eye = {0, 18, 0.01f},
                         .target = {0, 0, 0},
                         .up = {0, 0, -1},
                         .fieldOfView = 50,
                         .nearPlane = 1,
                         .farPlane = 60};
  }

  // How far the board has come down from above to the game's view since the
  // game started (0 to 1, on the dice's curve: slideAcross), or 1 once there
  float descent() const {
    if (!descentStarted_)
      return 1;
    return slideAcross(
        static_cast<float>(std::chrono::duration<double>(Clock::now() - *descentStarted_).count()));
  }
  static Scene::Camera between(const Scene::Camera &from, const Scene::Camera &to, float t) {
    Scene::Camera out = to;
    out.eye = from.eye + (to.eye - from.eye) * t;
    out.target = from.target + (to.target - from.target) * t;
    out.up = (from.up + (to.up - from.up) * t).normalised();
    return out;
  }

  // The game's board (or none yet), with the dice, the words over it and the
  // selection
  void drawBoard(const State *shown) {
    // (coming down from above at the start of a game: round 90, the user's)
    const float down = descent();
    if (down >= 1)
      descentStarted_.reset();
    const Scene::Camera camera = between(topDownCamera(), boardCamera(), down);
    renderer_->beginFrame(camera, boardEnvironment(1 - down));
    if (!shown && descentStarted_)
      shown = &preview_; // (the game's first state on its way: the same board)
    renderer_->draw(stars_, Scene::Transform::identity(),
                    {.shading = Scene::Material::Shading::Unlit});
    if (shown) {
      drawGaps(*shown, Scene::Transform::identity());
      drawPieces(*shown, Scene::Transform::identity());
    }
    drawDice(camera);
    drawText();
    // The selected hex, ringed on the 3D board too
    if (selected_)
      renderer_->draw(highlight_, BoardPieces::onHex(selected_->hex),
                      {.shading = Scene::Material::Shading::Unlit, .colour = {255, 214, 90}});
    showProfile();
    {
      auto timing = profile_.time(kProfileWait);
      renderer_->endFrame(); // waits for the screen's refresh
    }
  }

  // The setup's board, still (round 90, the user's: turning didn't help); on
  // a change of board, the old one slides out and the new one in, on the
  // dice's curve (slideAcross), the way the arrow pointed
  void drawSetup() {
    const Scene::Camera camera = topDownCamera();
    renderer_->beginFrame(camera, boardEnvironment(1));
    renderer_->draw(stars_, Scene::Transform::identity(),
                    {.shading = Scene::Material::Shading::Unlit});
    const float through =
        sliding_ ? slideAcross(static_cast<float>(
                       std::chrono::duration<double>(Clock::now() - slideStarted_).count()))
                 : 1;
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
      drawPieces(previewBefore_, Scene::Transform::translation(right * (-sign * through * wide)));
      drawPieces(preview_, Scene::Transform::translation(right * (sign * (1 - through) * wide)));
    }
    showProfile();
    {
      auto timing = profile_.time(kProfileWait);
      renderer_->endFrame();
    }
  }

  // A dim, see-through grey in the gaps between the tiles (round 80: dimmer),
  // made again when the board changes (another game, or the setup's choice)
  void drawGaps(const State &state, const Scene::Transform &placement) {
    updateGaps(state);
    if (gaps_)
      renderer_->draw(
          gaps_, placement,
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
      const auto turn = motion_.planetTurn(i, t);
      drawTile(state.planets[i].hex, turn, CosmicHex::Art::Face::PlanetFaceDown,
               faceOf(state.planets[i]));
      // its number chip (round 87), once it's lying face up
      const int number = chipOf(state.planets[i]);
      const Scene::Texture chip = chips_[static_cast<std::size_t>(number)];
      if (number > 0 && !turn && chip)
        renderer_->draw(
            numberChip_, placement * BoardPieces::onHex(state.planets[i].hex),
            Scene::Material{.shading = Scene::Material::Shading::Toon, .texture = chip});
    }
    for (std::size_t i = 0; i < state.zone.size(); ++i)
      drawTile(state.zone[i].hex, motion_.siteTurn(i, t), CosmicHex::Art::Face::FindFaceDown,
               faceOf(state.zone[i]));
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
    std::snprintf(line, sizeof line, "Round %d%s", state.round,
                  flow_.screen() == ScreenFlow::Screen::Paused ? "  (paused)"
                  : latest_->paused                            ? "  (paused: A)"
                                                               : "");
    overlay.text(4, 4, line, {226, 232, 244});
    for (int p = 0; p < state.players; ++p) {
      const auto &r = state.player[static_cast<std::size_t>(p)].resources;
      std::snprintf(line, sizeof line, "%sP%d %2d  M%d F%d U%d W%d C%d",
                    state.turn == p ? ">" : " ", p + 1, CosmicHex::points(state, p), r[0], r[1],
                    r[2], r[3], r[4]);
      overlay.text(4, 16 + 10 * static_cast<float>(p), line, BoardColours::ofPlayer(p));
    }
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
        overlay.text(4, kTopHeight - 98, where, {120, 220, 160});
      }
    }
    if (kWatchAtStart && !memoryBusiest_.empty())
      overlay.text(4, kTopHeight - 38, memoryBusiest_.substr(0, kCharsPerLine), {120, 220, 160});
    if (!latest_->lastMoves.empty())
      overlay.text(4, kTopHeight - 14, latest_->lastMoves.back().substr(0, kCharsPerLine),
                   {200, 208, 222});
    if (mode_ == Mode::Playing && latest_->yourMove)
      drawMoves(overlay);
    if (state.winner >= 0) {
      std::snprintf(line, sizeof line, "Player %d wins!", state.winner + 1);
      overlay.text(kTopWidth / 2.0f - 60, kTopHeight / 2.0f - 10, line, {255, 214, 90}, 1.5f);
    }
  }

  // The buttons, by screen (ScreenFlow): the title's and the pause menu's
  // buttons (up, down, A), the setup's menu (StartMenu: up, down, left,
  // right, A to start, B back), any button once a game's won; in a game,
  // watching: A pauses or goes on; playing, on your move: up and down choose
  // from the list (the moves at the touched spot, or all of them), A makes
  // it, B shows them all again
  void handleButtons(const Input::Edges &pressed) {
    using Input::Button;
    switch (flow_.screen()) {
    case ScreenFlow::Screen::Title:
    case ScreenFlow::Screen::Paused: {
      ButtonMenu &menu = flow_.screen() == ScreenFlow::Screen::Title ? titleMenu_ : pauseMenu_;
      std::optional<std::size_t> chosen;
      for (const auto &[button, key] : {std::pair{Button::Up, ButtonMenu::Key::Up},
                                        std::pair{Button::Down, ButtonMenu::Key::Down},
                                        std::pair{Button::FaceRight, ButtonMenu::Key::Confirm}})
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
            std::pair{Button::FaceRight, Key::Confirm}, std::pair{Button::FaceBottom, Key::Back}})
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
      if (pressed.pressed(Button::FaceRight) || pressed.pressed(Button::FaceBottom))
        apply(flow_.on(ScreenFlow::Event::Continue));
      return;
    case ScreenFlow::Screen::Game:
      break;
    }
    if (mode_ == Mode::Watching) {
      if (pressed.pressed(Button::FaceRight))
        send(TogglePause{});
      return;
    }
    if (!latest_ || !latest_->yourMove)
      return;
    const std::vector<std::size_t> shown = movesShown();
    if (pressed.pressed(Button::Down) && cursor_ + 1 < shown.size())
      ++cursor_;
    if (pressed.pressed(Button::Up) && cursor_ > 0)
      --cursor_;
    if (pressed.pressed(Button::FaceBottom)) {
      selected_.reset();
      cursor_ = 0;
      bottomDirty_ = true;
    }
    if (pressed.pressed(Button::FaceRight) && cursor_ < shown.size()) {
      send(PlayMove{latest_->moves[shown[cursor_]]});
      latest_->yourMove = false; // (until the game says it's yours again)
      selected_.reset();
      cursor_ = 0;
      bottomDirty_ = true;
    }
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

  // A touch: a tap selects the hex (and corner) under it; a drag moves the
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
      if (std::abs(touch.x - touchStart_[0]) + std::abs(touch.y - touchStart_[1]) > 6)
        dragged_ = true;
      if (dragged_ && (dx != 0 || dy != 0)) {
        picture_->pan(static_cast<float>(dx), static_cast<float>(dy));
        bottomDirty_ = true;
      }
      touchLast_ = {touch.x, touch.y};
    } else if (touchWasDown_ && !dragged_) {
      selected_ = picture_->at(touchStart_[0], touchStart_[1]);
      cursor_ = 0;
      bottomDirty_ = true;
    }
    touchWasDown_ = touch.down;
  }

  // Your moves, under the scores: a window of the list round the cursor,
  // the chosen one highlighted
  void drawMoves(Scene::Overlay &overlay) {
    const std::vector<std::size_t> shown = movesShown();
    overlay.rectangle(0, 60, kTopWidth, kTopHeight - 76, {12, 18, 30, 210});
    const char *heading =
        selected_ ? "Your moves here (B: all moves)" : "Your moves (touch the board to narrow)";
    overlay.text(6, 64, heading, {255, 214, 90});
    if (shown.empty()) {
      overlay.text(6, 80, "None here: touch elsewhere, or B for all", {200, 208, 222});
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
                   i == cursor_ ? Scene::Colour{255, 214, 90} : Scene::Colour{200, 208, 222});
    }
  }

  // The bottom screen: the board drawn flat, the selection marked, copied
  // into its framebuffer (240 x 320, a column at a time from the bottom
  // left: the screen is turned on its side; three bytes a pixel, B G R)
  void drawBottom() {
    bottomDirty_ = false;
    const std::vector<Scene::Colour> *drawn = nullptr;
    const ScreenFlow::Screen screen = flow_.screen();
    if (screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Paused) {
      menuPicture_.draw(screen == ScreenFlow::Screen::Title ? titleMenu_ : pauseMenu_);
      drawn = &menuPicture_.pixels();
    } else if (screen == ScreenFlow::Screen::Setup) {
      menuPicture_.draw(menu_);
      drawn = &menuPicture_.pixels();
    } else {
      if (!latest_)
        return;
      // (the map drawn when the game changes, not again each frame of the
      // fade: round 92)
      if (!fading_) {
        picture_->draw(latest_->state);
        if (selected_)
          picture_->mark(*selected_);
      }
      drawn = &picture_->pixels();
    }
    // Fading from the menu to the map as the board comes down: the setup
    // menu's picture (menuPicture_, not drawn again in a game) and the map,
    // mixed as they go to the screen, in whole numbers (no floating point
    // and no buffers of their own: the DS has neither the hardware nor the
    // memory; round 92)
    const float down = descent();
    const std::vector<Scene::Colour> *from = nullptr;
    int weight = 256; // (of the map, out of 256)
    if (fadeFromMenu_ && down < 1 && menuPicture_.pixels().size() == drawn->size()) {
      from = &menuPicture_.pixels();
      weight = static_cast<int>(down * 256);
      bottomDirty_ = true; // (and again next frame, until it's done)
    } else if (fadeFromMenu_ && down >= 1) {
      fadeFromMenu_ = false;
      bottomDirty_ = true; // (once more: the map as the game is now)
    }
    fading_ = from != nullptr;
    const auto &pixels = *drawn;
    auto copying = profile_.time(kProfileOut); // (the picture to the screen)
    // A pixel's colour on its way to the screen: the picture's, or mid-fade
    // the menu's moved `weight` / 256 of the way to it. (Plain arithmetic on
    // the bytes: as a lambda a pixel this took 120 ms a frame on the DS,
    // the copy alone 2 to 6; round 92)
    const Scene::Colour *to = pixels.data();
    const Scene::Colour *mixFrom = from ? from->data() : nullptr;
    const auto colourAt = [to, mixFrom, weight](std::size_t i) -> Scene::Colour {
      const Scene::Colour b = to[i];
      if (!mixFrom)
        return b;
      const Scene::Colour a = mixFrom[i];
      return {static_cast<std::uint8_t>(a.r + (((b.r - a.r) * weight) >> 8)),
              static_cast<std::uint8_t>(a.g + (((b.g - a.g) * weight) >> 8)),
              static_cast<std::uint8_t>(a.b + (((b.b - a.b) * weight) >> 8)), 255};
    };
#if defined(__NDS__)
    // 256 a row (the bitmap is 256 wide), 15-bit colours with bit 15 set
    for (int y = 0; y < kBottomHeight; ++y)
      for (int x = 0; x < kBottomWidth; ++x) {
        const Scene::Colour c = colourAt(static_cast<std::size_t>(y * kBottomWidth + x));
        bottomPixels_[y * 256 + x] =
            static_cast<u16>(RGB15(c.r >> 3, c.g >> 3, c.b >> 3) | BIT(15));
      }
#else
    u16 w = 0, h = 0;
    std::uint8_t *frame = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 320; ++x) {
        const Scene::Colour c = colourAt(static_cast<std::size_t>(y * 320 + x));
        std::uint8_t *at = frame + (static_cast<std::size_t>(x) * 240 + (239 - y)) * 3;
        at[0] = c.b;
        at[1] = c.g;
        at[2] = c.r;
      }
    gfxFlushBuffers();
#endif
  }

  std::optional<Scene::Display> display_;
  std::optional<Scene::Renderer> renderer_;
  // Each tile face (Art.hpp) drawn into a texture from its recipe (kFaceSize a side),
  // once, when the game starts
  void loadFaces() {
    namespace TR = ScratchPadd::TextureRecipe;
    for (int i = 0; i < CosmicHex::Art::kFaces; ++i) {
      const auto recipe = TR::read(CosmicHex::Art::recipeOf(static_cast<CosmicHex::Art::Face>(i)));
      if (!recipe)
        continue; // (the tile shows white: a broken recipe, tested against on the Mac)
      const TR::Image image =
          TR::draw(*recipe, kFaceSize, kFaceSize, TR::Format::Rgba8, {}, kRecipeSmoothing);
      std::vector<Scene::Colour> pixels(static_cast<std::size_t>(kFaceSize * kFaceSize));
      for (std::size_t p = 0; p < pixels.size(); ++p)
        pixels[p] = {image.pixels[p * 4], image.pixels[p * 4 + 1], image.pixels[p * 4 + 2],
                     image.pixels[p * 4 + 3]};
      faces_[static_cast<std::size_t>(i)] =
          renderer_->load(Scene::TextureImage{kFaceSize, kFaceSize, pixels});
    }
    // and the number chips (round 87), half a face's size
    constexpr int kChipSize = kFaceSize / 2;
    for (int n = CosmicHex::Art::kLowestChip; n <= CosmicHex::Art::kHighestChip; ++n) {
      const auto text = CosmicHex::Art::chipRecipeOf(n);
      const auto recipe = TR::read(text);
      if (text.empty() || !recipe)
        continue;
      const TR::Image image =
          TR::draw(*recipe, kChipSize, kChipSize, TR::Format::Rgba8, {}, kRecipeSmoothing);
      std::vector<Scene::Colour> pixels(static_cast<std::size_t>(kChipSize * kChipSize));
      for (std::size_t p = 0; p < pixels.size(); ++p)
        pixels[p] = {image.pixels[p * 4], image.pixels[p * 4 + 1], image.pixels[p * 4 + 2],
                     image.pixels[p * 4 + 3]};
      chips_[static_cast<std::size_t>(n)] =
          renderer_->load(Scene::TextureImage{kChipSize, kChipSize, pixels});
    }
  }

  Scene::Mesh hexTile_, numberChip_, station_, colony_, ship_, stars_, gaps_, highlight_;
  std::array<Scene::Texture, CosmicHex::Art::kFaces> faces_{};
  std::array<Scene::Texture, CosmicHex::Art::kHighestChip + 1> chips_{}; // (by number)
  // Memory, measured (with the counting allocator in the build)
  std::optional<ScratchPadd::MemoryStats::Reader> memory_;
  Clock::time_point memoryAt_;
  std::string memoryLine_, memoryBusiest_;
  // Where each frame's time goes (round 92: the DS's stutter), shown on
  // the -watch builds: the bottom screen, the dice, the top screen (its
  // wait for the refresh included), and that wait
  enum : std::size_t { kProfileBottom, kProfileOut, kProfileDice, kProfileTop, kProfileWait };
  ScratchPadd::FrameProfile profile_{"btm", "out", "dice", "top", "wait"};
  std::string profileLine_;

  // Whether the top screen would look as it did when last drawn: a game's
  // board, nothing new from the table, nothing moving (the pieces, the
  // dice, the board coming down), the camera and the selection the same.
  // (The -watch builds redraw once a second, for their measurements.)
  bool topIsStill() const {
    if (flow_.screen() != ScreenFlow::Screen::Game || !latest_ || descentStarted_ ||
        dice_ != DiceStage::Idle)
      return false;
    if (gameClock_.seconds() - motionStarted_ < kSecondsAnimating)
      return false; // (pieces still moving)
    const std::optional<HexGrid::Hex> selection =
        selected_ ? std::optional<HexGrid::Hex>(selected_->hex) : std::nullopt;
    if (snapshots_ != drawnSnapshots_ || turn_ != drawnTurn_ || selection != drawnSelection_)
      return false;
    return !kWatchAtStart || Clock::now() - drawnAt_ < std::chrono::seconds(1);
  }

  // Waits for the screen's refresh without drawing (the last frame stays)
  static void waitForTheRefresh() {
#if defined(__NDS__)
    swiWaitForVBlank();
#endif
  }

  bool onAMenuScreen() const {
    const ScreenFlow::Screen screen = flow_.screen();
    return screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Setup;
  }

  void showProfile() {
    if (!kWatchAtStart)
      return;
    if (profile_.secondDone()) {
      profileLine_ = profile_.line();
      spdlog::info("frames: {}", profileLine_);
    }
    // (two lines where the screen's narrow)
    for (std::size_t from = 0, row = 0; from < profileLine_.size() && row < 2;
         from += kCharsPerLine, ++row)
      renderer_->overlay().text(4, kTopHeight - 74 + 12 * static_cast<float>(row),
                                profileLine_.substr(from, kCharsPerLine), {240, 200, 120});
  }
  // The title scene
  std::optional<ScratchPadd::TitleScene::TitleScene> title_;
  Clock::time_point titleStarted_;
  // The physics dice
  enum class DiceStage : std::uint8_t { Idle, Rolling, Showing, Pulling };
  static constexpr double kSecondsShowingDice = 0.75;
  // The tray's fixed step: 30 a second on the DS (round 92: a step took
  // about 10 ms there, so 60 a second couldn't keep up; the throws are as
  // fair at 30, dice_fairness_flex_point16 over 20,000), 60 elsewhere. The
  // step decides the throw, so a seed lands differently on the DS.
#if defined(__NDS__)
  static constexpr double kDiceStep = 1.0 / 30;
  static constexpr int kMostDiceSteps = 2;
#else
  static constexpr double kDiceStep = 1.0 / 60;
  static constexpr int kMostDiceSteps = 4;
#endif
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
#if defined(__NDS__)
  u16 *bottomPixels_ = nullptr; // (the bottom screen's bitmap)
#endif
  bool bottomDirty_ = true, touchWasDown_ = false, dragged_ = false;
  std::array<int, 2> touchStart_{}, touchLast_{};
  std::optional<BoardPicture::Spot> selected_;
  // What the top screen last showed (topIsStill): the table's snapshots
  // drawn, the camera's turn and the selected hex, and when
  std::uint64_t snapshots_ = 0, drawnSnapshots_ = 0;
  float drawnTurn_ = 0;
  std::optional<HexGrid::Hex> drawnSelection_;
  Clock::time_point drawnAt_{};
  // Which screen (round 90: ScreenFlow); in a game, watching or playing
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
  bool fading_ = false;       // (mid-fade: the map already drawn)
  bool sliding_ = false;
  int slideWay_ = 1; // (+1: right was pressed, the new board comes in from the right)
  Clock::time_point slideStarted_;
  std::vector<CosmicHex::Hex> gapsTiles_; // (the tiles gaps_ was made for)

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
  void updateGaps(const State &state) {
    std::vector<CosmicHex::Hex> tiles;
    for (const CosmicHex::Planet &planet : state.planets)
      tiles.push_back(planet.hex);
    for (const CosmicHex::ZoneSite &site : state.zone)
      tiles.push_back(site.hex);
    if (tiles == gapsTiles_ && gaps_)
      return;
    if (gaps_)
      renderer_->unload(gaps_);
    gaps_ = renderer_->load(BoardPieces::gapsUnder(tiles).data());
    gapsTiles_ = std::move(tiles);
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
    send(StartGame{.withPerson = play,
                   .players = menu_.players(),
                   .board = menu_.boardItself(),
                   .finishRound = menu_.finishRound(),
                   .seatBonus = menu_.seatBonus()});
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
  while (pmMainLoop())
    swiWaitForVBlank();
  return 0;
#endif
#if !defined(__NDS__)
  gfxInitDefault(); // (before ScreenPadd's Display)
#endif
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
#if !defined(__NDS__)
  gfxExit();
#endif
  return 0;
}
