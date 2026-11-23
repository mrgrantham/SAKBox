#include "ScreenPadd.hpp"

#include <CosmicHex/Dice/DiceTray.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <ScratchPadd/MemoryStats.hpp>
#include <ScratchPadd/StartupSplash/StartupSplash.hpp>
#include <ScratchPadd/Storage/Messages.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>
// The title screen's rocket, as the build cooked it for this console
#if defined(__NDS__)
#include "rocket_ds.h"
#else
#include "rocket_3ds.h"
#endif

#include <algorithm>
#include <memory>

namespace CosmicHex::Handheld::App {

namespace Dice = CosmicHex::Dice;

ScreenPadd::ScreenPadd(ScratchPadd::System *system) : Base(system) {
  paceAnimations();
  hookShows();
  receive<TableSnapshot>([this](const TableSnapshot &snapshot) { onSnapshot(snapshot); });
}

void ScreenPadd::starting() {
  const Clock::time_point startedAt = Clock::now();
  display_.emplace(); // the stage (the top screen)
  // The touch panel (the bottom screen): the flat board (BoardPicture),
  // written straight into its one framebuffer when it changes
  touchScreen_.emplace();
  picture_.emplace(kTouchWidth, kTouchHeight, kThisPlatform.recipeSmoothing);
  picture_->setZoomBudget(kThisPlatform.boardZoomBytes);
  bottomScreen_.emplace(kTouchWidth, kTouchHeight);
  shows_.setBottomScreen(&*bottomScreen_);
#if !defined(NDEBUG)
  // (debug builds: the list of moves, behind a button, off at first)
  bottomScreen_->setMoveListButton(true);
#endif
  touchCanvas_.emplace(kTouchWidth, kTouchHeight);
  renderer_.emplace(*display_);
  loadPieces();
  // The title scene: the 3D title, the rocket, the night sky
  // (fewer stars on the DS: each is two of its 2048 polygons)
  title_.emplace(*renderer_, Assets::kRocket,
                 ScratchPadd::TitleScene::Settings{.title = "COSMIC HEX",
                                                   .aspect = static_cast<float>(kStageWidth) /
                                                             static_cast<float>(kStageHeight),
                                                   .stars = kThisPlatform.titleStars});
  // Everything drawn from recipes for the game ahead (the map's tile
  // faces and number chips, the touch panel's cards and icons, the stage's
  // faces, chips and cards), now, while the screens are dark,
  // then the title fades in (the user's: drawn a picture a
  // frame during the title, up to a tenth of a second each on a DS, they
  // made the rocket stutter for its first turn). The map's faces first:
  // the stage's are made from them where they're no smaller.
  // (behind a splash, fading in and out while it runs)
  ScratchPadd::StartupSplash splash(*renderer_, {.width = kStageWidth,
                                                 .height = kStageHeight,
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
    loadFacePictures(*renderer_, kThisPlatform, &*picture_, pictures_);
    loadCardPictures(*renderer_, kThisPlatform, pictures_, closeUps_);
  });
  titleStarted_ = Clock::now();
  // (how long the start took, shown on the -watch builds)
  startSeconds_ = secondsSince(startedAt);
  // The build that watches by itself (COSMIC_HEX_WATCH_AT_START: the
  // -watch apps), for unattended runs in an emulator or on a console
  if (kWatchAtStart) {
    menus_.chooseWatching();
    apply(flow_.on(ScreenFlow::Event::Play));
    apply(flow_.on(ScreenFlow::Event::Start));
  }
  // (the self-playing build: a game you play, played by the script)
  if (kScriptedPlay) {
    apply(flow_.on(ScreenFlow::Event::Play));
    apply(flow_.on(ScreenFlow::Event::Start));
  }
  push([this] { frame(); });
}

// The board's pieces, the dice and the pictures they're drawn with, loaded
// into the renderer
void ScreenPadd::loadPieces() {
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
  pictures_.hexRing = hexRing_;
  cornerRing_ = renderer_->load(pieces.cornerRing.data());
  chevron_ = renderer_->load(pieces.chevron.data());
  die_ = renderer_->load(DicePieces(static_cast<float>(Dice::DiceTray::kDieSize)).die.data());
  pictures_.die = die_;
  std::array<Scene::Colour, 5> sides{};
  for (int r = 0; r < CosmicHex::kResources; ++r)
    sides[static_cast<std::size_t>(r)] =
        BoardColours::ofKind(static_cast<CosmicHex::PlanetKind>(r));
  pictures_.homeDie = renderer_->load(homeDie(sides, Palette::kHighlight).data());
}

// Each frame: the pad and the stylus, the shows moved on, the touch panel
// drawn when it's changed, and the stage
void ScreenPadd::frame() {
  if (!ConsoleScreens::keepRunning()) {
    system_->stop();
    return;
  }
  shows_.setNow(Clock::now());
  shows_.setInGame(flow_.screen() == ScreenFlow::Screen::Game);
  Input::Gamepad pad = currentState<Input::Gamepad>().value;
  if (kScriptedPlay)
    playScripted(pad);
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
  // Holding X, R and L zoom the map in and out (the user's); without X,
  // L and R are left and right on a choosing screen
  const bool zooming = pad.isHeld(Input::Button::FaceTop);
  zoomMap(pressed, zooming);
  handleButtons(pressed, zooming);
  handleTouch(scripted(pad.touch));
  followTouchPanel();
  profile_.frameStarted();
  {
    // (the shows moved on: the dice stepped among them)
    auto timing = profile_.time(kProfileDice);
    shows_.setInGame(flow_.screen() == ScreenFlow::Screen::Game);
    shows_.frame();
  }
  updateShows();
  if (shows_.takeTouchChanged())
    touchDirty_ = true;
  if (touchDirty_) {
    auto timing = profile_.time(kProfileBottom);
    drawTouchPanel();
  }
  memory_.update();
  prepareMap();
  drawStage();
  profile_.frameEnded();
  push([this] { frame(); });
}

void ScreenPadd::zoomMap(const Input::Edges &pressed, bool zooming) {
  if (zooming && pressed.pressed(Input::Button::R)) {
    picture_->setZoom(picture_->zoom() + 1);
    touchDirty_ = true;
  }
  if (zooming && pressed.pressed(Input::Button::L)) {
    picture_->setZoom(picture_->zoom() - 1);
    touchDirty_ = true;
  }
}

// A pressed button's glow (drawn again while it fades), and a tapped
// button's message, for a second and a half
void ScreenPadd::followTouchPanel() {
  if (flow_.screen() == ScreenFlow::Screen::Settings && menus_.settingsPressing(now()))
    touchDirty_ = true; // (a key's glow fading)
  if (bottomScreen_) {
    bottomScreen_->setTime(secondsSince(startedAt_), showsNow());
    if (bottomScreen_->pressing() || pressDrawn_)
      touchDirty_ = true;
    pressDrawn_ = bottomScreen_->pressing(); // (and once more, to clear it)
  }
  if (bottomScreen_ && bottomScreen_->showingFeedback()) {
    if (!feedbackSince_) {
      feedbackSince_ = Clock::now();
    } else if (Clock::now() - *feedbackSince_ > std::chrono::milliseconds(1500)) {
      bottomScreen_->clearFeedback();
      feedbackSince_.reset();
      touchDirty_ = true;
    }
  }
}

// Each frame, after the shows moved on (GameShows): the pointer, the
// winner's celebration's end
void ScreenPadd::updateShows() {
  updatePointer();
  updateCelebration();
}

// The map, sharpened (the map's faces at the whole board's size, and the
// touch panel's pictures, are drawn at the start: starting())
void ScreenPadd::prepareMap() {
  // (what memory's left, where it runs out: the zoom's sharp tiles leave
  // the game its share)
  if (const ScratchPadd::MemoryStats::Heap heap = ScratchPadd::MemoryStats::heap(); heap.roomKnown)
    picture_->setMemoryRoom(heap.room, kThisPlatform.boardZoomReserve);
  if (flow_.screen() == ScreenFlow::Screen::Game && !picture_->ready()) {
    // Zoomed in or out: the map shows the whole board's faces scaled at
    // once, and sharpens a face a frame
    auto timing = profile_.time(kProfileBottom);
    picture_->prepare();
    touchDirty_ = true;
  }
}

// A file kept on the card (the settings': your name, the animations)
void ScreenPadd::save(const char *file, std::string_view bytes) {
  sendTo<ScratchPadd::Storage::StoragePadd>(ScratchPadd::Storage::SaveFile{
      .path = file,
      .bytes = std::make_shared<const ScratchPadd::Storage::Bytes>(bytes.begin(), bytes.end())});
}

// The board's moves, the dice and the shows at the setting's pace
void ScreenPadd::paceAnimations() {
  shows_.setNow(Clock::now());
  shows_.setAnimations(menus_.animations());
}

} // namespace CosmicHex::Handheld::App
