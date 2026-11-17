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
#include <CosmicHex/Handheld/BoardMotion.hpp>
#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/MoveSpots.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>

#include <ScratchPadd/AboutPadd.hpp>
#include <ScratchPadd/AppEnded/AppEnded.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/Input/GamePadd.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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
#if defined(__NDS__)
constexpr int kTopWidth = 256, kTopHeight = 192, kBottomWidth = 256, kBottomHeight = 192;
constexpr std::size_t kCharsPerLine = 31;
constexpr float kLineHeight = 10;
constexpr int kBotIterations = 15;
bool mainLoop() { return pmMainLoop(); }
// The DS has no textures in the scene layer yet: tiles in their face's colour
constexpr bool kTexturedTiles = false;
#else
constexpr int kTopWidth = 400, kTopHeight = 240, kBottomWidth = 320, kBottomHeight = 240;
constexpr std::size_t kCharsPerLine = 64;
constexpr float kLineHeight = 11;
constexpr int kBotIterations = 40;
bool mainLoop() { return aptMainLoop(); }
constexpr bool kTexturedTiles = true;
#endif
namespace Input = ScratchPadd::Input;
using CosmicHex::State;
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
};

// ScreenPadd to TablePadd: start a game (you against three bots, or four
// bots to watch); pause or go on (watching); the move you chose
struct StartGame {
  bool withPerson = false;
};
struct TogglePause {};
struct PlayMove {
  CosmicHex::Action move;
};

