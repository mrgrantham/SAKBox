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

// A choosing screen's touch on its Back, go or Decline button, remembered
// with when, for that button to glow and sink a moment
void rememberActionTouch(ChoiceScreens::Press &press,
                         const std::optional<ChoiceScreens::Touch> &touch, double now) {
  using What = ChoiceScreens::Touch::What;
  if (touch && touch->what == What::Back)
    press = {ChoiceScreens::ActionButton::Back, now};
  else if (touch && touch->what == What::Confirm)
    press = {ChoiceScreens::ActionButton::Confirm, now};
}
void rememberActionTouch(ChoiceScreens::Press &press,
                         const std::optional<ChoiceScreens::TradeTouch> &touch, double now) {
  using What = ChoiceScreens::TradeTouch::What;
  if (touch && touch->what == What::Back)
    press = {ChoiceScreens::ActionButton::Back, now};
  else if (touch && touch->what == What::Confirm)
    press = {ChoiceScreens::ActionButton::Confirm, now};
  else if (touch && touch->what == What::Decline)
    press = {ChoiceScreens::ActionButton::Decline, now};
}
} // namespace

BottomScreen::BottomScreen(int width, int height)
    : width_(width), height_(height), cards_(width), bigCards_(width, kCardsScreenWidth) {}

void BottomScreen::update(const State &state, const std::vector<Action> &legal, bool yourMove,
                          bool yourRollWaits) {
  // A screen you're looking at (your cards, the other players) stays while
  // the game moves on under it (opening one holds the game, which itself
  // brings a new look at it); everything else is brought up to date
  const Mode looking = mode_;
  follow(state, legal, yourMove, yourRollWaits);
  if (looking == Mode::Cards || looking == Mode::Others)
    mode_ = looking;
}

