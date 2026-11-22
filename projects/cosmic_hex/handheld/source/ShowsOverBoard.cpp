// ScreenPadd's shows: what shows over the board on the stage for a while
// (TopShows, drawn by TopViews), the HUD and the banners, the close-ups
// they show, each show's update a frame, and whether the stage is still
#include "ScreenPadd.hpp"

#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/Hud.hpp>
#include <CosmicHex/Handheld/MoveSpots.hpp>
#include <CosmicHex/Handheld/OtherPlayers.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>
#include <ScratchPadd/Motion/Motion.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>

namespace CosmicHex::Handheld::App {

// What shows over the board for a while (TopViews; on a DS words are on
// a layer over every shape, so only one at a time: GameShows::overBoard)
void ScreenPadd::drawShowsOverBoard(const TopFrame &frame) {
  // The roll's dice (behind another player's view they wait unseen: the
  // game's held)
  const double t = showsNow();
  if (!shows_.others().showing(t))
    drawRoll(frame, shows_.roll());
  const auto &latest = shows_.latest();
  if (!latest) {
    drawText(frame);
    return;
  }
  const State &state = latest->state;
  const int you = shows_.playing() ? 0 : -1;
  // (a roll's pay, over the board, the HUD's words still showing: the
  // bots' cards fly to their rows of the points bars)
  if (shows_.rollPay().moving(t))
    drawRollPay(frame, shows_.rollPay(), state, you);
  // (your cards gained, as Show resources shows them)
  if (shows_.hand().showing(t) && !shows_.cards().showing(t))
    drawGained(frame, state.player[0].resources, shows_.hand());
  // (Show resources, over the board, under the views below)
  if (shows_.cards().showing(t))
    drawResources(frame, state.player[static_cast<std::size_t>(infoPlayer(state))], shows_.cards());
  switch (shows_.overBoard()) {
  case GameShows::OverBoard::FirstRoll:
    drawFirstRoll(frame, latest->firstRoll, shows_.firstRoll());
    break;
  case GameShows::OverBoard::OtherPlayers:
    for (const PanelShow::Drawn &drawn : shows_.others().drawn(t))
      drawOtherPlayer(frame, viewOf(state, drawn.which), drawn.down, drawn.across);
    break;
  case GameShows::OverBoard::Discovery:
    drawDiscovery(frame, shows_.discovery(), closeUps_.face, closeUps_.back);
    break;
  case GameShows::OverBoard::Pick:
    drawPick(frame, shows_.pick(), bottomScreen_->resourceChoice()->title,
             state.player[static_cast<std::size_t>(state.turn)].resources, state.settings,
             BoardColours::ofPlayer(state.turn));
    drawBanner(renderer_->overlay());
    break;
  case GameShows::OverBoard::HomePay:
    drawHomePay(frame, shows_.homePay(), state,
                shows_.playing() && shows_.homePay().current()->player == 0);
    // (its die waiting out for the game: the banner over it, the picker's
    // CHOOSE A RESOURCE as the star's choice opens)
    if (shows_.homePay().waitingForGame(t))
      drawBanner(renderer_->overlay());
    break;
  case GameShows::OverBoard::CardPlay:
    drawCardPlay(frame, shows_.cardPlay(), state, you);
    break;
  case GameShows::OverBoard::Dealt:
    drawDealtCards(frame, shows_.dealt(), bottomScreen_ ? bottomScreen_->keepChosen() : -1);
    break;
  case GameShows::OverBoard::Hud:
    drawText(frame);
    break;
  }
}

// Over the board: the round, each player's points and resources in
// their colour, and the last move in words (the HUD); on your move, the
// list of moves when it's on; the held tag and the banner
void ScreenPadd::drawText(const TopFrame &frame) {
  Scene::Overlay &overlay = renderer_->overlay();
  if (!shows_.latest())
    return;
  const State &state = shows_.latest()->state;
  // The HUD (Hud.hpp): the turn's outline easing from the last turn's row
  // to this one; badges, yours when playing, the turn's when watching
  const std::optional<float> outlineRow = shows_.outlineRow();
  const bool lastMoveShows =
      !shows_.latest()->lastMoves.empty() && shows_.shownInfo() == BottomScreen::Info::None &&
      !shows_.cards().showing(showsNow()) && !shows_.others().showing(showsNow());
  const std::string lastMove =
      lastMoveShows ? shows_.latest()->lastMoves.back().substr(0, kCharsPerLine) : std::string();
  drawHud(frame, state,
          {.outlineRow = outlineRow,
           .badgesOf = shows_.playing() ? 0 : state.turn,
           .pops = &shows_.scorePops(),
           .lastMove = lastMove,
           .winner = state.winner < 0 ? std::string()
                     : shows_.playing() && state.winner == 0
                         ? std::string("You win!")
                         : shows_.names()[state.winner] + " wins!"});
  drawWatchReadouts(overlay);
  // (the list of moves: debug builds only, when turned on)
  if (shows_.playing() && shows_.latest()->yourMove &&
      shows_.shownInfo() == BottomScreen::Info::None && !shows_.cards().showing(showsNow()) &&
      bottomScreen_ && bottomScreen_->showingMoveList())
    drawMoves(overlay);
  drawHeldTag(overlay);
  drawBanner(overlay);
}

// The -watch builds: memory, measured, above the last move; and the bots'
// search, how many games a move and where its time went
void ScreenPadd::drawWatchReadouts(Scene::Overlay &overlay) {
  static constexpr Scene::Colour kReadout{120, 220, 160};
  if (kWatchAtStart && !memory_.line().empty())
    overlay.text(4, kStageHeight - 26, memory_.line().substr(0, kCharsPerLine), kReadout);
  if (kWatchAtStart && shows_.latest()->botGames > 0) {
    char games[32];
    std::snprintf(games, sizeof games, "bot: %d games a move", shows_.latest()->botGames);
    overlay.text(4, kStageHeight - 50, games, kReadout);
    // Where its time went, in ms: guessing, the tree, the rollouts,
    // judging; and rollout moves a game
    const GameBots::SearchTimes &t = shows_.latest()->botSearch;
    if (t.games > 0) {
      char where[64];
      std::snprintf(
          where, sizeof where, "g%lld t%lld r%lld j%lld m%lld",
          static_cast<long long>(t.guessing / 1000), static_cast<long long>(t.descending / 1000),
          static_cast<long long>(t.rollingOut / 1000), static_cast<long long>(t.judging / 1000),
          static_cast<long long>(t.rolloutActions / t.games));
      overlay.text(4, kStageHeight - 122, where, kReadout);
    }
  }
  if (kWatchAtStart && !memory_.busiest().empty())
    overlay.text(4, kStageHeight - 38, memory_.busiest().substr(0, kCharsPerLine), kReadout);
}

// While a view you opened holds the game (Show resources, another
// player's panel), the banner says so: PAUSED on a still grey band, as the
// pause itself shows (the user's: the Banner, like everything else). It
// comes in quickly as the view opens and goes as it closes, at the same
// time as the view's own way off (a second press of Show resources)
void ScreenPadd::drawHeldTag(Scene::Overlay &overlay) {
  const bool held =
      shows_.held(GameShows::Hold::Info) && flow_.screen() == ScreenFlow::Screen::Game;
  if (held != heldShown_) {
    heldShown_ = held;
    heldChangedAt_ = showsNow();
  }
  constexpr double kSecondsIn = 0.2, kSecondsOut = 0.35;
  const double since = showsNow() - heldChangedAt_;
  float shown = 0; // (0 gone, 1 there)
  if (held)
    shown = ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::EaseOut,
                                       static_cast<float>(std::min(since / kSecondsIn, 1.0)));
  else if (since < kSecondsOut)
    shown = 1 - ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::EaseIn,
                                           static_cast<float>(since / kSecondsOut));
  if (shown <= 0)
    return;
  // (going: the words slide off to the right as the band fades)
  const float middle = static_cast<float>(kStageWidth) / 2;
  static const std::string kTitle = "PAUSED";
  static const std::string kNote = "close to go on";
  drawBand(overlay, {.bandAlpha = shown,
                     .wordsX = held ? middle : middle + (1 - shown) * middle,
                     .title = kTitle,
                     .note = kNote,
                     .colour = {140, 146, 158}});
}

