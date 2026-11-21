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
// a layer over every shape, so only one at a time)
void ScreenPadd::drawShowsOverBoard(const TopFrame &frame) {
  // The roll's dice (behind another player's view they wait unseen: the
  // game's held)
  if (!others_.showing(showsNow()))
    drawRoll(frame, roll_);
  // (a roll's pay, over the board, the HUD's words still showing: the
  // bots' cards fly to their rows of the points bars)
  if (latest_ && rollPay_.moving(showsNow()))
    drawRollPay(frame, rollPay_, latest_->state, mode_ == Mode::Playing ? 0 : -1);
  // (your cards gained, as Show resources shows them)
  if (latest_ && hand_.showing(showsNow()) && !cards_.showing(showsNow()))
    drawGained(frame, latest_->state.player[0].resources, hand_);
  // (Show resources, over the board, under the views below)
  if (latest_ && cards_.showing(showsNow()))
    drawResources(
        frame, latest_->state.player[static_cast<std::size_t>(infoPlayer(latest_->state))], cards_);
  if (firstRoll_.on() && latest_)
    drawFirstRoll(frame, latest_->firstRoll, firstRoll_);
  else if (others_.showing(showsNow()) && latest_ && others_.which() >= 0) {
    for (const PanelShow::Drawn &drawn : others_.drawn(showsNow()))
      drawOtherPlayer(frame, viewOf(latest_->state, drawn.which), drawn.down, drawn.across);
  } else if (discovery_.moving(showsNow()))
    drawDiscovery(frame, discovery_, closeUps_.face, closeUps_.back);
  else if (pick_.at(showsNow()) && latest_ && bottomScreen_ && bottomScreen_->resourceChoice()) {
    drawPick(frame, pick_, bottomScreen_->resourceChoice()->title,
             latest_->state.player[static_cast<std::size_t>(latest_->state.turn)].resources,
             latest_->state.settings, BoardColours::ofPlayer(latest_->state.turn));
    drawBanner(renderer_->overlay());
  } else if (homePay_.moving(showsNow()) && latest_)
    drawHomePay(frame, homePay_, latest_->state,
                mode_ == Mode::Playing && homePay_.current()->player == 0, closeUps_.cards[0]);
  else if (cardPlay_.moving(showsNow()) && latest_)
    drawCardPlay(frame, cardPlay_, latest_->state, mode_ == Mode::Playing ? 0 : -1);
  else if (dealt_.on())
    drawDealtCards(frame, dealt_, bottomScreen_ ? bottomScreen_->keepChosen() : -1,
                   closeUps_.cards);
  else
    drawText(frame);
}