void BottomScreen::follow(const State &state, const std::vector<Action> &legal, bool yourMove,
                          bool yourRollWaits) {
  state_ = state;
  legal_ = legal;
  yourMove_ = yourMove;
  situation_ = BoardButtons::situationOf(state, legal, yourMove, yourRollWaits);
  situation_.moveListButton = moveListButton_;
  // (before the roll too: each row then says it waits for the roll)
  rows_ = yourMove ? buildRows(state) : std::vector<BuildRow>{};
  // (the other players, as everyone sees them: from player 1, yours)
  others_.clear();
  for (const int p : othersOf(state, 0))
    others_.push_back(viewOf(state, p));
  trade_.update(state, legal, yourMove);
  // (your cards: player 1's, the one you play; on your move the turn's)
  held_ = heldCards(state, legal, yourMove ? state.turn : 0, yourMove);
  if (heldChosen_ >= static_cast<int>(held_.size()))
    heldChosen_ = -1;
  if (!yourMove) {
    // (the cards screen and the other players stay open to look at,
    // whoever's move it is)
    if (mode_ != Mode::Cards && mode_ != Mode::Others)
      mode_ = Mode::Board;
    startPlacing_ = false;
    chosen_.reset();
    return;
  }
  // An offer to you, or a counter to yours: the trade room, by itself, to
  // answer it
  if (trade_.answering()) {
    mode_ = Mode::Trade;
    return;
  }
  // A choice of resources the rules ask for now: the picker, by itself
  if (auto choice = Pickers::resourceChoiceOf(state, legal)) {
    // (each choice its own: the next of two in a row opens afresh)
    if (mode_ != Mode::Resources || !resources_ || !resources_->sameAs(*choice))
      openPicker(std::move(*choice), false);
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
  // The blockade (a 7, or an Enforcer): placed as everything else is
  if (state.phase == Phase::Blockade) {
    followBlockade(state, legal);
    return;
  }
  if (blockadePlacing_ || blockadeSaid_) {
    blockadePlacing_ = blockadeSaid_ = false;
    chosen_.reset();
    if (mode_ == Mode::Placing)
      mode_ = Mode::Board;
  }
  // The start's placing: every place marked, a touch to choose, End Turn
  // to place (the user's: End Turn puts it down)
  const bool starting = state.phase == Phase::PlaceColony || state.phase == Phase::PlaceShip;
  if (starting) {
    placing_.clear();
    for (const Action &a : legal)
      if (a.kind == Kind::PlaceColony || a.kind == Kind::PlaceShip)
        placing_.push_back(a);
    if (chosen_ && std::find(placing_.begin(), placing_.end(), *chosen_) == placing_.end())
      chosen_.reset();
    startPlacing_ = true;
    placingShip_ = -1; // (not a ship's moves)
    mode_ = Mode::Placing;
    return;
  }
  if (startPlacing_) {
    startPlacing_ = false;
    chosen_.reset();
    mode_ = Mode::Board;
  }
  if (chosen_ && std::find(placing_.begin(), placing_.end(), *chosen_) == placing_.end())
    chosen_.reset();
  // Placing what was chosen: only while those moves are still the rules'
  // (or will be, once the turn's offers to players end)
  if (mode_ == Mode::Placing && !placingEndsTrading_ &&
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
      buttons_.press(*touched, now_);
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
      case BoardButtons::Button::Confirm:
        if (!chosen_) {
          buttons_.say("Touch a marked corner first");
          break;
        }
        buttons_.say({});
        return confirmChosen();
      case BoardButtons::Button::EndTurn:
        if (situation_.newShipMustMove) {
          buttons_.say("Move your new ship off its colony first");
          break;
        }
        for (const Action &a : legal_)
          if (a.kind == Kind::EndTurn || a.kind == Kind::EndTrading) {
            buttons_.say({});
            return {a, true};
          }
        break;
      case BoardButtons::Button::Trade:
        if (usable) {
          mode_ = Mode::Trade;
          buttons_.say({});
        } else {
          buttons_.say(yourMove_ ? "Trade: after your roll" : "Trade: on your turn");
        }
        break;
      case BoardButtons::Button::ShowResources:
        info_ = info_ == Info::Resources ? Info::None : Info::Resources;
        buttons_.say({});
        break;
      case BoardButtons::Button::OtherPlayers:
        if (others_.empty())
          break;
        mode_ = Mode::Others;
        info_ = Info::Player;
        if (othersChosen_ < 0)
          othersChosen_ = others_.front().player;
        buttons_.say({});
        break;
      case BoardButtons::Button::Cards:
        if (held_.empty()) {
          buttons_.say("Cards: none yet (buy one from Build)");
        } else {
          mode_ = Mode::Cards;
          buttons_.say({});
        }
        break;
      case BoardButtons::Button::MoveList:
        moveList_ = !moveList_;
        buttons_.say(moveList_ ? "Move list: on (debug)" : "Move list: off");
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
            if (chosen_ == a)
              chosen_.reset();
            else
              chosen_ = a;
            buttons_.say({});
            return {std::nullopt, true};
          }
        }
      chosen_.reset();
      return {std::nullopt, false}; // (elsewhere: the app's, its pointer goes there)
    }
    if (mode_ == Mode::Placing && blockadePlacing_) {
      // (a marked planet: chosen, for Confirm; it again: put down; it has
      // to go somewhere, so a touch elsewhere only says so)
      if (spot)
        for (const Action &a : placing_)
          if (state_.planets[static_cast<std::size_t>(a.a)].hex == spot->hex) {
            if (chosen_ == a)
              chosen_.reset();
            else
              chosen_ = a;
            buttons_.say({});
            return {std::nullopt, true};
          }
      buttons_.say("Touch a marked planet for the blockade");
      return {std::nullopt, true};
    }
    if (mode_ == Mode::Placing) {
      // (a marked corner: chosen, for Confirm; it again: put down)
      if (spot)
        for (const Action &a : placing_) {
          const MoveSpot where = spotOf(state_, a);
          if (!where.corners.empty() && where.corners.back() == spot->corner) {
            if (chosen_ == a)
              chosen_.reset();
            else
              chosen_ = a;
            buttons_.say({});
            return {std::nullopt, true};
          }
        }
      // (anywhere else: the ship or build put down, and the touch goes on:
      // another of your ships, or the app's pointer)
      mode_ = Mode::Board;
      chosen_.reset();
    }
    // One of your ships touched: where it can go, marked, to touch. While
    // the turn's offers to players are open (the trading phase, after the
    // roll), its moves as they'll be once they end: choosing one ends them
    // first, as a build does (the user's: touching the ship did nothing)
    if (yourMove_ && spot) {
      const auto shipHere =
          std::find_if(state_.ships.begin(), state_.ships.end(), [&](const Ship &ship) {
            return ship.owner == state_.turn && ship.corner == spot->corner;
          });
      if (shipHere != state_.ships.end()) {
        const bool trading = state_.phase == Phase::Trading;
        std::vector<Action> legal = legal_;
        if (trading) {
          State after = state_;
          Game::apply(after, Action{Kind::EndTrading});
          legal = Game::legalActions(after);
        }
        const auto index = static_cast<int>(shipHere - state_.ships.begin());
        std::vector<Action> moves;
        std::vector<bool> fuel;
        State from = state_;
        if (trading)
          Game::apply(from, Action{Kind::EndTrading});
        const auto fuelOf = [](const State &s) {
          return s.player[static_cast<std::size_t>(s.turn)]
              .resources[static_cast<std::size_t>(Resource::Fuel)];
        };
        for (const Action &a : legal)
          if (a.kind == Kind::Move && a.a == index) {
            moves.push_back(a);
            // (the boost: moving there costs a fuel)
            State after = from;
            Game::apply(after, a);
            fuel.push_back(fuelOf(after) < fuelOf(from));
          }
        if (!moves.empty()) {
          placing_ = std::move(moves);
          placingFuel_ = std::move(fuel);
          placingShip_ = index;
          placingEndsTrading_ = trading;
          mode_ = Mode::Placing;
          buttons_.say({});
          return {std::nullopt, true};
        }
        // (why not, rather than nothing)
        const bool beforeRoll = state_.phase == Phase::HomeIncome ||
                                state_.phase == Phase::HomeDieDraw ||
                                state_.phase == Phase::RollDraw;
        buttons_.say(beforeRoll        ? "Roll first, then move"
                     : shipHere->moved ? "That ship has moved this turn"
                                       : "That ship can't move now");
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
    rememberActionTouch(actionPress_, touch, now_);
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
        return build(row.actions.front(), row.endsTrading);
      }
      if (row.canBuild) {
        placing_ = row.actions; // (more than one place: chosen on the board)
        placingEndsTrading_ = row.endsTrading;
        placingShip_ = -1; // (not a ship's moves)
        mode_ = Mode::Placing;
      }
    }
    return {std::nullopt, true};
  }
  case Mode::Resources: {
    const auto touch = ChoiceScreens::resourcesTouch(width_, x, y);
    rememberActionTouch(actionPress_, touch, now_);
    if (!touch || !resources_)
      return {std::nullopt, true};
    if (touch->what == What::Back && resourcesCanGoBack_) {
      mode_ = Mode::Cards; // (opened from the cards screen: back to it)
    } else if (touch->what == What::Item) {
      const auto resource = static_cast<Resource>(touch->item);
      if (resources_->offers(resource)) {
        resourceFocus_ = touch->item;
        if (resources_->howMany == 1 || static_cast<int>(picked_.size()) >= resources_->howMany)
          picked_.clear();
        picked_.push_back(resource);
      }
    } else if (touch->what == What::Confirm) {
      if (const auto action = resources_->actionFor(picked_)) {
        mode_ = Mode::Board;
        return {action, true};
      }
    }
    return {std::nullopt, true};
  }
  case Mode::Players: {
    const auto touch = players_
                           ? ChoiceScreens::playersTouch(width_, players_->options.size(), x, y)
                           : std::nullopt;
    rememberActionTouch(actionPress_, touch, now_);
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
    rememberActionTouch(actionPress_, touch, now_);
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
  case Mode::Trade: {
    using TradeWhat = ChoiceScreens::TradeTouch::What;
    const auto touch = ChoiceScreens::tradeTouch(width_, trade_, x, y);
    rememberActionTouch(actionPress_, touch, now_);
    if (!touch)
      return {std::nullopt, true};
    switch (touch->what) {
    case TradeWhat::Partner:
      // (a closed one says why)
      if (trade_.open(touch->partner))
        trade_.choosePartner(touch->partner);
      break;
    case TradeWhat::Add:
      trade_.add(touch->side, touch->resource);
      break;
    case TradeWhat::Remove:
      trade_.remove(touch->side, touch->resource);
      break;
    case TradeWhat::Back:
      mode_ = Mode::Board;
      break;
    case TradeWhat::Confirm:
      return confirmTrade();
    case TradeWhat::Decline:
      if (const auto decline = trade_.decline())
        return {decline, true};
      break;
    }
    return {std::nullopt, true};
  }
  case Mode::Others: {
    const auto touch = ChoiceScreens::othersTouch(width_, others_, x, y);
    rememberActionTouch(actionPress_, touch, now_);
    if (touch && touch->what == What::Item)
      othersChosen_ = touch->item;
    else if (touch && touch->what == What::Back)
      closeOthers();
    return {std::nullopt, true};
  }
  case Mode::Keep: {
    const auto touch =
        keep_ ? ChoiceScreens::keepTouch(width_, keep_->cards.size(), x, y) : std::nullopt;
    rememberActionTouch(actionPress_, touch, now_);
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

BottomScreen::Outcome BottomScreen::build(const Action &action, bool endsTrading) {
  placingEndsTrading_ = false;
  if (!endsTrading)
    return {action, true};
  // (the turn's offers to players end first; the build once that's done:
  // nextQueued)
  queued_ = {action};
  return {Action{Action::Kind::EndTrading}, true};
}

BottomScreen::Outcome BottomScreen::confirmTrade() {
  if (trade_.answering()) {
    if (const auto accept = trade_.accept())
      return {accept, true};
    return {std::nullopt, true};
  }
  std::vector<Action> moves = trade_.moves();
  if (moves.empty())
    return {std::nullopt, true};
  // (the first now, the rest once the rules allow them: nextQueued)
  const Action first = moves.front();
  queued_.assign(moves.begin() + 1, moves.end());
  return {first, true};
}

std::optional<Action> BottomScreen::nextQueued() {
  if (queued_.empty() || std::find(legal_.begin(), legal_.end(), queued_.front()) == legal_.end())
    return std::nullopt;
  const Action next = queued_.front();
  queued_.erase(queued_.begin());
  return next;
}

void BottomScreen::closeOthers() {
  mode_ = Mode::Board;
  info_ = Info::None;
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
    openPicker(std::move(*choice), true);
    return {std::nullopt, true};
  }
  // Shipyard Surplus: your colonies marked, one touched for the ships
  if (held.kind == Kind::PlayShipyard) {
    placing_.clear();
    for (const Action &a : legal_)
      if (a.kind == Kind::PlayShipyard)
        placing_.push_back(a);
    placingShip_ = -1; // (not a ship's moves)
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

BottomScreen::Outcome BottomScreen::confirmChosen() {
  if (!chosen_)
    return {std::nullopt, true};
  const Action chosen = *chosen_;
  chosen_.reset();
  if (blockadePlacing_) {
    // (more than one player there: whom to steal from, next)
    if (auto choice = Pickers::stealChoiceOf(state_, legal_, chosen.a)) {
      players_ = std::move(choice);
      playerChosen_ = -1;
      cardChosen_ = -1;
      mode_ = Mode::Players;
      return {std::nullopt, true};
    }
    for (const Action &a : legal_)
      if (a.kind == Kind::MoveBlockade && a.a == chosen.a)
        return {a, true};
    return {std::nullopt, true};
  }
  if (startPlacing_)
    return {chosen, true};
  mode_ = Mode::Board;
  return build(chosen, placingEndsTrading_);
}

void BottomScreen::followBlockade(const State &state, const std::vector<Action> &legal) {
  // One place a planet it can go to (whom to steal from comes after)
  placing_.clear();
  for (const Action &a : legal)
    if (a.kind == Kind::MoveBlockade && a.a >= 0 &&
        std::none_of(placing_.begin(), placing_.end(), [&](const Action &p) { return p.a == a.a; }))
      placing_.push_back(a);
  if (placing_.empty()) {
    // (nowhere to go, no planet turned over yet: it stays, said once, and
    // the game goes on by itself: the only move, made for you)
    if (!blockadeSaid_) {
      blockadeSaid_ = true;
      queued_.push_back({Kind::MoveBlockade, -1, -1});
      buttons_.say("No planet turned over yet: the blockade stays");
    }
    return;
  }
  if (chosen_ && std::none_of(placing_.begin(), placing_.end(),
                              [&](const Action &p) { return p.a == chosen_->a; }))
    chosen_.reset();
  blockadePlacing_ = true;
  startPlacing_ = false;
  placingShip_ = -1;
  placingEndsTrading_ = false;
  if (mode_ != Mode::Players) // (choosing whom to steal from: stays)
    mode_ = Mode::Placing;
}

std::vector<Hex> BottomScreen::placingHexes() const {
  std::vector<Hex> out;
  if (!blockadePlacing_ || mode_ != Mode::Placing)
    return out;
  for (const Action &a : placing_)
    out.push_back(state_.planets[static_cast<std::size_t>(a.a)].hex);
  return out;
}

BottomScreen::Outcome BottomScreen::press(Key key) {
  const auto step = [](int at, int by, int count) {
    return count <= 0 ? -1 : std::clamp(at + by, 0, count - 1);
  };
  switch (mode_) {
  case Mode::Board:
    return {};
  case Mode::Placing:
    // (A places what's been touched, as Confirm; B puts it down, and then
    // the ship or build itself)
    if (key == Key::Confirm && chosen_)
      return confirmChosen();
    if (key != Key::Back)
      return {};
    if (chosen_)
      chosen_.reset();
    else if (!startPlacing_ && !blockadePlacing_) // (those must be placed)
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
      focusNextResource(key == Key::Left ? -1 : 1);
    } else if (key == Key::Back && resourcesCanGoBack_) {
      mode_ = Mode::Cards; // (opened from the cards screen: back to it)
    } else if (key == Key::Confirm) {
      // A picks the card in focus; once enough are picked, A takes them
      if (static_cast<int>(picked_.size()) == resources_->howMany) {
        if (const auto action = resources_->actionFor(picked_)) {
          mode_ = Mode::Board;
          return {action, true};
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
  case Mode::Trade: {
    // (left and right through the open tabs; A trades or accepts; B back)
    if (key == Key::Confirm)
      return confirmTrade();
    if (key == Key::Back && !trade_.answering()) {
      mode_ = Mode::Board;
    } else if ((key == Key::Left || key == Key::Right) && !trade_.answering()) {
      std::vector<int> open;
      for (const int p : trade_.partners())
        if (trade_.open(p))
          open.push_back(p);
      const auto at = std::find(open.begin(), open.end(), trade_.partner());
      const int i = at == open.end() ? 0 : static_cast<int>(at - open.begin());
      if (!open.empty())
        trade_.choosePartner(open[static_cast<std::size_t>(
            step(i, key == Key::Left ? -1 : 1, static_cast<int>(open.size())))]);
    }
    return {std::nullopt, true};
  }
  case Mode::Others: {
    // (left and right through the tabs; B back)
    if (key == Key::Back) {
      closeOthers();
    } else if ((key == Key::Left || key == Key::Right) && !others_.empty()) {
      const auto at = std::find_if(others_.begin(), others_.end(),
                                   [&](const PlayerView &v) { return v.player == othersChosen_; });
      const int i = at == others_.end() ? 0 : static_cast<int>(at - others_.begin());
      othersChosen_ = others_[static_cast<std::size_t>(step(i, key == Key::Left ? -1 : 1,
                                                            static_cast<int>(others_.size())))]
                          .player;
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

ChoiceScreens::Press BottomScreen::actionPress() const {
  ChoiceScreens::Press press = actionPress_;
  press.now = now_;
  return press;
}

void BottomScreen::layOutButtons() {
  situation_.placing = mode_ == Mode::Placing;
  situation_.placeChosen = chosen_.has_value();
  // (buttons that came or went since fade in or out: Roll giving way to
  // End Turn, Confirm while placing)
  fades_.update(BoardButtons::layout(width_, height_, situation_), now_);
  std::vector<ScratchPadd::Buttons::Button> buttons;
  for (const auto &[place, opacity] : fades_.shown(now_)) {
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
    buttons.push_back(
        {.id = static_cast<int>(place.button),
         .area = {place.x, place.y, place.size, place.size},
         .icon = &sprite,
         .iconSize = inner,
         .usable = place.usable,
         // (the toggles, while what they show is open)
         .on = (place.button == BoardButtons::Button::ShowResources && info_ == Info::Resources) ||
               (place.button == BoardButtons::Button::MoveList && moveList_),
         .face = roll ? std::optional(kRollButton) : std::nullopt,
         .edge = roll ? std::optional(kRollEdge) : std::nullopt,
         .shown = opacity});
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

std::vector<Corner> BottomScreen::placingCornersWithFuel() const {
  std::vector<Corner> out;
  if (mode_ != Mode::Placing || placingShip_ < 0)
    return out;
  for (std::size_t i = 0; i < placing_.size() && i < placingFuel_.size(); ++i)
    if (placingFuel_[i]) {
      const MoveSpot spot = spotOf(state_, placing_[i]);
      if (!spot.corners.empty())
        out.push_back(spot.corners.back());
    }
  return out;
}

std::optional<int> BottomScreen::selectedShip() const {
  if (mode_ != Mode::Placing || placingShip_ < 0)
    return std::nullopt;
  return placingShip_;
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
    ChoiceScreens::drawBuild(canvas, rows_, buildChosen_, actionPress());
    return;
  case Mode::Resources:
    if (resources_)
      ChoiceScreens::drawResources(canvas, *resources_, picked_,
                                   state_.player[static_cast<std::size_t>(state_.turn)].resources,
                                   resourcesCanGoBack_, cards_, resourceFocus_, actionPress());
    return;
  case Mode::Players:
    if (players_)
      ChoiceScreens::drawPlayers(canvas, *players_, playerChosen_, cardChosen_, Art::kResourceBack,
                                 cards_, actionPress());
    return;
  case Mode::Cards:
    ChoiceScreens::drawCards(canvas, held_, heldChosen_, bigCards_, actionPress());
    return;
  case Mode::Keep:
    if (keep_)
      ChoiceScreens::drawKeep(canvas, *keep_, keepChosen_, bigCards_, actionPress());
    return;
  case Mode::Others:
    ChoiceScreens::drawOthers(canvas, others_, othersChosen_, trade_.names(), actionPress());
    return;
  case Mode::Trade:
    ChoiceScreens::drawTrade(canvas, trade_, cards_, actionPress());
    return;
  case Mode::Board:
  case Mode::Placing:
    break;
  }
}

void BottomScreen::drawOverBoard(Canvas &canvas) {
  // The buttons over the board, and a tapped one's message
  layOutButtons();
  buttons_.setTime(now_);
  buttons_.draw(canvas, {.face = kButton,
                         .edge = kButtonEdge,
                         .on = kHint,
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
      hint = chosen_ ? "End Turn: place your colony" : "Home colony: touch a corner";
    else if (startPlacing_)
      hint = chosen_ ? "End Turn: place your ship" : "Your ship: touch a corner";
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
  for (const auto &[place, opacity] : fades_.shown(now_)) // (fading ones too)
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

void BottomScreen::openPicker(Pickers::ResourceChoice choice, bool canGoBack) {
  resources_ = std::move(choice);
  picked_.clear();
  resourcesCanGoBack_ = canGoBack;
  mode_ = Mode::Resources;
  // (the focus on the first card it offers; for a single card, that card
  // picked: A takes it at once, the user's)
  resourceFocus_ = kResources - 1;
  focusNextResource(1);
}

void BottomScreen::focusNextResource(int direction) {
  if (!resources_)
    return;
  // (on to the next card offered, round and round; a single card's choice
  // follows the focus)
  for (int tries = 0; tries < kResources; ++tries) {
    resourceFocus_ = (resourceFocus_ + direction + kResources) % kResources;
    if (resources_->offers(static_cast<Resource>(resourceFocus_)))
      break;
  }
  if (resources_->howMany == 1)
    picked_ = {static_cast<Resource>(resourceFocus_)};
}

std::optional<Resource> BottomScreen::resourceInFocus() const {
  if (mode_ != Mode::Resources || !resources_)
    return std::nullopt;
  return static_cast<Resource>(resourceFocus_);
}

} // namespace CosmicHex::Handheld
