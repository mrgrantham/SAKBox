#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/MoveSpots.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>

#include <algorithm>
#include <cmath>

namespace CosmicHex::Handheld {

namespace {
using Kind = Action::Kind;
using Colour = Canvas::Colour;
constexpr Colour kButton{31, 42, 64, 255};
constexpr Colour kButtonEdge{58, 74, 106, 255};
constexpr Colour kRollButton{32, 51, 74, 255};
constexpr Colour kRollEdge{106, 168, 255, 255};
constexpr Colour kHintBand{10, 15, 26, 255};
constexpr Colour kHint = Palette::kHighlight;
} // namespace

BottomScreen::BottomScreen(int width, int height) : width_(width), height_(height), cards_(width) {}

void BottomScreen::update(const State &state, const std::vector<Action> &legal, bool yourMove,
                          bool yourRollWaits) {
  state_ = state;
  legal_ = legal;
  yourMove_ = yourMove;
  situation_ = BoardButtons::situationOf(state, legal, yourMove, yourRollWaits);
  // (before the roll too: each row then says it waits for the roll)
  rows_ = yourMove ? buildRows(state) : std::vector<BuildRow>{};
  // (your cards: player 1's, the one you play; on your move the turn's)
  held_ = heldCards(state, legal, yourMove ? state.turn : 0, yourMove);
  if (heldChosen_ >= static_cast<int>(held_.size()))
    heldChosen_ = -1;
  if (!yourMove) {
    // (the cards screen stays open to look at, whoever's move it is)
    if (mode_ != Mode::Cards)
      mode_ = Mode::Board;
    startPlacing_ = false;
    startPlacement_.reset();
    return;
  }
  // A choice of resources the rules ask for now: the picker, by itself
  if (auto choice = Pickers::resourceChoiceOf(state, legal)) {
    if (mode_ != Mode::Resources || !resources_ || resources_->title != choice->title) {
      resources_ = std::move(choice);
      picked_.clear();
      resourcesCanGoBack_ = false;
      mode_ = Mode::Resources;
    }
    return;
  }
  if (mode_ == Mode::Resources && !resourcesCanGoBack_)
    mode_ = Mode::Board; // (that choice made)
  // After buying: the two cards dealt, to keep one of, by itself
  if (auto keep = keepChoiceOf(state, legal)) {
    if (mode_ != Mode::Keep) {
      keep_ = std::move(keep);
      keepChosen_ = -1;
      mode_ = Mode::Keep;
    }
    return;
  }
  if (mode_ == Mode::Keep)
    mode_ = Mode::Board; // (kept)
  // The start's placing: every place marked, a touch to choose, End Turn
  // to place (the user's: End Turn puts it down)
  const bool starting = state.phase == Phase::PlaceColony || state.phase == Phase::PlaceShip;
  if (starting) {
    placing_.clear();
    for (const Action &a : legal)
      if (a.kind == Kind::PlaceColony || a.kind == Kind::PlaceShip)
        placing_.push_back(a);
    if (startPlacement_ &&
        std::find(placing_.begin(), placing_.end(), *startPlacement_) == placing_.end())
      startPlacement_.reset();
    startPlacing_ = true;
    mode_ = Mode::Placing;
    situation_.placing = true;
    situation_.placeChosen = startPlacement_.has_value();
    return;
  }
  if (startPlacing_) {
    startPlacing_ = false;
    startPlacement_.reset();
    mode_ = Mode::Board;
  }
  // Placing what was chosen: only while those moves are still the rules'
  if (mode_ == Mode::Placing &&
      std::none_of(placing_.begin(), placing_.end(), [&](const Action &a) {
        return std::find(legal.begin(), legal.end(), a) != legal.end();
      }))
    mode_ = Mode::Board;
}

BottomScreen::Outcome BottomScreen::tap(int x, int y,
                                        const std::optional<BoardPicture::Spot> &spot) {
  using What = ChoiceScreens::Touch::What;
  switch (mode_) {
  case Mode::Board:
  case Mode::Placing: {
    // Any button, usable or not: it flashes, and says why when it can't
    layOutButtons();
    if (const std::optional<int> touched = buttons_.at(x, y)) {
      const auto button = static_cast<BoardButtons::Button>(*touched);
      const bool usable = buttons_.find(*touched)->usable;
      buttons_.flash(*touched);
      switch (button) {
      case BoardButtons::Button::Build:
        if (yourMove_) {
          mode_ = Mode::Build;
          buildChosen_ = -1;
          buttons_.say({});
        } else {
          buttons_.say("Build: on your turn");
        }
        break;
      case BoardButtons::Button::EndTurn:
        if (startPlacing_) {
          if (!startPlacement_) {
            buttons_.say("Touch a marked corner first");
            break;
          }
          const Action placed = *startPlacement_;
          startPlacement_.reset();
          buttons_.say({});
          return {placed, true};
        }
        for (const Action &a : legal_)
          if (a.kind == Kind::EndTurn || a.kind == Kind::EndTrading) {
            buttons_.say({});
            return {a, true};
          }
        break;
      case BoardButtons::Button::Trade:
        buttons_.say(usable ? "Trade: its screen is next to be built" : "Trade: after your roll");
        break;
      case BoardButtons::Button::ShowResources:
        info_ = info_ == Info::Resources ? Info::None : Info::Resources;
        buttons_.say({});
        break;
      case BoardButtons::Button::OtherPlayers:
        buttons_.say("Other players: not built yet");
        break;
      case BoardButtons::Button::Cards:
        if (held_.empty()) {
          buttons_.say("Cards: none yet (buy one from Build)");
        } else {
          mode_ = Mode::Cards;
          buttons_.say({});
        }
        break;
      case BoardButtons::Button::Roll:
        buttons_.say({});
        return {std::nullopt, true, true};
      }
      return {std::nullopt, true};
    }
    if (mode_ == Mode::Placing && startPlacing_) {
      // (a marked corner: shown there, for End Turn to place; it again:
      // taken back; anywhere else: nothing)
      if (spot)
        for (const Action &a : placing_) {
          const MoveSpot where = spotOf(state_, a);
          if (!where.corners.empty() && where.corners.back() == spot->corner) {
            if (startPlacement_ == a)
              startPlacement_.reset();
            else
              startPlacement_ = a;
            situation_.placeChosen = startPlacement_.has_value();
            buttons_.say({});
            return {std::nullopt, true};
          }
        }
      return {std::nullopt, true};
    }
    if (mode_ == Mode::Placing) {
      // (a marked corner: that move; anywhere else: put it down again)
      if (spot)
        for (const Action &a : placing_) {
          const MoveSpot where = spotOf(state_, a);
          if (!where.corners.empty() && where.corners.back() == spot->corner) {
            mode_ = Mode::Board;
            return {a, true};
          }
        }
      mode_ = Mode::Board;
      return {std::nullopt, true};
    }
    // One of your ships touched: where it can go, marked, to touch
    if (yourMove_ && spot) {
      std::vector<Action> moves;
      for (const Action &a : legal_)
        if (a.kind == Kind::Move && a.a >= 0 && a.a < static_cast<int>(state_.ships.size()) &&
            state_.ships[static_cast<std::size_t>(a.a)].corner == spot->corner)
          moves.push_back(a);
      if (!moves.empty()) {
        placing_ = std::move(moves);
        mode_ = Mode::Placing;
        buttons_.say({});
        return {std::nullopt, true};
      }
    }
    // The blockade beside more than one player: whom to steal from
    if (yourMove_ && state_.phase == Phase::Blockade && spot) {
      for (std::size_t p = 0; p < state_.planets.size(); ++p)
        if (state_.planets[p].hex == spot->hex)
          if (auto choice = Pickers::stealChoiceOf(state_, legal_, static_cast<int>(p))) {
            players_ = std::move(choice);
            playerChosen_ = -1;
            cardChosen_ = -1;
            mode_ = Mode::Players;
            return {std::nullopt, true};
          }
    }
    return {};
  }
  case Mode::Build: {
    const auto touch = ChoiceScreens::buildTouch(width_, rows_.size(), x, y);
    if (!touch)
      return {std::nullopt, true};
    if (touch->what == What::Back) {
      mode_ = Mode::Board;
    } else if (touch->what == What::Item) {
      buildChosen_ = touch->item;
    } else if (buildChosen_ >= 0 && buildChosen_ < static_cast<int>(rows_.size())) {
      const BuildRow &row = rows_[static_cast<std::size_t>(buildChosen_)];
      if (row.canBuild && row.actions.size() == 1) {
        mode_ = Mode::Board;
        return {row.actions.front(), true};
      }
      if (row.canBuild) {
        placing_ = row.actions; // (more than one place: chosen on the board)
        mode_ = Mode::Placing;
      }
    }
    return {std::nullopt, true};
  }
  case Mode::Resources: {
    const auto touch = ChoiceScreens::resourcesTouch(width_, x, y);
    if (!touch || !resources_)
      return {std::nullopt, true};
    if (touch->what == What::Back && resourcesCanGoBack_) {
      mode_ = Mode::Cards; // (opened from the cards screen: back to it)
    } else if (touch->what == What::Item) {
      const auto resource = static_cast<Resource>(touch->item);
      if (resources_->offers(resource)) {
        if (resources_->howMany == 1 || static_cast<int>(picked_.size()) >= resources_->howMany)
          picked_.clear();
        picked_.push_back(resource);
      }
    } else if (touch->what == What::Confirm) {
      if (const auto action = resources_->actionFor(picked_)) {
        mode_ = Mode::Board;
        return {*action, true};
      }
    }
    return {std::nullopt, true};
  }
  case Mode::Players: {
    const auto touch = players_
                           ? ChoiceScreens::playersTouch(width_, players_->options.size(), x, y)
                           : std::nullopt;
    if (!touch)
      return {std::nullopt, true};
    if (touch->what == What::Item) {
      playerChosen_ = touch->item;
      const int cards = players_->options[static_cast<std::size_t>(playerChosen_)].cards;
      cardChosen_ = std::clamp(touch->card, 0, std::max(0, std::min(cards, 16) - 1));
    } else if (touch->what == What::Confirm && playerChosen_ >= 0 && cardChosen_ >= 0) {
      mode_ = Mode::Board;
      return {players_->options[static_cast<std::size_t>(playerChosen_)].action, true};
    }
    return {std::nullopt, true};
  }
  case Mode::Cards: {
    const auto touch = ChoiceScreens::cardsTouch(width_, held_.size(), x, y);
    if (!touch)
      return {std::nullopt, true};
    if (touch->what == What::Item)
      heldChosen_ = touch->item;
    else if (touch->what == What::Back)
      mode_ = Mode::Board;
    else if (touch->what == What::Confirm)
      return useChosenCard();
    return {std::nullopt, true};
  }
  case Mode::Keep: {
    const auto touch =
        keep_ ? ChoiceScreens::keepTouch(width_, keep_->cards.size(), x, y) : std::nullopt;
    if (!touch)
      return {std::nullopt, true};
    if (touch->what == What::Item)
      keepChosen_ = touch->item;
    else if (touch->what == What::Confirm && keepChosen_ >= 0)
      return {keep_->actions[static_cast<std::size_t>(keepChosen_)], true};
    return {std::nullopt, true};
  }
  }
  return {};
}

BottomScreen::Outcome BottomScreen::useChosenCard() {
  if (heldChosen_ < 0 || heldChosen_ >= static_cast<int>(held_.size()))
    return {std::nullopt, true};
  const HeldCard &held = held_[static_cast<std::size_t>(heldChosen_)];
  if (!held.playable)
    return {std::nullopt, true};
  // The Cartel's resource or the Supply drop's two: the picker, which can go
  // back to the cards
  if (auto choice = Pickers::cardChoiceOf(legal_, held.kind)) {
    resources_ = std::move(choice);
    picked_.clear();
    resourcesCanGoBack_ = true;
    mode_ = Mode::Resources;
    return {std::nullopt, true};
  }
  // Shipyard Surplus: your colonies marked, one touched for the ships
  if (held.kind == Kind::PlayShipyard) {
    placing_.clear();
    for (const Action &a : legal_)
      if (a.kind == Kind::PlayShipyard)
        placing_.push_back(a);
    mode_ = Mode::Placing;
    return {std::nullopt, true};
  }
  // The Enforcer (then the blockade's moved on the board) or a Charter shown
  for (const Action &a : legal_)
    if (a.kind == held.kind && (!held.charter || a.a == held.contract)) {
      mode_ = Mode::Board;
      heldChosen_ = -1;
      return {a, true};
    }
  return {std::nullopt, true};
}

BottomScreen::Outcome BottomScreen::press(Key key) {
  const auto step = [](int at, int by, int count) {
    return count <= 0 ? -1 : std::clamp(at + by, 0, count - 1);
  };
  switch (mode_) {
  case Mode::Board:
    return {};
  case Mode::Placing:
    if (startPlacing_) {
      // (A places what's been touched, as End Turn; B takes it back)
      if (key == Key::Confirm && startPlacement_) {
        const Action placed = *startPlacement_;
        startPlacement_.reset();
        return {placed, true};
      }
      if (key == Key::Back)
        startPlacement_.reset();
      situation_.placeChosen = startPlacement_.has_value();
      return {std::nullopt, key == Key::Back};
    }
    if (key != Key::Back)
      return {};
    mode_ = Mode::Board;
    return {std::nullopt, true};
  case Mode::Build: {
    const int count = static_cast<int>(rows_.size());
    if (key == Key::Up || key == Key::Down) {
      buildChosen_ = buildChosen_ < 0 ? 0 : step(buildChosen_, key == Key::Up ? -1 : 1, count);
    } else if (key == Key::Back) {
      mode_ = Mode::Board;
    } else if (key == Key::Confirm && buildChosen_ < 0) {
      buildChosen_ = 0;
    } else if (key == Key::Confirm) {
      // (as touching Build: the same outcome)
      const float k = static_cast<float>(width_) / 256;
      return tap(static_cast<int>(220 * k), static_cast<int>(178 * k), std::nullopt);
    }
    return {std::nullopt, true};
  }
  case Mode::Resources: {
    if (!resources_)
      return {std::nullopt, true};
    if (key == Key::Left || key == Key::Right) {
      resourceFocus_ = step(resourceFocus_, key == Key::Left ? -1 : 1, kResources);
    } else if (key == Key::Back && resourcesCanGoBack_) {
      mode_ = Mode::Cards; // (opened from the cards screen: back to it)
    } else if (key == Key::Confirm) {
      // A picks the card in focus; once enough are picked, A takes them
      if (static_cast<int>(picked_.size()) == resources_->howMany) {
        if (const auto action = resources_->actionFor(picked_)) {
          mode_ = Mode::Board;
          return {*action, true};
        }
      }
      const auto resource = static_cast<Resource>(resourceFocus_);
      if (resources_->offers(resource)) {
        if (static_cast<int>(picked_.size()) >= resources_->howMany)
          picked_.clear();
        picked_.push_back(resource);
      }
    }
    return {std::nullopt, true};
  }
  case Mode::Players: {
    if (!players_)
      return {std::nullopt, true};
    const int count = static_cast<int>(players_->options.size());
    if (playerChosen_ < 0) {
      playerChosen_ = 0;
      cardChosen_ = 0;
    }
    const int cards =
        std::max(1, std::min(players_->options[static_cast<std::size_t>(playerChosen_)].cards, 16));
    if (key == Key::Up || key == Key::Down) {
      playerChosen_ = step(playerChosen_, key == Key::Up ? -1 : 1, count);
      cardChosen_ = 0;
    } else if (key == Key::Left || key == Key::Right) {
      cardChosen_ = step(cardChosen_, key == Key::Left ? -1 : 1, cards);
    } else if (key == Key::Confirm) {
      mode_ = Mode::Board;
      return {players_->options[static_cast<std::size_t>(playerChosen_)].action, true};
    }
    return {std::nullopt, true};
  }
  case Mode::Cards: {
    // (the D-pad across and down the grid of four a row; A uses the chosen)
    const int count = static_cast<int>(held_.size());
    if (key == Key::Back) {
      mode_ = Mode::Board;
    } else if (key == Key::Confirm && heldChosen_ >= 0) {
      return useChosenCard();
    } else if (count > 0) {
      const int at = std::max(heldChosen_, 0);
      const int by = key == Key::Left ? -1 : key == Key::Right ? 1 : key == Key::Up ? -4 : 4;
      heldChosen_ = heldChosen_ < 0 ? 0 : std::clamp(at + by, 0, count - 1);
    }
    return {std::nullopt, true};
  }
  case Mode::Keep: {
    if (!keep_)
      return {std::nullopt, true};
    const int count = static_cast<int>(keep_->cards.size());
    if (key == Key::Left || key == Key::Right)
      keepChosen_ = keepChosen_ < 0 ? 0 : step(keepChosen_, key == Key::Left ? -1 : 1, count);
    else if (key == Key::Confirm && keepChosen_ >= 0)
      return {keep_->actions[static_cast<std::size_t>(keepChosen_)], true};
    return {std::nullopt, true};
  }
  }
  return {};
}

void BottomScreen::clearFeedback() { buttons_.clearFeedback(); }

void BottomScreen::layOutButtons() {
  std::vector<ScratchPadd::Buttons::Button> buttons;
  for (const BoardButtons::Place &place : BoardButtons::layout(width_, height_, situation_)) {
    // Its icon (art/icons), drawn at the button's size the first time
    const int icon = static_cast<int>(BoardButtons::iconOf(place.button));
    const int inner = static_cast<int>(place.size) - 4;
    if (iconSize_ != inner) {
      for (auto &sprite : icons_)
        sprite.clear();
      iconSize_ = inner;
    }
    auto &sprite = icons_[static_cast<std::size_t>(icon)];
    if (sprite.empty())
      sprite = recipePixels(Art::iconRecipeOf(static_cast<Art::Icon>(icon)), inner);
    const bool roll = place.button == BoardButtons::Button::Roll; // (Roll stands out)
    buttons.push_back({.id = static_cast<int>(place.button),
                       .area = {place.x, place.y, place.size, place.size},
                       .icon = &sprite,
                       .iconSize = inner,
                       .usable = place.usable,
                       .face = roll ? std::optional(kRollButton) : std::nullopt,
                       .edge = roll ? std::optional(kRollEdge) : std::nullopt});
  }
  buttons_.set(std::move(buttons));
  // (a tapped button's message in a band at the bottom; while placing, the
  // placing's band shows it)
  const float k = static_cast<float>(width_) / 256;
  buttons_.setMessageArea(mode_ == Mode::Board ? std::optional(ScratchPadd::Buttons::Area{
                                                     30 * k, static_cast<float>(height_) - 18 * k,
                                                     static_cast<float>(width_) - 60 * k, 14 * k})
                                               : std::nullopt);
}

std::vector<Corner> BottomScreen::placingCorners() const {
  std::vector<Corner> out;
  if (mode_ != Mode::Placing)
    return out;
  for (const Action &a : placing_) {
    const MoveSpot spot = spotOf(state_, a);
    // (a ship's build or move: where it goes, the last corner the move names)
    if (!spot.corners.empty())
      out.push_back(spot.corners.back());
  }
  return out;
}

void BottomScreen::draw(Canvas &canvas) {
  switch (mode_) {
  case Mode::Build:
    ChoiceScreens::drawBuild(canvas, rows_, buildChosen_);
    return;
  case Mode::Resources:
    if (resources_)
      ChoiceScreens::drawResources(canvas, *resources_, picked_,
                                   state_.player[static_cast<std::size_t>(state_.turn)].resources,
                                   resourcesCanGoBack_, cards_, resourceFocus_);
    return;
  case Mode::Players:
    if (players_)
      ChoiceScreens::drawPlayers(canvas, *players_, playerChosen_, cardChosen_, Art::kResourceBack,
                                 cards_);
    return;
  case Mode::Cards:
    ChoiceScreens::drawCards(canvas, held_, heldChosen_, cards_);
    return;
  case Mode::Keep:
    if (keep_)
      ChoiceScreens::drawKeep(canvas, *keep_, keepChosen_, cards_);
    return;
  case Mode::Board:
  case Mode::Placing:
    break;
  }
}

void BottomScreen::drawOverBoard(Canvas &canvas) {
  // The buttons over the board, and a tapped one's message
  layOutButtons();
  buttons_.draw(canvas, {.face = kButton,
                         .edge = kButtonEdge,
                         .flash = kHint,
                         .band = kHintBand,
                         .message = kHint,
                         .dimmed = 0.35f});
  if (mode_ == Mode::Placing) {
    const float k = static_cast<float>(width_) / 256;
    // (between the bottom corners' buttons, Roll's and End Turn's)
    canvas.fillRect(28 * k, static_cast<float>(height_) - 16 * k,
                    static_cast<float>(width_) - 56 * k, 16 * k, kHintBand);
    const char *hint = "Where to? Touch a marked corner";
    if (startPlacing_ && state_.phase == Phase::PlaceColony)
      hint = startPlacement_ ? "End Turn: place your colony" : "Home colony: touch a corner";
    else if (startPlacing_)
      hint = startPlacement_ ? "End Turn: place your ship" : "Your ship: touch a corner";
    if (!buttons_.message().empty())
      hint = buttons_.message().c_str(); // (a tapped button's message, in the band's place)
    canvas.textCentred(static_cast<float>(width_) / 2, static_cast<float>(height_) - 8 * k, hint,
                       kHint);
  }
}

std::vector<BoardPicture::Area> BottomScreen::overBoardAreas() const {
  // (each a pixel or two bigger all round: a flashing button's outline is
  // outside it, and the rounding of fractional sizes)
  const auto area = [](float x, float y, float w, float h) {
    const int x0 = static_cast<int>(std::floor(x)) - 2, y0 = static_cast<int>(std::floor(y)) - 2;
    return BoardPicture::Area{x0, y0, static_cast<int>(std::ceil(x + w)) + 2 - x0,
                              static_cast<int>(std::ceil(y + h)) + 2 - y0};
  };
  std::vector<BoardPicture::Area> out;
  for (const BoardButtons::Place &place : BoardButtons::layout(width_, height_, situation_))
    out.push_back(area(place.x, place.y, place.size, place.size));
  const float k = static_cast<float>(width_) / 256;
  if (!buttons_.message().empty() && mode_ == Mode::Board)
    out.push_back(area(30 * k, static_cast<float>(height_) - 18 * k,
                       static_cast<float>(width_) - 60 * k, 14 * k));
  if (mode_ == Mode::Placing)
    out.push_back(area(28 * k, static_cast<float>(height_) - 16 * k,
                       static_cast<float>(width_) - 56 * k, 16 * k));
  return out;
}

bool BottomScreen::prepare() {
  if (!cards_.prepare())
    return false;
  const int inner = static_cast<int>(22.0f * static_cast<float>(width_) / 256.0f) - 4;
  if (iconSize_ != inner) {
    for (auto &sprite : icons_)
      sprite.clear();
    iconSize_ = inner;
  }
  for (int i = 0; i < Art::kIcons; ++i)
    if (icons_[static_cast<std::size_t>(i)].empty()) {
      icons_[static_cast<std::size_t>(i)] =
          recipePixels(Art::iconRecipeOf(static_cast<Art::Icon>(i)), inner);
      return false;
    }
  return true;
}

} // namespace CosmicHex::Handheld