// Whose cards an info view shows: yours when playing, the turn's when
// watching
int ScreenPadd::infoPlayer(const State &state) const { return shows_.playing() ? 0 : state.turn; }

// The turn banner as it moves (ScratchPadd::Banner): a band softly in the player's
// colour with turquoise edges, the words sweeping through the middle
void ScreenPadd::drawBanner(Scene::Overlay &overlay) {
  // Paused: the banner still, in a neutral grey, rather than the turn's
  // (the user's: plainer to see than a word in the corner)
  // (only when paused by you, not held by a view or a discovery; its
  // own flag, as a hold's let go reaches the game a frame or two later)
  const bool pausedByYou =
      shows_.latest() && shows_.latest()->paused && !shows_.playing() && shows_.youPaused();
  if (flow_.screen() == ScreenFlow::Screen::Paused || pausedByYou) {
    static const std::string kTitle = "PAUSED";
    const std::string note =
        flow_.screen() == ScreenFlow::Screen::Paused ? "the menu is below" : "A to go on";
    drawBand(overlay, {.bandAlpha = 1,
                       .wordsX = static_cast<float>(kStageWidth) / 2,
                       .title = kTitle,
                       .note = note,
                       .colour = {140, 146, 158}});
    return;
  }
  if (const auto look = shows_.banner().at(gameNow()))
    drawBand(overlay, *look);
}

