// ScreenPadd's stage: the title scene, the setup's board, and the game's
// board with its pieces (what shows over the board: ShowsOverBoard.cpp)
#include "ScreenPadd.hpp"

#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/BoardView.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <ScratchPadd/Motion/Motion.hpp>

#include <chrono>
#include <cmath>
#include <utility>

namespace CosmicHex::Handheld::App {

// The stage, by the screen: the title scene (the title, the settings, and
// once a game's won); the board chosen (setup); or the game's board, unless
// nothing on it has changed. (The stage's drawing, its wait for the
// refresh included, is timed)
void ScreenPadd::drawStage() {
  auto drawing = std::optional<ScratchPadd::FrameProfile::Timing>();
  drawing.emplace(profile_, kProfileTop);
  const ScreenFlow::Screen screen = flow_.screen();
  if (kTitleScene && (screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Won ||
                      screen == ScreenFlow::Screen::Settings)) {
    drawTitle();
    return;
  }
  // (L and R don't turn the 3D board: the user's choice)
  if (screen == ScreenFlow::Screen::Setup) {
    drawSetup();
  } else if (kThisPlatform.holdStillFrames && topIsStill()) {
    // Nothing on the stage has changed: send nothing and let the
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
}

// The title scene, turning; its words over it: before a game nothing (the
// menu's on the touch panel), after one who won
void ScreenPadd::drawTitle() {
  renderer_->beginFrame(title_->camera(), title_->environment());
  title_->draw(*renderer_, static_cast<float>(secondsSince(titleStarted_)));
  // Fading in from black, once the start's work is done
  if (const float in = titleFadedIn(); in < 1)
    renderer_->overlay().rectangle(
        0, 0, kStageWidth, kStageHeight,
        {0, 0, 0, static_cast<std::uint8_t>(std::lround((1 - in) * 255))});
  if (flow_.screen() == ScreenFlow::Screen::Won && latest_) {
    const int winner = latest_->state.winner;
    Scene::Overlay &overlay = renderer_->overlay();
    overlay.textCentred(kStageWidth / 2.0f, kStageHeight - 34,
                        (mode_ == Mode::Playing && winner == 0 ? std::string("You win")
                                                               : names_[winner] + " wins") +
                            "!",
                        Palette::kHighlight, 2);
    overlay.text(kStageWidth / 2.0f - 60, kStageHeight - 16, "A: to the title",
                 Palette::kSoftWords);
  }
  showProfile();
  {
    auto timing = profile_.time(kProfileWait);
    renderer_->endFrame();
  }
}

// How far the title has faded in from black, 0 to 1, over
// kSecondsTitleFadeIn from when the start's work was done
float ScreenPadd::titleFadedIn() const {
  return std::min(1.0f, static_cast<float>(secondsSince(titleStarted_) / kSecondsTitleFadeIn));
}

// The board's light: the game's (a lamp above and to one side, toon
// bands), or, `flat` of the way to it, bright and even (the setup's: the
// board seen from above looks like the flat map, the user's)
Scene::Environment ScreenPadd::boardEnvironment(float flat) {
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
float ScreenPadd::descent() const {
  if (!descentStarted_)
    return 1;
  return slideAcross(static_cast<float>(secondsSince(*descentStarted_)));
}

// The game's board (or none yet), with the dice, the shows and words over
// it and the selection
void ScreenPadd::drawBoard(const State *shown) {
  // (coming down from above at the start of a game: the user's idea)
  const float down = descent();
  if (down >= 1 && descentStarted_) {
    descentStarted_.reset();
    // (down: a moment to look at the board, the game still held, then who
    // goes first; A goes on at once)
    if (flow_.screen() == ScreenFlow::Screen::Game)
      lookingSince_ = Clock::now();
  }
  const Scene::Camera camera = cameraBetween(topDownCamera(), gameCamera(turn_), down);
  renderer_->beginFrame(camera, boardEnvironment(1 - down));
  if (!shown && descentStarted_)
    shown = &menus_.preview(); // (the game's first state on its way: the same board)
  // (not under another player's panel once it covers the stage: the
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
  drawShowsOverBoard(TopFrame{*renderer_, renderer_->overlay(), camera,
                              static_cast<float>(kStageWidth), static_cast<float>(kStageHeight),
                              pictures_, names_, showsNow()});
  // The touched corner (or the blockade's hex), ringed on the 3D board too
  if (selected_) {
    const Scene::Material marked{.shading = Scene::Material::Shading::Unlit,
                                 .colour = Palette::kHighlight};
    if (choosingAHex())
      renderer_->draw(hexRing_, BoardPieces::onHex(selected_->hex), marked);
    else
      renderer_->draw(cornerRing_, BoardPieces::onCorner(selected_->corner), marked);
  }
  drawSelectedShip();
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
void ScreenPadd::drawSetup() {
  const Scene::Camera camera = topDownCamera();
  renderer_->beginFrame(camera, boardEnvironment(1));
  renderer_->draw(stars_, Scene::Transform::identity(),
                  {.shading = Scene::Material::Shading::Unlit});
  const float through = menus_.slideThrough();
  if (through >= 1) {
    drawGaps(menus_.preview(), Scene::Transform::identity());
    drawPieces(menus_.preview(), Scene::Transform::identity());
  } else {
    // One stage's width at the board's distance, across the stage
    const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
    const Scene::Vector3 right = forward.cross(camera.up).normalised();
    const float away = (camera.target - camera.eye).length();
    const float wide = 2 * away * std::tan(camera.fieldOfView * 3.14159265f / 360) *
                       static_cast<float>(kStageWidth) / static_cast<float>(kStageHeight);
    const auto sign =
        static_cast<float>(menus_.slideWay()); // (+1: the new one comes from the right)
    const auto before = Scene::Transform::translation(right * (-sign * through * wide));
    const auto after = Scene::Transform::translation(right * (sign * (1 - through) * wide));
    drawGaps(menus_.previewBefore(), before);
    drawPieces(menus_.previewBefore(), before);
    drawGaps(menus_.preview(), after);
    drawPieces(menus_.preview(), after);
  }
  // The board's name over it, and anything to know about it (the user's:
  // the four-cluster boards' longer games)
  Scene::Overlay &overlay = renderer_->overlay();
  const StartMenu &setup = menus_.setup();
  overlay.text(4, 4, setup.boardName(), Palette::kWords);
  if (!setup.boardNote().empty())
    overlay.text(4, 16, setup.boardNote().substr(0, kCharsPerLine), Palette::kHighlight);
  showProfile();
  {
    auto timing = profile_.time(kProfileWait);
    renderer_->endFrame();
  }
}

// A dim, see-through grey in the gaps between the tiles (dimmer),
// made when a board is first drawn (another game, or the setup's choice)
void ScreenPadd::drawGaps(const State &state, const Scene::Transform &placement) {
  if (const Scene::Mesh gaps = gapsFor(state))
    renderer_->draw(
        gaps, placement,
        {.shading = Scene::Material::Shading::Unlit, .colour = {150, 160, 180}, .alpha = 4});
}

// The gaps between a state's tiles, made again when its tiles aren't the
// ones they were made for
Scene::Mesh ScreenPadd::gapsFor(const State &state) {
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

// A ship at `pose` in `colour`, its engine's flame when it burns
// (swivelled, flickering a little, from the middle of its tail)
void ScreenPadd::drawShip(const ShipPose &pose, const Scene::Transform &placement,
                          Scene::Colour colour, std::uint8_t alpha) {
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

// A board's tiles, chips, stations and ships, placed by `placement`
void ScreenPadd::drawPieces(const State &state, const Scene::Transform &placement) {
  // How far through the latest move's animation (0 to 1)
  const float t = static_cast<float>((gameClock_.seconds() - motionStarted_) / kSecondsAnimating);
  drawTiles(state, placement, t);
  drawStations(state, placement, t);
  drawShips(state, placement, t);
}

// A tile with its face (Art.hpp); turning over, lifted a little,
// its face-down side first, then its face, about the axis across
void ScreenPadd::drawTile(CosmicHex::Hex hex, std::optional<float> turn, CosmicHex::Art::Face down,
                          CosmicHex::Art::Face up, const Scene::Transform &placement) {
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
}

// The planets and the zone's finds, `t` of the way through the latest
// move's animation
void ScreenPadd::drawTiles(const State &state, const Scene::Transform &placement, float t) {
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    // (one turned over is shown big first: face down here till it's
    // back, then simply face up: Discovery)
    const CosmicHex::Hex hex = state.planets[i].hex;
    if (discovery_.stillToShow(hex, showsNow())) {
      if (!discovery_.flying(hex, showsNow()))
        drawTile(hex, std::nullopt, CosmicHex::Art::Face::PlanetFaceDown,
                 CosmicHex::Art::Face::PlanetFaceDown, placement);
      continue;
    }
    const bool shownBig = discovery_.shownBig(hex);
    const auto turn = shownBig ? std::nullopt : motion_.planetTurn(i, t);
    drawTile(hex, turn, CosmicHex::Art::Face::PlanetFaceDown, faceOf(state.planets[i]), placement);
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
      renderer_->draw(pictures_.numberChip,
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
                 CosmicHex::Art::Face::FindFaceDown, placement);
    } else
      drawTile(hex, discovery_.shownBig(hex) ? std::nullopt : motion_.siteTurn(i, t),
               CosmicHex::Art::Face::FindFaceDown, faceOf(state.zone[i]), placement);
  }
}

// Stations and colonies, one just built dropping in with a puff of
// dust (BoardMotion's landing)
void ScreenPadd::drawStations(const State &state, const Scene::Transform &placement, float t) {
  const auto toon = [](Scene::Colour colour) {
    return Scene::Material{.shading = Scene::Material::Shading::Toon, .colour = colour};
  };
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
}

// Ships, flying from where they were, nose first, their engines
// burning as they speed up (BoardMotion); the winner's flying round above
// it instead, once someone's won; and the blockade, swooping to its planet
void ScreenPadd::drawShips(const State &state, const Scene::Transform &placement, float t) {
  for (std::size_t i = 0; i < state.ships.size(); ++i)
    if (!celebration_ || state.ships[i].owner != state.winner)
      drawShip(motion_.ship(i, t), placement, BoardColours::ofPlayer(state.ships[i].owner));
  if (celebration_ && state.winner >= 0)
    for (std::size_t i = 0; i < celebration_->ships(); ++i)
      drawShip(celebration_->pose(i, showsNow()), placement, BoardColours::ofPlayer(state.winner),
               static_cast<std::uint8_t>(31 * celebration_->fadedIn(i, showsNow())));
  if (const auto pose = motion_.blockade(t))
    drawShip(*pose, placement, BoardColours::kBlockade);
}

// Where you may choose on the board: a ring at each corner a touched ship
// can reach, fuel's orange where the move takes a fuel, or where a build
// may go; and the board's pointer (the user's: a white chevron)
void ScreenPadd::drawSelectedShip() {
  if (!bottomScreen_ || !latest_)
    return;
  const std::vector<CosmicHex::Corner> fuel = bottomScreen_->placingCornersWithFuel();
  for (const CosmicHex::Corner &corner : bottomScreen_->placingCorners()) {
    const bool withFuel = std::ranges::find(fuel, corner) != fuel.end();
    renderer_->draw(cornerRing_, BoardPieces::onCorner(corner),
                    {.shading = Scene::Material::Shading::Unlit,
                     .colour = withFuel ? kFuelOrange : Palette::kHighlight});
  }
  // The pointer (CornerPointer): turning and bobbing over the corner in
  // focus, as shown as it's faded in, gliding between corners
  const auto look = pointer_.at(showsNow());
  if (!look)
    return;
  const auto t = static_cast<float>(showsNow());
  constexpr float kTurn = 6.2831853f; // (a whole turn, radians)
  const float bob = 0.06f * std::sin(t * kTurn * 0.8f);
  const Scene::Transform from = BoardPieces::onCorner(look->from),
                         to = BoardPieces::onCorner(look->to);
  const auto between = [&](int axis) {
    return from.m[static_cast<std::size_t>(axis)][3] +
           (to.m[static_cast<std::size_t>(axis)][3] - from.m[static_cast<std::size_t>(axis)][3]) *
               look->along;
  };
  renderer_->draw(chevron_,
                  Scene::Transform::translation({between(0), between(1) + 0.8f + bob, between(2)}) *
                      Scene::Transform::rotation({0, 1, 0}, t * 2.2f),
                  {.shading = Scene::Material::Shading::Unlit,
                   .colour = {255, 255, 255},
                   .alpha = static_cast<std::uint8_t>(std::lround(31 * look->shown)),
                   .doubleSided = true});
}

} // namespace CosmicHex::Handheld::App