class TablePadd : public ScratchPadd::Base {
public:
  TablePadd(ScratchPadd::System *system) : Base(system) {
    receive<StartGame>([this](const StartGame &start) {
      TableSettings settings{.botIterations = kBotIterations, .seed = startingSeed()};
      if (start.withPerson)
        settings.seats[0] = Seat::Person; // (you're player 1)
      game_.emplace(settings);
      sendSnapshot();
    });
    receive<TogglePause>([this](const TogglePause &) {
      paused_ = !paused_;
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
                           .paused = paused_};
    if (game_->waitingForPerson()) {
      snapshot.yourMove = true;
      snapshot.moves = CosmicHex::Game::legalActions(game_->state());
      for (const CosmicHex::Action &move : snapshot.moves)
        snapshot.moveText.push_back(CosmicHex::describe(game_->state(), move));
    }
    send(std::move(snapshot));
  }

  std::optional<TableGame> game_;
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
      motionStarted_ = Clock::now();
      latest_ = snapshot;
      bottomDirty_ = true;
      // The gaps between the tiles, once: the board's tiles don't move
      if (!gaps_ && renderer_) {
        std::vector<CosmicHex::Hex> tiles;
        for (const CosmicHex::Planet &planet : snapshot.state.planets)
          tiles.push_back(planet.hex);
        for (const CosmicHex::ZoneSite &site : snapshot.state.zone)
          tiles.push_back(site.hex);
        gaps_ = renderer_->load(BoardPieces::gapsUnder(tiles).data());
      }
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
    picture_.emplace(kBottomWidth, kBottomHeight);
    renderer_.emplace(*display_);
    const BoardPieces pieces;
    hexTile_ = renderer_->load(pieces.hexTile.data());
    station_ = renderer_->load(pieces.station.data());
    colony_ = renderer_->load(pieces.colony.data());
    ship_ = renderer_->load(pieces.ship.data());
    stars_ = renderer_->load(pieces.stars.data());
    highlight_ = renderer_->load(pieces.highlight.data());
    if (kTexturedTiles)
      loadFaces();
    // The build that watches by itself (COSMIC_HEX_WATCH_AT_START: the
    // -watch apps), for unattended runs in an emulator or on a console
    if (kWatchAtStart) {
      mode_ = Mode::Watching;
      send(StartGame{.withPerson = false});
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
    if (pressed.pressed(Input::Button::Start)) {
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
    if (bottomDirty_)
      drawBottom();
    // L and R turn the board while held
    if (pad.isHeld(Input::Button::L))
      turn_ -= 0.02f;
    if (pad.isHeld(Input::Button::R))
      turn_ += 0.02f;
    drawBoard();
    push([this] { frame(); });
  }

  // The board from above and in front, turned by L and R (looking a little
  // in front of the centre, so the near hexes stay on screen)
  void drawBoard() {
    const float distance = 9.5f, height = 9.5f;
    const Scene::Camera camera{
        .eye = {distance * std::sin(turn_), height, distance * std::cos(turn_)},
        .target = {1.2f * std::sin(turn_), 0, 1.2f * std::cos(turn_)},
        .up = {0, 1, 0},
        .fieldOfView = 50,
        .nearPlane = 1,
        .farPlane = 60};
    static const Scene::Environment kEnvironment = [] {
      Scene::Environment environment;
      environment.background = BoardColours::kSpace;
      environment.ambient = {120, 120, 130};
      environment.addLight({.direction = {-0.4f, -1, -0.5f}, .colour = {200, 200, 190}});
      return environment;
    }();
    renderer_->beginFrame(camera, kEnvironment);
    renderer_->draw(stars_, Scene::Transform::identity(),
                    {.shading = Scene::Material::Shading::Unlit});
    // A dim, see-through grey in the gaps between the tiles (round 80: dimmer)
    if (gaps_)
      renderer_->draw(
          gaps_, Scene::Transform::identity(),
          {.shading = Scene::Material::Shading::Unlit, .colour = {150, 160, 180}, .alpha = 4});
    if (latest_) {
      const State &state = latest_->state;
      // How far through the latest move's animation (0 to 1)
      const float t = static_cast<float>(
          std::chrono::duration<double>(Clock::now() - motionStarted_).count() / kSecondsAnimating);
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
          if (!kTexturedTiles)
            return Scene::Material{.shading = Scene::Material::Shading::Toon,
                                   .colour = BoardColours::ofFace(face)};
          return Scene::Material{.shading = Scene::Material::Shading::Toon,
                                 .texture = faces_[static_cast<std::size_t>(face)]};
        };
        if (!turn) {
          renderer_->draw(hexTile_, BoardPieces::onHex(hex), showing(up));
          return;
        }
        const float half = 1.5707963f;
        const bool upSide = *turn > half;
        const auto lift = Scene::Transform::translation({0, 0.6f * std::sin(*turn), 0});
        const auto spin = Scene::Transform::rotation({1, 0, 0}, upSide ? *turn - 2 * half : *turn);
        renderer_->draw(hexTile_, BoardPieces::onHex(hex) * lift * spin,
                        showing(upSide ? up : down));
      };
      for (std::size_t i = 0; i < state.planets.size(); ++i)
        drawTile(state.planets[i].hex, motion_.planetTurn(i, t),
                 CosmicHex::Art::Face::PlanetFaceDown, faceOf(state.planets[i]));
      for (std::size_t i = 0; i < state.zone.size(); ++i)
        drawTile(state.zone[i].hex, motion_.siteTurn(i, t), CosmicHex::Art::Face::FindFaceDown,
                 faceOf(state.zone[i]));
      for (const CosmicHex::Station &station : state.stations)
        renderer_->draw(station.colony ? colony_ : station_, BoardPieces::onCorner(station.at),
                        toon(BoardColours::ofPlayer(station.owner)));
      // Ships, sliding from where they were
      for (std::size_t i = 0; i < state.ships.size(); ++i)
        renderer_->draw(ship_, Scene::Transform::translation(motion_.shipAt(i, t)),
                        toon(BoardColours::ofPlayer(state.ships[i].owner)));
      if (state.blockade >= 0)
        renderer_->draw(
            ship_, BoardPieces::onHex(state.planets[static_cast<std::size_t>(state.blockade)].hex),
            toon(BoardColours::kBlockade));
    }
    drawText();
    // The selected hex, ringed on the 3D board too
    if (selected_)
      renderer_->draw(highlight_, BoardPieces::onHex(selected_->hex),
                      {.shading = Scene::Material::Shading::Unlit, .colour = {255, 214, 90}});
    renderer_->endFrame(); // waits for the screen's refresh
  }