// Over the board: the round, each player's points and resources in
// their colour, and the last move in words (the HUD); on your move, the
// list of moves when it's on; the held tag and the banner
void ScreenPadd::drawText(const TopFrame &frame) {
  Scene::Overlay &overlay = renderer_->overlay();
  if (!latest_)
    return;
  const State &state = latest_->state;
  // The HUD (Hud.hpp): the turn's outline easing from the last turn's row
  // to this one; badges, yours when playing, the turn's when watching
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
  const bool lastMoveShows = !latest_->lastMoves.empty() &&
                             shownInfo_ == BottomScreen::Info::None &&
                             !cards_.showing(showsNow()) && !others_.showing(showsNow());
  const std::string lastMove =
      lastMoveShows ? latest_->lastMoves.back().substr(0, kCharsPerLine) : std::string();
  drawHud(frame, state,
          {.outlineRow = outlineRow,
           .badgesOf = mode_ == Mode::Playing ? 0 : state.turn,
           .pops = &scorePops_,
           .lastMove = lastMove,
           .winner = state.winner < 0 ? std::string()
                     : mode_ == Mode::Playing && state.winner == 0
                         ? std::string("You win!")
                         : names_[state.winner] + " wins!"});
  drawWatchReadouts(overlay);
  // (the list of moves: debug builds only, when turned on)
  if (mode_ == Mode::Playing && latest_->yourMove && shownInfo_ == BottomScreen::Info::None &&
      !cards_.showing(showsNow()) && bottomScreen_ && bottomScreen_->showingMoveList())
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
  if (kWatchAtStart && latest_->botGames > 0) {
    char games[32];
    std::snprintf(games, sizeof games, "bot: %d games a move", latest_->botGames);
    overlay.text(4, kStageHeight - 50, games, kReadout);
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
  const bool held = (holds_ & static_cast<std::uint8_t>(Hold::Info)) != 0 &&
                    flow_.screen() == ScreenFlow::Screen::Game;
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
int ScreenPadd::infoPlayer(const State &state) const {
  return mode_ == Mode::Playing ? 0 : state.turn;
}

// The turn banner as it moves (ScratchPadd::Banner): a band softly in the player's
// colour with turquoise edges, the words sweeping through the middle
void ScreenPadd::drawBanner(Scene::Overlay &overlay) {
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
                       .wordsX = static_cast<float>(kStageWidth) / 2,
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
    std::string text = (i == cursor_ ? "> " : "  ") + latest_->moveText[shown[i]];
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
  if (firstRoll_.on() || others_.showing(showsNow()) || dealt_.on() ||
      discovery_.moving(showsNow()) || homePay_.moving(showsNow()))
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
  if (const CosmicHex::Handheld::Discovery::Found *found = discovery_.current())
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

void ScreenPadd::closeUpCards(const std::vector<CosmicHex::Art::Card> &cards) {
  const int size = kThisPlatform.closeUpSize, height = size * 52 / 63;
  for (std::size_t i = 0; i < cards.size() && i < closeUps_.cards.size(); ++i) {
    const auto recipe = ScratchPadd::TextureRecipe::read(CosmicHex::Art::cardRecipeOf(cards[i]));
    if (!recipe)
      continue;
    closeUps_.cards[i].show(
        *renderer_, static_cast<int>(cards[i]),
        [&](int first, int rows) {
          // (the card's picture: its drawing (the card's 63 x 88) down to
          // the picture's foot, `height`; the slot's rows below it clear)
          std::vector<Scene::Colour> band(static_cast<std::size_t>(size) *
                                          static_cast<std::size_t>(rows));
          const int drawn = std::clamp(height - first, 0, rows);
          if (drawn > 0) {
            const auto pixels = recipePixels(*recipe, size, size * 88 / 63,
                                             kThisPlatform.recipeSmoothing, {first, drawn});
            std::copy(pixels.begin(), pixels.end(), band.begin());
          }
          return band;
        },
        kThisPlatform.closeUpBandRows);
  }
}

// Each frame: the game held while a roll's pay flies
void ScreenPadd::updateRollPay() {
  // (once the dice have gone from the stage: not over the roll)
  if (rollPay_.waiting() && roll_.idle())
    rollPay_.start(showsNow());
  const RollPayShow::Step step = rollPay_.update(showsNow());
  if (step.hold || step.letGo)
    holdFor(Hold::RollPay, step.hold);
}

// Each frame: while you choose a resource on the picker, the banner as
// each choice opens (two in a row: one each), then, once it's gone, the
// card in focus shown big on the stage (PickShow; the user's: not behind it)
void ScreenPadd::updatePick() {
  const bool choosing = mode_ == Mode::Playing && latest_ && latest_->yourMove && bottomScreen_ &&
                        bottomScreen_->mode() == BottomScreen::Mode::Resources &&
                        bottomScreen_->resourceChoice();
  if (!choosing) {
    bannerFor_.reset();
    pick_.show(std::nullopt, showsNow());
    return;
  }
  const Pickers::ResourceChoice &choice = *bottomScreen_->resourceChoice();
  if (!bannerFor_ || !bannerFor_->sameAs(choice)) {
    bannerFor_ = choice;
    pick_.show(std::nullopt, showsNow()); // (the last choice's card gone)
    banner_.start(gameClock_.seconds(), "CHOOSE A RESOURCE", std::string(choice.title),
                  Palette::kHighlight);
  }
  pick_.show(banner_.moving(gameClock_.seconds()) ? std::nullopt : bottomScreen_->resourceInFocus(),
             showsNow());
}

// Each frame: your cards gained shown once nothing that brings them is
// still showing (the roll's pay, the home's, choosing one, a discovery)
void ScreenPadd::updateHand() {
  const double t = showsNow();
  // (the board's pieces moving count too: a discovery comes once they've
  // stopped)
  const bool busy = !roll_.idle() || rollPay_.moving(t) || rollPay_.waiting() ||
                    gameClock_.seconds() - motionStarted_ < kSecondsBoardMoving ||
                    homePay_.moving(t) || pick_.at(t) || bannerFor_ || discovery_.moving(t) ||
                    dealt_.on() || cardPlay_.moving(t) || cards_.showing(t) || firstRoll_.on();
  if (hand_.waiting() && !busy)
    hand_.start(t);
}

void ScreenPadd::updateCardPlay() {
  const CardPlayShow::Step step = cardPlay_.update(showsNow());
  if (step.hold || step.letGo)
    holdFor(Hold::CardPlay, step.hold);
}

// Each frame: the home's pay held over the game; its card drawn sharp
// (into the first dealt card's slot: the two never show together, and
// the DS has no texture memory for another)
void ScreenPadd::updateHomePay() {
  if (const HomePayShow::Pay *pay = homePay_.current(); pay && pay->resource) {
    const auto card = static_cast<CosmicHex::Art::Card>(*pay->resource);
    if (!closeUps_.cards[0].showing(static_cast<int>(card)))
      closeUpCards({card});
  }
  const HomePayShow::Step step = homePay_.update(showsNow());
  if (step.hold || step.letGo)
    holdFor(Hold::HomePay, step.hold);
}

// Each frame: a discovery's close-up drawn before it starts, the game
// held while one shows and let go after the last (unless a view holds
// it too), the map drawn again as its tile fades in
void ScreenPadd::updateDiscovery() {
  const DiscoveryShow::Step step =
      discovery_.update(showsNow(), [this](CosmicHex::Art::Face face) { closeUpTile(face); });
  if (step.hold || step.letGo)
    holdFor(Hold::Discovery, step.hold);
  if (step.redrawMap)
    touchDirty_ = true;
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
  if (!pointerCorner() || !bottomScreen_ || !latest_)
    return std::nullopt;
  if (bottomScreen_->placingBlockade())
    return bottomScreen_->chosenPlacement() ? pointerCorner() : std::nullopt;
  if (const auto chosen = bottomScreen_->chosenPlacement())
    if (const MoveSpot spot = spotOf(latest_->state, *chosen); !spot.corners.empty())
      return spot.corners.back();
  const auto &ships = latest_->state.ships;
  if (const auto ship = bottomScreen_->selectedShip();
      ship && *ship >= 0 && *ship < static_cast<int>(ships.size()))
    return ships[static_cast<std::size_t>(*ship)].corner;
  if (selected_)
    return selected_->corner;
  return std::nullopt;
}

std::optional<CosmicHex::Corner> ScreenPadd::pointerCorner() const {
  if (mode_ != Mode::Playing || !latest_ || !latest_->yourMove || !bottomScreen_ ||
      flow_.screen() != ScreenFlow::Screen::Game)
    return std::nullopt;
  const std::vector<CosmicHex::Corner> places = bottomScreen_->placingCorners();
  const std::optional<int> ship = bottomScreen_->selectedShip();
  if (places.empty() && !ship && !selected_ && !bottomScreen_->placingBlockade())
    return std::nullopt; // (nothing to choose on the board, nothing touched)
  const double t = showsNow();
  const bool busy = gameClock_.seconds() - motionStarted_ < kSecondsBoardMoving ||
                    banner_.moving(gameClock_.seconds()) || !roll_.idle() ||
                    shownInfo_ != BottomScreen::Info::None || cards_.showing(t) ||
                    discovery_.moving(t) || homePay_.moving(t) || rollPay_.moving(t) ||
                    cardPlay_.moving(t) || hand_.moving(t) || dealt_.on() || pick_.at(t);
  if (busy)
    return std::nullopt;
  // (the blockade: centred on the planet chosen, else on the first it can
  // go to; a tile's middle is corner -1)
  if (bottomScreen_->placingBlockade()) {
    const auto hexes = bottomScreen_->placingHexes();
    if (const auto chosen = bottomScreen_->chosenPlacement())
      return CosmicHex::Corner{latest_->state.planets[static_cast<std::size_t>(chosen->a)].hex, -1};
    if (!hexes.empty())
      return CosmicHex::Corner{hexes.front(), -1};
  }
  // (what you've chosen to place, or where a ship's to go: there, ahead
  // of the ship itself)
  if (const std::optional<CosmicHex::Action> chosen = bottomScreen_->chosenPlacement())
    if (const MoveSpot spot = spotOf(latest_->state, *chosen); !spot.corners.empty())
      return spot.corners.back();
  const auto &ships = latest_->state.ships;
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

// Whether anything on the stage moves now
bool ScreenPadd::topMoving() const {
  if (flow_.screen() != ScreenFlow::Screen::Game || !latest_ || descentStarted_ || !roll_.idle() ||
      firstRoll_.moving())
    return true;
  if (gameClock_.seconds() - motionStarted_ < kSecondsBoardMoving)
    return true; // (pieces still moving, or a number chip dropping)
  if (pointer_.at(showsNow()) || follow_.moving(showsNow()))
    return true; // (the board's pointer turning, fading or gliding; the camera following)
  if (banner_.moving(gameClock_.seconds()))
    return true; // (the turn's banner moving)
  if (shownInfo_ != BottomScreen::Info::None || cards_.moving(showsNow()) || outlineMovedAt_)
    return true; // (an info view coming in or going, the points' outline)
  // (the views over the board: each says)
  if (scorePops_.popping(showsNow()) || cardPlay_.moving(showsNow()) || celebration_ ||
      pick_.moving(showsNow()) || hand_.moving(showsNow()))
    return true; // (points or badges popping in, a card played)
  if (dealt_.moving() || others_.moving(showsNow()) || discovery_.moving(showsNow()) ||
      homePay_.moving(showsNow()) || rollPay_.moving(showsNow()))
    return true;
  return false;
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