// A banner's band and words, as `look` says (ScratchPadd::Banner::draw,
// in the game's colours)
void ScreenPadd::drawBand(Scene::Overlay &overlay, const ScratchPadd::Banner::Look &look) {
  ScratchPadd::Banner::draw(overlay, look,
                            {.screenWidth = static_cast<float>(kStageWidth),
                             .screenHeight = static_cast<float>(kStageHeight),
                             .scale = kStageScale,
                             .accent = Palette::kAccent,
                             .titleScale = 2.0f});
}

// Your moves, under the scores: a window of the list round the cursor,
// the chosen one highlighted
void ScreenPadd::drawMoves(Scene::Overlay &overlay) {
  const std::vector<std::size_t> shown = movesShown();
  overlay.rectangle(0, 60, kStageWidth, kStageHeight - 76, {12, 18, 30, 210});
  const char *heading =
      selected_ ? "Your moves here (B: all moves)" : "Your moves (touch the board to narrow)";
  overlay.text(6, 64, heading, Palette::kHighlight);
  if (shown.empty()) {
    overlay.text(6, 80, "None here: touch elsewhere, or B for all", Palette::kSoftWords);
    return;
  }
  const auto kLines = static_cast<std::size_t>((kStageHeight - 96) / kLineHeight);
  const std::size_t first = cursor_ >= kLines ? cursor_ - kLines + 1 : 0;
  for (std::size_t line = 0; line < kLines && first + line < shown.size(); ++line) {
    const std::size_t i = first + line;
    std::string text = (i == cursor_ ? "> " : "  ") + shows_.latest()->moveText[shown[i]];
    if (text.size() > kCharsPerLine)
      text = text.substr(0, kCharsPerLine - 2) + "..";
    overlay.text(6, 78 + kLineHeight * static_cast<float>(line), text,
                 i == cursor_ ? Palette::kHighlight : Palette::kSoftWords);
  }
}

