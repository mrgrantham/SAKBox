// ScreenPadd's touch panel and controls: the buttons and the stylus on
// every screen, your moves, and the touch panel's picture (a menu, a
// choosing screen, or the flat map with its buttons) put on the bottom
// screen
#include "ScreenPadd.hpp"

#include <CosmicHex/Handheld/MoveSpots.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>

namespace CosmicHex::Handheld::App {

namespace {

// The scripted-taps builds (COSMIC_HEX_SCRIPTED_TAPS): taps the
// app makes itself at set seconds after starting, as if the stylus had, so
// a screen reached by touch can be seen in an emulator without touching it
// (and photographed for screenshot tests). Each: when, where on the touch
// panel (DS pixels, scaled to the 3DS's), held for a moment
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

} // namespace

// The buttons, by screen (ScreenFlow): the menus' (MenuScreens), any
// button once a game's won; in a game, watching: A pauses or goes on;
// playing, on your move: the touch panel's choosing screens, or up and
// down to choose from the list (the moves at the touched spot, or all of
// them), A to make it, B to show them all again
void ScreenPadd::handleButtons(const Input::Edges &pressed, bool zooming) {
  // Confirm and Back by the pad's own convention (Input's menu roles: on
  // a DS and 3DS, A and B)
  const Input::Button confirm = lastPad_.buttonFor(Input::Role::Confirm);
  const Input::Button back = lastPad_.buttonFor(Input::Role::Back);
  if (skipOrHurry(pressed, confirm, back))
    return;
  if (menus_.showing()) {
    if (menus_.press(pressed, confirm, back, now()))
      touchDirty_ = true;
    return;
  }
  if (flow_.screen() == ScreenFlow::Screen::Won) {
    if (pressed.pressed(confirm) || pressed.pressed(back))
      apply(flow_.on(ScreenFlow::Event::Continue));
    return;
  }
  pressInGame(pressed, zooming, confirm, back);
}

// An animation's end, or the start's next step, on Confirm or Back: true
// if the press was used
bool ScreenPadd::skipOrHurry(const Input::Edges &pressed, Input::Button confirm,
                             Input::Button back) {
  // (B: an animation straight to its end; pressed again, B's own job)
  if (pressed.pressed(back) && skippable()) {
    skipAnimations();
    return true;
  }
  // (A: on from the winner's celebration)
  if (celebration_ && pressed.pressed(confirm)) {
    skipAnimations();
    return true;
  }
  // (A: on from looking at the board, and from who goes first)
  if (lookingSince_ && pressed.pressed(confirm)) {
    startFirstRoll();
    return true;
  }
  if (firstRoll_.on() && pressed.pressed(confirm)) {
    firstRoll_.hurry(showsNow()); // (the next die now; once all have landed, on)
    if (firstRoll_.done(showsNow()))
      endFirstRoll();
    return true;
  }
  return false;
}

// In a game: the touch panel's buttons (BottomScreen::press: the D-pad
// moves, A picks then confirms, B goes back): a choosing screen's on your
// move, a screen to look at's (the other players, your cards) any time
void ScreenPadd::pressInGame(const Input::Edges &pressed, bool zooming, Input::Button confirm,
                             Input::Button back) {
  using Input::Button;
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
  if (key && bottomScreen_ && bottomScreen_->looking()) {
    bottomScreen_->press(*key);
    touchDirty_ = true;
    return;
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
  if (bottomScreen_ && key) {
    const BottomScreen::Outcome outcome = bottomScreen_->press(*key);
    if (outcome.play) {
      send(PlayMove{*outcome.play});
      latest_->yourMove = false;
      selected_.reset();
    }
    if (outcome.used) {
      touchDirty_ = true;
      return;
    }
  }
  pressMoveList(pressed, confirm, back);
}

// The list of moves' keys, only while it shows (debug builds)
void ScreenPadd::pressMoveList(const Input::Edges &pressed, Input::Button confirm,
                               Input::Button back) {
  using Input::Button;
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
    touchDirty_ = true;
  }
  if (pressed.pressed(confirm) && cursor_ < shown.size()) {
    send(PlayMove{latest_->moves[shown[cursor_]]});
    latest_->yourMove = false; // (until the game says it's yours again)
    selected_.reset();
    cursor_ = 0;
    touchDirty_ = true;
  }
}

// Whether your move is on a hex (the blockade's, after a 7), not a corner
bool ScreenPadd::choosingAHex() const {
  return latest_ && latest_->yourMove && latest_->state.phase == CosmicHex::Phase::Blockade;
}

// The moves listed on your move: those at the touched spot, or all of
// them (their places in the snapshot's list)
std::vector<std::size_t> ScreenPadd::movesShown() const {
  std::vector<std::size_t> shown;
  if (!latest_)
    return shown;
  for (std::size_t i = 0; i < latest_->moves.size(); ++i)
    if (!selected_ || concerns(latest_->state, latest_->moves[i], *selected_))
      shown.push_back(i);
  return shown;
}

// A touch: on a menu, a tap as the stylus comes down; in a game, a tap
// selects the corner (and hex) under it, and a drag moves the board, when
// it's zoomed in
void ScreenPadd::handleTouch(const Input::Touch &touch) {
  const ScreenFlow::Screen screen = flow_.screen();
  if (screen != ScreenFlow::Screen::Game) {
    if (touch.down && !touchWasDown_) {
      if (screen == ScreenFlow::Screen::Won)
        apply(flow_.on(ScreenFlow::Event::Continue));
      else
        menus_.tap(touch.x, touch.y, now());
      touchDirty_ = true;
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
    if (std::abs(touch.x - touchStart_[0]) + std::abs(touch.y - touchStart_[1]) > 6 && onTheBoard())
      dragged_ = true;
    if (dragged_ && (dx != 0 || dy != 0)) {
      picture_->pan(static_cast<float>(dx), static_cast<float>(dy));
      touchDirty_ = true;
    }
    touchLast_ = {touch.x, touch.y};
  } else if (touchWasDown_ && !dragged_) {
    tapBoard();
  }
  touchWasDown_ = touch.down;
}

// A tap in a game, where the stylus came down: the touch panel's first
// (its buttons, its choosing screens, a place for what's being built);
// what it doesn't use selects a corner
void ScreenPadd::tapBoard() {
  const auto spot = picture_->at(touchStart_[0], touchStart_[1]);
  const BottomScreen::Outcome outcome =
      bottomScreen_
          ? bottomScreen_->tap(touchStart_[0], touchStart_[1], onTheBoard() ? spot : std::nullopt)
          : BottomScreen::Outcome{};
  if (outcome.roll && latest_ && yourRollWaits(*latest_))
    rollAsked_ = true;
  if (outcome.play && latest_ && latest_->yourMove) {
    send(PlayMove{*outcome.play});
    latest_->yourMove = false; // (until the game says it's yours again)
    selected_.reset();
  } else if (!outcome.used) {
    // (the same place again: the pointer put away; elsewhere: it goes
    // there and taps it)
    const bool same = selected_ && spot && selected_->corner == spot->corner;
    selected_ = same ? std::nullopt : spot;
    tapPointer_ = selected_.has_value();
  } else if (bottomScreen_ && bottomScreen_->chosenPlacement()) {
    tapPointer_ = true; // (a marked place chosen: the pointer taps it)
  }
  cursor_ = 0;
  touchDirty_ = true;
}

// The stylus as the scripted-taps builds have it: each tap down for a few
// frames at its time (else the stylus as it is)
Input::Touch ScreenPadd::scripted(const Input::Touch &touch) {
  const double now = std::chrono::duration<double>(Clock::now() - startedAt_).count();
  for (const ScriptedTap &tap : kScriptedTaps)
    if (now >= tap.seconds && now < tap.seconds + 0.1) {
      Input::Touch down = touch;
      down.down = true;
      down.x = static_cast<std::int16_t>(static_cast<float>(tap.x) * kTouchScale);
      down.y = static_cast<std::int16_t>(static_cast<float>(tap.y) * kTouchScale);
      return down;
    }
  return touch;
}

// Whether the touch panel shows the board (rather than a choosing screen)
bool ScreenPadd::onTheBoard() const {
  return !bottomScreen_ || bottomScreen_->mode() == BottomScreen::Mode::Board ||
         bottomScreen_->mode() == BottomScreen::Mode::Placing;
}

// Whether the title's fade from black shows on the touch panel too
bool ScreenPadd::onAMenuScreen() const {
  const ScreenFlow::Screen screen = flow_.screen();
  return screen == ScreenFlow::Screen::Title || screen == ScreenFlow::Screen::Setup ||
         screen == ScreenFlow::Screen::Settings;
}

// The touch panel, drawn again: its picture, then on to the screen
void ScreenPadd::drawTouchPanel() {
  touchDirty_ = false;
  bool showingMap = false; // (the map, rather than a menu or a choosing screen)
  const auto drawn = drawTouchPicture(showingMap);
  if (!drawn)
    return;
  presentTouchPanel(*drawn, showingMap);
}

// The touch panel's picture: a menu, a choosing screen in the board's
// place, or the board with its buttons drawn straight onto it (no copy of
// the map); none before a game's first state
std::optional<std::span<const Scene::Colour>> ScreenPadd::drawTouchPicture(bool &showingMap) {
  if (menus_.showing())
    return menus_.draw(*touchCanvas_, now());
  if (!latest_)
    return std::nullopt;
  if (bottomScreen_->coversBoard()) {
    bottomScreen_->draw(*touchCanvas_);
    return touchCanvas_->pixels();
  }
  // (the map drawn when the game changes, not again each frame of the
  // fade)
  if (!fading_)
    drawMapWithButtons();
  showingMap = true;
  return picture_->pixels();
}

// The map, its marks, and the buttons over it
void ScreenPadd::drawMapWithButtons() {
  // The touched corner (or the blockade's hex), and where what's
  // being built may go
  BoardPicture::Marks marks;
  if (selected_ && choosingAHex())
    marks.hex = selected_->hex;
  // (the pointer, flat and still, where the top screen's chevron is)
  if (const auto look = pointer_.at(showsNow()))
    marks.pointer = look->to;
  // (and at the start, what's been touched, shown as the piece)
  const std::optional<CosmicHex::Action> chosen = bottomScreen_->chosenPlacement();
  std::optional<CosmicHex::Corner> chosenAt;
  if (chosen)
    chosenAt = CosmicHex::boardCorners()[static_cast<std::size_t>(chosen->a)];
  for (const CosmicHex::Corner &corner : bottomScreen_->placingCorners())
    if (corner != chosenAt)
      marks.corners.push_back(corner);
  marks.fuelCorners = bottomScreen_->placingCornersWithFuel();
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
    // Tiles turned over still face down on the map until the stage
    // has shown them, then each fades in (Discovery)
    marks.faceDown = discovery_.waitingAfterCurrent();
    if (const auto revealing = discovery_.revealing(showsNow()))
      marks.revealing =
          BoardPicture::Marks::Revealing{.hex = revealing->hex, .shown = revealing->shown};
    picture_->draw(latest_->state, marks);
  }
  auto timing = profile_.time(kProfileButtons);
  Canvas board = Canvas::over(picture_->pixelsToDrawOver(), kTouchWidth, kTouchHeight);
  bottomScreen_->drawOverBoard(board);
}

// The touch panel's picture to the bottom screen (a column at a time on a
// 3DS, whose screen is turned on its side). Fading from the menu to the
// map as the board comes down: the setup menu's picture (not drawn again
// in a game) and the map, mixed as they go to the screen, in whole numbers
// (no floating point and no buffers of their own: the DS has neither the
// hardware nor the memory); or the title fading in from black
void ScreenPadd::presentTouchPanel(std::span<const Scene::Colour> drawn, bool showingMap) {
  const float down = descent();
  std::optional<std::span<const Scene::Colour>> from;
  int weight = 256; // (of the map, out of 256)
  if (fadeFromMenu_ && down < 1 && menus_.lastPicture().size() == drawn.size()) {
    from = menus_.lastPicture();
    weight = static_cast<int>(down * 256);
    touchDirty_ = true; // (and again next frame, until it's done)
  } else if (fadeFromMenu_ && down >= 1) {
    fadeFromMenu_ = false;
    touchDirty_ = true; // (once more: the map as the game is now)
  }
  fading_ = from.has_value();
  // The title fading in from black (the menu below it too)
  const bool fromBlack = !from.has_value() && onAMenuScreen() && titleFadedIn() < 1;
  if (fromBlack) {
    weight = static_cast<int>(titleFadedIn() * 256);
    touchDirty_ = true; // (and again next frame, until it's done)
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
      touchScreen_->present(kTouchWidth, kTouchHeight, to); // (as it is: the quick copy)
      return;
    }
    int fromRow = kTouchHeight, toRow = 0;
    for (const BoardPicture::Area &area : picture_->changed()) {
      fromRow = std::min(fromRow, area.y);
      toRow = std::max(toRow, area.y + area.height);
    }
    if (fromRow < toRow)
      touchScreen_->present(kTouchWidth, kTouchHeight, to, fromRow, toRow);
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
  touchScreen_->present(kTouchWidth, kTouchHeight, colourAt);
}

} // namespace CosmicHex::Handheld::App
