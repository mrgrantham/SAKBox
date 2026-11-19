#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/MoveSpots.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>

#include <algorithm>

namespace CosmicHex::Handheld {

namespace {
using Kind = Action::Kind;
using Colour = Canvas::Colour;
constexpr Colour kButton{31, 42, 64, 255};
constexpr Colour kButtonEdge{58, 74, 106, 255};
constexpr Colour kRollButton{32, 51, 74, 255};
constexpr Colour kRollEdge{106, 168, 255, 255};
constexpr Colour kHintBand{10, 15, 26, 255};
constexpr Colour kHint{255, 214, 90, 255};
} // namespace

BottomScreen::BottomScreen(int width, int height) : width_(width), height_(height), cards_(width) {}

void BottomScreen::update(const State &state, const std::vector<Action> &legal, bool yourMove) {
  state_ = state;
  legal_ = legal;
  yourMove_ = yourMove;
  situation_ = BoardButtons::situationOf(state, legal, yourMove);
  rows_ = yourMove && state.phase == Phase::Main ? buildRows(state) : std::vector<BuildRow>{};
  if (!yourMove) {
    mode_ = Mode::Board;
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
  // Placing what was chosen: only while those moves are still the rules'
  if (mode_ == Mode::Placing &&
      std::none_of(placing_.begin(), placing_.end(), [&](const Action &a) {
        return std::find(legal.begin(), legal.end(), a) != legal.end();
      }))
    mode_ = Mode::Board;
  if (mode_ == Mode::Build && state.phase != Phase::Main)
    mode_ = Mode::Board;
}

BottomScreen::Outcome BottomScreen::tap(int x, int y,
                                        const std::optional<BoardPicture::Spot> &spot) {
  using What = ChoiceScreens::Touch::What;
  switch (mode_) {
  case Mode::Board:
  case Mode::Placing: {
    const auto buttons = BoardButtons::layout(width_, height_, situation_);
    if (const auto button = BoardButtons::at(buttons, x, y)) {
      if (*button == BoardButtons::Button::Build) {
        mode_ = Mode::Build;
        buildChosen_ = -1;
      } else if (*button == BoardButtons::Button::EndTurn) {
        for (const Action &a : legal_)
          if (a.kind == Kind::EndTurn || a.kind == Kind::EndTrading)
            return {a, true};
      }
      return {std::nullopt, true}; // (the other buttons' screens come next)
    }
    if (mode_ == Mode::Placing) {
      if (spot)
        for (const Action &a : placing_)
          if (concerns(state_, a, *spot)) {
            mode_ = Mode::Board;
            return {a, true};
          }
      return {std::nullopt, true};
    }
    // The blockade beside more than one player: whom to steal from
    if (yourMove_ && state_.phase == Phase::Blockade && spot) {
      for (std::size_t p = 0; p < state_.planets.size(); ++p)
        if (state_.planets[p].hex == spot->hex)
          if (auto choice = Pickers::stealChoiceOf(state_, legal_, static_cast<int>(p))) {
            players_ = std::move(choice);
            playerChosen_ = -1;
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
      mode_ = Mode::Board;
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
    } else if (touch->what == What::Confirm && playerChosen_ >= 0) {
      mode_ = Mode::Board;
      return {players_->options[static_cast<std::size_t>(playerChosen_)].action, true};
    }
    return {std::nullopt, true};
  }
  }
  return {};
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

void BottomScreen::draw(Canvas &canvas, const std::vector<Canvas::Colour> &board) {
  switch (mode_) {
  case Mode::Build:
    ChoiceScreens::drawBuild(canvas, rows_, buildChosen_);
    return;
  case Mode::Resources:
    if (resources_)
      ChoiceScreens::drawResources(canvas, *resources_, picked_,
                                   state_.player[static_cast<std::size_t>(state_.turn)].resources,
                                   resourcesCanGoBack_, cards_);
    return;
  case Mode::Players:
    if (players_)
      ChoiceScreens::drawPlayers(canvas, *players_, playerChosen_, Art::CardBack::Rings, cards_);
    return;
  case Mode::Board:
  case Mode::Placing:
    break;
  }
  // The board, then its buttons over it
  canvas.copy(board);
  for (const BoardButtons::Place &place : BoardButtons::layout(width_, height_, situation_)) {
    const bool roll = place.button == BoardButtons::Button::Roll;
    canvas.fillRect(place.x, place.y, place.size, place.size, roll ? kRollButton : kButton);
    canvas.outline(place.x, place.y, place.size, place.size, roll ? kRollEdge : kButtonEdge);
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
    canvas.blend(sprite, inner, inner, static_cast<int>(place.x) + 2, static_cast<int>(place.y) + 2,
                 place.usable ? 1.0f : 0.35f);
  }
  if (mode_ == Mode::Placing) {
    const float k = static_cast<float>(width_) / 256;
    canvas.fillRect(0, static_cast<float>(height_) - 16 * k, static_cast<float>(width_), 16 * k,
                    kHintBand);
    canvas.textCentred(static_cast<float>(width_) / 2, static_cast<float>(height_) - 8 * k,
                       "Where? Touch a marked corner", kHint);
  }
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