// The -watch builds: where each frame's time goes, once a second
void ScreenPadd::showProfile() {
  if (!kWatchAtStart)
    return;
  if (profile_.secondDone()) {
    char start[24];
    std::snprintf(start, sizeof start, "start %.1fs ", startSeconds_);
    profileLine_ = start + profile_.line();
    spdlog::info("frames: {}", profileLine_);
  }
  // (not over a view that fills the stage: its words would be lost)
  if (shows_.firstRoll().on() || shows_.others().showing(showsNow()) || shows_.dealt().on() ||
      shows_.discovery().moving(showsNow()) || shows_.homePay().moving(showsNow()))
    return;
  // (three lines where the stage's narrow)
  for (std::size_t from = 0, row = 0; from < profileLine_.size() && row < 4;
       from += kCharsPerLine, ++row)
    renderer_->overlay().text(4, kStageHeight - 110 + 12 * static_cast<float>(row),
                              profileLine_.substr(from, kCharsPerLine), {240, 200, 120});
}

// Close-ups: a picture shown big drawn into its slot
// (PlatformProfile::closeUpSize; PictureSlot: drawn only when it isn't
// there already), a band of rows at a time (PlatformProfile::
// closeUpBandRows: on a DS, so a 128-pixel picture never has to be in
// memory whole), each recipe read once
void ScreenPadd::closeUpTile(CosmicHex::Art::Face face) {
  closeUpTileInto(closeUps_.face, face, kThisPlatform.closeUpSize);
  if (const CosmicHex::Handheld::Discovery::Found *found = shows_.discovery().current())
    closeUpTileInto(closeUps_.back, found->faceDown, kThisPlatform.closeUpBackSize);
}

void ScreenPadd::closeUpTileInto(ScratchPadd::PictureSlot &slot, CosmicHex::Art::Face face,
                                 int size) {
  if (slot.showing(static_cast<int>(face)))
    return; // (drawn already)
  const auto recipe = ScratchPadd::TextureRecipe::read(CosmicHex::Art::recipeOf(face));
  if (!recipe)
    return;
  slot.show(
      *renderer_, static_cast<int>(face),
      [&](int first, int rows) {
        return recipePixels(*recipe, size, size, kThisPlatform.recipeSmoothing, {first, rows});
      },
      kThisPlatform.closeUpBandRows);
}

// Each frame: what the pointer points at. While the game wants you to
// choose on the board (your move: placing at the start, where a build
// goes, a ship's moves) and nothing on the stage is moving (pieces,
// a banner, a show, a view over the board): the ship touched, else what's
// been touched, else the place nearest the middle; otherwise nothing, and
// it fades away (the user's)
void ScreenPadd::updatePointer() {
  pointer_.aim(pointerCorner(), showsNow());
  // (the map draws the pointer too: again when it's moved)
  if (pointerCorner() != flatPointer_)
    touchDirty_ = true;
  if (tapPointer_ && pointerCorner())
    pointer_.tap(showsNow());
  tapPointer_ = false;
  // (the camera follows what you chose, and comes back when nothing is)
  const auto followed = followedCorner();
  const auto pointAt = [](const CosmicHex::Corner &place) {
    const Scene::Transform at = BoardPieces::pointerAt(place);
    return Scene::Vector3{at.m[0][3], at.m[1][3], at.m[2][3]};
  };
  follow_.aim(followed ? std::optional(pointAt(*followed)) : std::nullopt, showsNow());
}

// What the camera follows: the corner you chose (a move, a placement),
// else the ship you touched, else the corner you touched; not the pointer's
// own resting place near the middle
std::optional<CosmicHex::Corner> ScreenPadd::followedCorner() const {
  if (!pointerCorner() || !bottomScreen_ || !shows_.latest())
    return std::nullopt;
  if (bottomScreen_->placingBlockade())
    return bottomScreen_->chosenPlacement() ? pointerCorner() : std::nullopt;
  if (const auto chosen = bottomScreen_->chosenPlacement())
    if (const MoveSpot spot = spotOf(shows_.latest()->state, *chosen); !spot.corners.empty())
      return spot.corners.back();
  const auto &ships = shows_.latest()->state.ships;
  if (const auto ship = bottomScreen_->selectedShip();
      ship && *ship >= 0 && *ship < static_cast<int>(ships.size()))
    return ships[static_cast<std::size_t>(*ship)].corner;
  if (selected_)
    return selected_->corner;
  return std::nullopt;
}

