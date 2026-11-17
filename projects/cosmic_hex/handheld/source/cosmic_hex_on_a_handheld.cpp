// Cosmic Hex on a Nintendo 3DS, in padds: the agreed rules (the rules
// library's agreedRules, on R3), the bots as the simulator plays them,
// fewer iterations a move.
//
//   ScreenPadd (main thread)  draws the board on the top screen with the
//              scene layer (3DS backend, in stereo while the 3D slider is
//              up): hexes in the lab's colours, stations, colonies, ships
//              and the blockade in the players' colours, toon shaded; and on
//              the bottom screen's console each player's points and
//              resources and the last moves in words
//   TablePadd  (own thread)  plays the game (TableGame): the dealer's draws
//              and the bots' moves, one at a time, paced so they can be
//              watched, and sends ScreenPadd each new state
//
// The first build (round 71): four bots, to watch it run and time the bots.
// A: pause or go on   L/R: turn the board   START: quit
#include <3ds.h>
#include <ScratchPadd/Scene/3DS/Display.hpp>

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Handheld/BoardMotion.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>

#include <ScratchPadd/AboutPadd.hpp>
#include <ScratchPadd/AppEnded/AppEnded.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/HandheldControls/ConsolePanel.hpp>
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
namespace Input = ScratchPadd::Input;
using CosmicHex::State;
using namespace CosmicHex::Handheld;
using Clock = std::chrono::steady_clock;

constexpr int kConsoleColumns = 40, kConsoleRows = 30; // the bottom screen's
// How long each move stays on screen before the next, so a game can be
// watched (the dealer's draws go straight on), and how long its slide or
// flip takes
constexpr double kSecondsAMove = 0.6;
constexpr double kSecondsAnimating = 0.35;

// The game as it stands (TablePadd to ScreenPadd), after each move
struct TableSnapshot {
  State state;
  std::vector<std::string> lastMoves; // the newest last
  double lastThinkSeconds = 0;        // the last bot move's search
  bool paused = false;
};

// A (ScreenPadd to TablePadd): pause or go on
struct TogglePause {};

class TablePadd : public ScratchPadd::Base {
public:
  TablePadd(ScratchPadd::System *system)
      : Base(system), game_(TableSettings{.botIterations = 40, .seed = startingSeed()}) {
    receive<TogglePause>([this](const TogglePause &) {
      paused_ = !paused_;
      sendSnapshot();
    });
  }
  std::string name() override { return "TablePadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::duration<double>(kSecondsAMove);
  }
  void starting() override { sendSnapshot(); }
  void repeat() override {
    if (paused_ || game_.over())
      return;
    // The dealer's draws straight through; then one player's move
    for (;;) {
      const bool dealer = CosmicHex::Game::currentPlayer(game_.state()) == GameBots::kDealer;
      const auto started = Clock::now();
      if (!game_.step())
        break; // (over, or a bug: the game stops where it is)
      if (!dealer) {
        lastThinkSeconds_ = std::chrono::duration<double>(Clock::now() - started).count();
        break;
      }
    }
    sendSnapshot();
  }

private:
  // A new game each time the app starts: the seed from the clock
  static std::uint64_t startingSeed() {
    return static_cast<std::uint64_t>(Clock::now().time_since_epoch().count());
  }
  void sendSnapshot() {
    const auto &history = game_.history();
    const std::size_t shown = std::min<std::size_t>(history.size(), 8);
    send(TableSnapshot{.state = game_.state(),
                       .lastMoves = std::vector<std::string>(
                           history.end() - static_cast<long>(shown), history.end()),
                       .lastThinkSeconds = lastThinkSeconds_,
                       .paused = paused_});
  }

  TableGame game_;
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
      // The gaps between the tiles, once: the board's tiles don't move
      if (!gaps_ && renderer_) {
        std::vector<CosmicHex::Hex> tiles;
        for (const CosmicHex::Planet &planet : snapshot.state.planets)
          tiles.push_back(planet.hex);
        for (const CosmicHex::ZoneSite &site : snapshot.state.zone)
          tiles.push_back(site.hex);
        gaps_ = renderer_->load(BoardPieces::gapsUnder(tiles).data());
      }
      drawText();
    });
  }
  std::string name() override { return "ScreenPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }

  void starting() override {
    display_.emplace(); // the top screen
    consoleInit(GFX_BOTTOM, nullptr);
    renderer_.emplace(*display_);
    const BoardPieces pieces;
    hexTile_ = renderer_->load(pieces.hexTile.data());
    station_ = renderer_->load(pieces.station.data());
    colony_ = renderer_->load(pieces.colony.data());
    ship_ = renderer_->load(pieces.ship.data());
    stars_ = renderer_->load(pieces.stars.data());
    loadFaces();
    push([this] { frame(); });
  }

private:
  void frame() {
    if (!aptMainLoop()) {
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
    if (pressed.pressed(Input::Button::FaceRight)) // A
      send(TogglePause{});
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
    renderer_->endFrame(); // waits for the screen's refresh
  }

  // The bottom screen: each player's points and resources, and the last
  // moves in words (written when a move comes, not each frame)
  void drawText() {
    if (!latest_)
      return;
    using ScratchPadd::HandheldControls::printLine;
    constexpr int kColumns = kConsoleColumns;
    const State &state = latest_->state;
    std::printf("\x1b[2J");
    printLine(0, kColumns, "Cosmic Hex  round %d%s", state.round,
              latest_->paused ? "  (paused: A)" : "");
    for (int p = 0; p < state.players; ++p) {
      const auto &r = state.player[static_cast<std::size_t>(p)].resources;
      printLine(2 + p, kColumns, "P%d %2d pts  M%d F%d U%d W%d C%d%s", p + 1,
                CosmicHex::points(state, p), r[0], r[1], r[2], r[3], r[4],
                state.turn == p ? "  <" : "");
    }
    printLine(7, kColumns, "Bot's last think: %.2f s", latest_->lastThinkSeconds);
    int row = 9;
    for (const std::string &move : latest_->lastMoves) {
      // Each move on its own line or two
      for (std::size_t at = 0; at < move.size() && row < kConsoleRows - 1; at += kColumns - 1)
        printLine(row++, kColumns, "%s", move.substr(at, kColumns - 1).c_str());
    }
    if (state.winner >= 0)
      printLine(kConsoleRows - 1, kColumns, "Player %d wins! START: quit", state.winner + 1);
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

  Scene::Mesh hexTile_, station_, colony_, ship_, stars_, gaps_;
  std::array<Scene::Texture, CosmicHex::Art::kFaces> faces_{};
  std::optional<TableSnapshot> latest_;
  BoardMotion motion_;
  Clock::time_point motionStarted_;
  Input::Gamepad lastPad_;
  float turn_ = 0;
};

} // namespace

int main() {
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
  gfxInitDefault();                      // (before ScreenPadd's Display)
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
  gfxExit();
  return 0;
}