  // The bottom screen: each player's points and resources, and the last
  // moves in words (written when a move comes, not each frame)
  // Over the top screen: the round, each player's points and resources in
  // their colour, and the last move in words; before a game, the choice; on
  // your move, the moves you can make
  void drawText() {
    Scene::Overlay &overlay = renderer_->overlay();
    if (mode_ == Mode::Choosing) {
      const float left = kTopWidth / 2.0f - 100;
      overlay.text(left + 30, kTopHeight / 2.0f - 30, "Cosmic Hex", {226, 232, 244}, 2);
      overlay.text(left, kTopHeight / 2.0f + 10, "A: play against three bots", {255, 214, 90});
      overlay.text(left, kTopHeight / 2.0f + 22, "B: watch four bots play", {200, 208, 222});
      return;
    }
    if (!latest_)
      return;
    const State &state = latest_->state;
    char line[64];
    std::snprintf(line, sizeof line, "Round %d%s", state.round,
                  latest_->paused ? "  (paused: A)" : "");
    overlay.text(4, 4, line, {226, 232, 244});
    for (int p = 0; p < state.players; ++p) {
      const auto &r = state.player[static_cast<std::size_t>(p)].resources;
      std::snprintf(line, sizeof line, "%sP%d %2d  M%d F%d U%d W%d C%d",
                    state.turn == p ? ">" : " ", p + 1, CosmicHex::points(state, p), r[0], r[1],
                    r[2], r[3], r[4]);
      overlay.text(4, 16 + 10 * static_cast<float>(p), line, BoardColours::ofPlayer(p));
    }
    if (!latest_->lastMoves.empty())
      overlay.text(4, kTopHeight - 14, latest_->lastMoves.back().substr(0, kCharsPerLine),
                   {200, 208, 222});
    if (mode_ == Mode::Playing && latest_->yourMove)
      drawMoves(overlay);
    if (state.winner >= 0) {
      std::snprintf(line, sizeof line, "Player %d wins!  START: quit", state.winner + 1);
      overlay.text(kTopWidth / 2.0f - 90, kTopHeight / 2.0f - 10, line, {255, 214, 90}, 1.5f);
    }
  }

  // The buttons. Before a game: A to play against three bots, B to watch
  // four. Watching: A pauses or goes on. Playing, on your move: up and down
  // choose from the list (the moves at the touched spot, or all of them), A
  // makes it, B shows them all again
  void handleButtons(const Input::Edges &pressed) {
    using Input::Button;
    if (mode_ == Mode::Choosing) {
      if (pressed.pressed(Button::FaceRight) || pressed.pressed(Button::FaceBottom)) {
        const bool play = pressed.pressed(Button::FaceRight);
        mode_ = play ? Mode::Playing : Mode::Watching;
        send(StartGame{.withPerson = play});
      }
      return;
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
    if (!latest_)
      return;
    picture_->draw(latest_->state);
    if (selected_)
      picture_->mark(*selected_);
    const auto &pixels = picture_->pixels();
#if defined(__NDS__)
    // 256 a row (the bitmap is 256 wide), 15-bit colours with bit 15 set
    for (int y = 0; y < kBottomHeight; ++y)
      for (int x = 0; x < kBottomWidth; ++x) {
        const Scene::Colour c = pixels[static_cast<std::size_t>(y * kBottomWidth + x)];
        bottomPixels_[y * 256 + x] =
            static_cast<u16>(RGB15(c.r >> 3, c.g >> 3, c.b >> 3) | BIT(15));
      }
#else
    u16 w = 0, h = 0;
    std::uint8_t *frame = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 320; ++x) {
        const Scene::Colour c = pixels[static_cast<std::size_t>(y * 320 + x)];
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
  // Each tile face (Art.hpp) drawn into a 64 x 64 texture from its recipe,
  // once, when the game starts
  void loadFaces() {
    namespace TR = ScratchPadd::TextureRecipe;
    for (int i = 0; i < CosmicHex::Art::kFaces; ++i) {
      const auto recipe = TR::read(CosmicHex::Art::recipeOf(static_cast<CosmicHex::Art::Face>(i)));
      if (!recipe)
        continue; // (the tile shows white: a broken recipe, tested against on the Mac)
      const TR::Image image = TR::draw(*recipe, 64, 64, TR::Format::Rgba8);
      std::vector<Scene::Colour> pixels(64 * 64);
      for (std::size_t p = 0; p < pixels.size(); ++p)
        pixels[p] = {image.pixels[p * 4], image.pixels[p * 4 + 1], image.pixels[p * 4 + 2],
                     image.pixels[p * 4 + 3]};
      faces_[static_cast<std::size_t>(i)] = renderer_->load(Scene::TextureImage{64, 64, pixels});
    }
  }

  Scene::Mesh hexTile_, station_, colony_, ship_, stars_, gaps_, highlight_;
  std::array<Scene::Texture, CosmicHex::Art::kFaces> faces_{};
  std::optional<TableSnapshot> latest_;
  BoardMotion motion_;
  Clock::time_point motionStarted_;
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
  // Before a game, watching one, or playing in one; the list's cursor
  enum class Mode : std::uint8_t { Choosing, Watching, Playing };
  Mode mode_ = Mode::Choosing;
  std::size_t cursor_ = 0;
};

} // namespace

int main() {
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
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