std::optional<CosmicHex::Corner> ScreenPadd::pointerCorner() const {
  if (!shows_.playing() || !shows_.latest() || !shows_.latest()->yourMove || !bottomScreen_ ||
      flow_.screen() != ScreenFlow::Screen::Game)
    return std::nullopt;
  const std::vector<CosmicHex::Corner> places = bottomScreen_->placingCorners();
  const std::optional<int> ship = bottomScreen_->selectedShip();
  if (places.empty() && !ship && !selected_ && !bottomScreen_->placingBlockade())
    return std::nullopt; // (nothing to choose on the board, nothing touched)
  const double t = showsNow();
  const bool busy = shows_.showsPlaying() || shows_.banner().moving(gameNow()) ||
                    shows_.shownInfo() != BottomScreen::Info::None || shows_.cards().showing(t) ||
                    shows_.hand().moving(t) || shows_.pick().at(t);
  if (busy)
    return std::nullopt;
  // (the blockade: centred on the planet chosen, else on the first it can
  // go to; a tile's middle is corner -1)
  if (bottomScreen_->placingBlockade()) {
    const auto hexes = bottomScreen_->placingHexes();
    if (const auto chosen = bottomScreen_->chosenPlacement())
      return CosmicHex::Corner{
          shows_.latest()->state.planets[static_cast<std::size_t>(chosen->a)].hex, -1};
    if (!hexes.empty())
      return CosmicHex::Corner{hexes.front(), -1};
  }
  // (what you've chosen to place, or where a ship's to go: there, ahead
  // of the ship itself)
  if (const std::optional<CosmicHex::Action> chosen = bottomScreen_->chosenPlacement())
    if (const MoveSpot spot = spotOf(shows_.latest()->state, *chosen); !spot.corners.empty())
      return spot.corners.back();
  const auto &ships = shows_.latest()->state.ships;
  if (ship && *ship >= 0 && *ship < static_cast<int>(ships.size()))
    return ships[static_cast<std::size_t>(*ship)].corner;
  if (selected_) // (touched: there, marked or not)
    return selected_->corner;
  // (nothing touched yet: the place nearest the board's middle, in view)
  const auto fromMiddle = [](const CosmicHex::Corner &corner) {
    const Scene::Transform at = BoardPieces::onCorner(corner);
    return at.m[0][3] * at.m[0][3] + at.m[2][3] * at.m[2][3];
  };
  return *std::ranges::min_element(places, {}, fromMiddle);
}

// Whether anything on the stage moves now: the board coming down, the
// shows (GameShows::moving), the pointer and the camera, a view you opened
bool ScreenPadd::topMoving() const {
  if (flow_.screen() != ScreenFlow::Screen::Game || !shows_.latest() || descentStarted_)
    return true;
  if (pointer_.at(showsNow()) || follow_.moving(showsNow()))
    return true; // (the board's pointer turning, fading or gliding; the camera following)
  return shows_.moving() || shows_.shownInfo() != BottomScreen::Info::None;
}

// Whether the stage can hold its last frame (StillFrames: after something
// moved, one frame more, as its last drawn frame may still show a trace of
// it: a banner's band fading)
bool ScreenPadd::topIsStill() const {
  const bool changed =
      snapshots_ != drawnSnapshots_ || turn_ != drawnTurn_ || selected_ != drawnSelection_;
  if (!still_.canHold(topMoving(), changed))
    return false;
  return !kWatchAtStart || Clock::now() - drawnAt_ < std::chrono::seconds(1);
}

} // namespace CosmicHex::Handheld::App
