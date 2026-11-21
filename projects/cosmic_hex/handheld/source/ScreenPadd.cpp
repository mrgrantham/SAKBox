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
  // Holding X, R and L zoom the map in and out (the user's); without X,
  // L and R are left and right on a choosing screen
  const bool zooming = pad.isHeld(Input::Button::FaceTop);
  zoomMap(pressed, zooming);
  handleButtons(pressed, zooming);
  handleTouch(scripted(pad.touch));
  followInfoView();
  followTouchPanel();
  profile_.frameStarted();
  updateStart();
  if (touchDirty_) {
    auto timing = profile_.time(kProfileBottom);
    drawTouchPanel();
  }
  {
    auto timing = profile_.time(kProfileDice);
    throwDice();
  }
  memory_.update();
  updateShows();
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

// An info view opened or closed (Show resources): the game held while
// it's open, and let go when it closes, if the view was what held it
void ScreenPadd::followInfoView() {
  if (!bottomScreen_ || bottomScreen_->info() == shownInfo_)
    return;
  // (Show resources: its cards in as it opens, back off as it closes)
  if (bottomScreen_->info() == BottomScreen::Info::Resources)
    cards_.open(showsNow());
  else if (shownInfo_ == BottomScreen::Info::Resources)
    cards_.close(showsNow());
  // (another player's panel: down as it opens, up as it closes)
  if (bottomScreen_->info() == BottomScreen::Info::Player) {
    others_.open(bottomScreen_->infoPlayer(), showsNow());
    othersShown_ = bottomScreen_->infoPlayer();
  } else if (shownInfo_ == BottomScreen::Info::Player)
    others_.close(showsNow());
  shownInfo_ = bottomScreen_->info();
  if (flow_.screen() == ScreenFlow::Screen::Game)
    holdFor(Hold::Info, shownInfo_ != BottomScreen::Info::None);
}

// A pressed button's glow (drawn again while it fades), another player
// chosen while their panel's open, and a tapped button's message, for a
// second and a half
void ScreenPadd::followTouchPanel() {
  if (flow_.screen() == ScreenFlow::Screen::Settings && menus_.settingsPressing(now()))
    touchDirty_ = true; // (a key's glow fading)
  if (bottomScreen_) {
    bottomScreen_->setTime(secondsSince(startedAt_));
    if (bottomScreen_->pressing() || pressDrawn_)
      touchDirty_ = true;
    pressDrawn_ = bottomScreen_->pressing(); // (and once more, to clear it)
  }
  // (another player chosen while the panel's open: their view)
  // (the next player round the table comes in from the right, the one
  // before from the left)
  if (bottomScreen_ && shownInfo_ == BottomScreen::Info::Player && latest_) {
    const int which = bottomScreen_->infoPlayer(), players = latest_->state.players;
    const int ahead = othersShown_ < 0 ? 1 : (which - othersShown_ + players) % players;
    others_.choose(which, showsNow(), ahead <= players / 2 ? 1 : -1);
    othersShown_ = which;
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

// The start of a game: animations skipped when they're off, the board
// looked at, then who goes first
void ScreenPadd::updateStart() {
  if (menus_.animations() == Animations::Off && skippable())
    skipAnimations();
  if (lookingSince_ && secondsSince(*lookingSince_) >= kSecondsLookingAtBoard)
    startFirstRoll();
  firstRoll_.update(showsNow());
  if (firstRoll_.done(showsNow()))
    endFirstRoll();
}

// Each frame: what shows over the board moved on
void ScreenPadd::updateShows() {
  updateDiscovery();
  updateHomePay();
  updateRollPay();
  updateCardPlay();
  updatePick();
  updateHand();
  updatePointer();
  updateCelebration();
  // The two cards dealt, drawn sharp before they're thrown in (on a DS
  // about a quarter of a second, while the screen is still)
  if (dealt_.needsCloseUp()) {
    closeUpCards(dealt_.cards());
    dealt_.closeUpDrawn(showsNow());
  }
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

void ScreenPadd::holdFor(Hold why, bool on) {
  const auto bit = static_cast<std::uint8_t>(why);
  const std::uint8_t before = holds_;
  holds_ = on ? static_cast<std::uint8_t>(holds_ | bit) : static_cast<std::uint8_t>(holds_ & ~bit);
  if ((before == 0) != (holds_ == 0))
    sendHold();
}

void ScreenPadd::sendHold() { send(SetHold{youPaused_ || holds_ != 0}); }

// A file kept on the card (the settings': your name, the animations)
void ScreenPadd::save(const char *file, std::string_view bytes) {
  sendTo<ScratchPadd::Storage::StoragePadd>(ScratchPadd::Storage::SaveFile{
      .path = file,
      .bytes = std::make_shared<const ScratchPadd::Storage::Bytes>(bytes.begin(), bytes.end())});
}

// The board's moves, the dice and the shows at the setting's pace
void ScreenPadd::paceAnimations() {
  gameClock_.setRate(rateOf(menus_.animations()));
  showClock_.setRate(rateOf(menus_.animations()));
}

// Whether something is animating that B (Back) can skip to its end: the
// dice, who goes first, the board's pieces moving, the turn's banner, a
// tile turned over, a home's or a roll's pay, the cards dealt coming in
bool ScreenPadd::skippable() const {
  if (flow_.screen() != ScreenFlow::Screen::Game || !latest_)
    return false;
  if (!roll_.idle() || firstRoll_.on() || lookingSince_ || celebration_)
    return true;
  if (gameClock_.seconds() - motionStarted_ < kSecondsBoardMoving)
    return true;
  if (banner_.moving(gameClock_.seconds())) // (not a prompt waiting for you)
    return true;
  const double t = showsNow();
  const bool dealing =
      dealt_.on() && !dealt_.cards().empty() && dealt_.turned(dealt_.cards().size() - 1, t) < 1;
  return dealing || discovery_.moving(t) || homePay_.moving(t) || rollPay_.moving(t) ||
         cardPlay_.moving(t);
}

// Every animation straight to its end frame (the user's: B, or every
// frame when Animations is Off)
void ScreenPadd::skipAnimations() {
  // The shows: their clock on past any of their ends
  static constexpr double kPastAnyShow = 30;
  showClock_.skipAhead(kPastAnyShow);
  // Who goes first: each die set down and the order read
  if (lookingSince_)
    startFirstRoll();
  for (int i = 0; i < 16 && firstRoll_.on() && !firstRoll_.done(showsNow()); ++i)
    firstRoll_.hurry(showsNow());
  if (firstRoll_.done(showsNow()))
    endFirstRoll();
  // The board's pieces where they were going, the turn's banner gone
  motionStarted_ = std::min(motionStarted_, gameClock_.seconds() - kSecondsBoardMoving);
  banner_.hurry(gameClock_.seconds()); // (a prompt waiting for you stays)
  roll_.skip(gameClock_.seconds());
}

} // namespace CosmicHex::Handheld::App
