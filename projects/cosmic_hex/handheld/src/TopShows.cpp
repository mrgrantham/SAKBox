#include <CosmicHex/Handheld/TopShows.hpp>

#include <CosmicHex/Handheld/CardsHeld.hpp>

#include <algorithm>

namespace CosmicHex::Handheld {

namespace Motion = ScratchPadd::Motion;

// --------------------------------------------------------------- Discovery

void DiscoveryShow::arrived(const State &before, const State &after, double startAt) {
  shownBig_.clear();
  for (Discovery::Found &found : Discovery::between(before, after))
    queue_.push_back(std::move(found));
  if (!queue_.empty() && !startsAt_)
    startsAt_ = startAt;
}

DiscoveryShow::Step DiscoveryShow::update(double now,
                                          const std::function<void(Art::Face)> &closeUp) {
  Step step;
  if (queue_.empty() || !startsAt_ || now < *startsAt_)
    return step;
  if (!closeUpDrawn_) {
    if (closeUp)
      closeUp(queue_.front().face);
    closeUpDrawn_ = true;
    startsAt_ = now; // (the show starts after the drawing)
  }
  if (!held_) {
    held_ = true;
    step.hold = true;
  }
  const Discovery::Moment m = Discovery::at(static_cast<float>(now - *startsAt_));
  step.redrawMap = m.fade > 0;
  if (!m.done)
    return step;
  shownBig_.push_back(queue_.front().hex);
  queue_.erase(queue_.begin());
  closeUpDrawn_ = false;
  if (!queue_.empty()) {
    startsAt_ = now;
    return step;
  }
  startsAt_.reset();
  held_ = false;
  step.letGo = true;
  step.redrawMap = true;
  return step;
}

const Discovery::Found *DiscoveryShow::current() const {
  return queue_.empty() ? nullptr : &queue_.front();
}

std::optional<Discovery::Moment> DiscoveryShow::moment(double now) const {
  if (queue_.empty() || !startsAt_ || now < *startsAt_ || !closeUpDrawn_)
    return std::nullopt;
  return Discovery::at(static_cast<float>(now - *startsAt_));
}

bool DiscoveryShow::stillToShow(Hex hex, double now) const {
  for (std::size_t i = 0; i < queue_.size(); ++i) {
    if (queue_[i].hex != hex)
      continue;
    const auto m = i == 0 ? moment(now) : std::nullopt;
    return !m || !m->back;
  }
  return false;
}

bool DiscoveryShow::flying(Hex hex, double now) const {
  const auto m = moment(now);
  return m && queue_.front().hex == hex && !m->back;
}

bool DiscoveryShow::shownBig(Hex hex) const {
  return std::ranges::find(shownBig_, hex) != shownBig_.end() ||
         (!queue_.empty() && queue_.front().hex == hex);
}

std::vector<Hex> DiscoveryShow::waitingAfterCurrent() const {
  std::vector<Hex> hexes;
  for (std::size_t i = 1; i < queue_.size(); ++i)
    hexes.push_back(queue_[i].hex);
  return hexes;
}

std::optional<DiscoveryShow::Revealing> DiscoveryShow::revealing(double now) const {
  if (queue_.empty())
    return std::nullopt;
  const auto m = moment(now);
  return Revealing{queue_.front().hex, m ? m->fade : 0.0f};
}

// ------------------------------------------------------------------ Dealt

void DealtShow::deal(const std::vector<Art::Card> &cards, double now) {
  if (on() && cards == cards_)
    return;
  cards_ = cards;
  closeUpDrawn_ = false;
  shown_.start(now);
}

void DealtShow::closeUpDrawn(double now) {
  closeUpDrawn_ = true;
  shown_.start(now);
}

float DealtShow::thrown(std::size_t i, double now) const {
  const auto since = static_cast<float>(shown_.seconds(now));
  return Motion::along(Motion::Curve::Settle,
                       (since - kThrowStagger * static_cast<float>(i)) / kSecondsThrow);
}

float DealtShow::turned(std::size_t i, double now) const {
  const auto since = static_cast<float>(shown_.seconds(now));
  return std::clamp((since - kSecondsThrow - kTurnAfter - kTurnStagger * static_cast<float>(i)) /
                        kSecondsTurn,
                    0.0f, 1.0f);
}

// ------------------------------------------------------------------ Panel

void PanelShow::open(int which, double now) {
  which_ = which;
  opened_.start(now);
  closing_.stop();
}

void PanelShow::close(double now) {
  if (opened_.on() && !closing_.on())
    closing_.start(now);
}

bool PanelShow::showing(double now) const {
  if (!opened_.on())
    return false;
  return !closing_.on() || closing_.seconds(now) < seconds_;
}

float PanelShow::down(double now) const {
  if (!opened_.on())
    return 0;
  const Motion::Showing &from = closing_.on() ? closing_ : opened_;
  const float moved =
      Motion::along(Motion::Curve::EaseInOut, static_cast<float>(from.seconds(now)) / seconds_);
  return closing_.on() ? 1 - moved : moved;
}

// ------------------------------------------------------------------ Cards

void CardsShow::open(double now) {
  opened_.start(now);
  closing_.stop();
}

void CardsShow::close(double now) {
  if (opened_.on() && !closing_.on())
    closing_.start(now);
}

bool CardsShow::showing(double now) const {
  if (!opened_.on())
    return false;
  return !closing_.on() ||
         closing_.seconds(now) < kSecondsCard + kStagger * static_cast<float>(kMostCards);
}

float CardsShow::arrived(int order, double now) const {
  const Motion::Showing &from = closing_.on() ? closing_ : opened_;
  const float since = static_cast<float>(from.seconds(now)) - kStagger * static_cast<float>(order);
  const float moved = Motion::along(Motion::Curve::EaseInOut, since / kSecondsCard);
  return closing_.on() ? 1 - moved : moved;
}

// -------------------------------------------------------------- First roll

void FirstRollShow::start(const FirstRoll &roll, std::uint64_t seed, double stepSeconds,
                          double now) {
  roll_ = roll;
  seed_ = seed;
  step_ = stepSeconds;
  on_ = !roll.rounds.empty();
  shown_.assign(roll.rounds.size(), std::vector<int>(kMaxPlayers, 0));
  finishedAt_.reset();
  if (on_)
    beginRound(0, now);
}

void FirstRollShow::stop() {
  on_ = false;
  dice_.reset();
}

void FirstRollShow::beginRound(std::size_t round, double now) {
  round_ = round;
  next_ = 0;
  throwers_.clear();
  const std::vector<int> &faces = roll_.rounds[round];
  for (std::size_t p = 0; p < faces.size(); ++p)
    if (faces[p] > 0)
      throwers_.push_back(static_cast<int>(p));
  dice_ = std::make_unique<Dice::DiceInTurn>(step_);
  simulated_ = now;
  landedAt_ = now - kSecondsBetween; // (the first thrown at once)
}

void FirstRollShow::throwNext(double now) {
  if (!dice_ || next_ >= throwers_.size())
    return;
  const int player = throwers_[next_];
  dice_->throwDie(static_cast<int>(next_), seed_ * 31 + round_ * 7 + next_,
                  roll_.rounds[round_][static_cast<std::size_t>(player)]);
  ++next_;
  landedAt_.reset();
  simulated_ = now;
}

void FirstRollShow::update(double now) {
  if (!on_ || !dice_)
    return;
  // The physics stepped to now, a fixed step at a time (a few at most a
  // frame, so a slow frame doesn't stall)
  for (int steps = 0; simulated_ + step_ <= now && steps < 4; ++steps) {
    dice_->step();
    simulated_ += step_;
  }
  if (simulated_ + step_ <= now)
    simulated_ = now;
  if (dice_->moving())
    return;
  // A die has landed: its number by its player's name
  for (std::size_t i = 0; i < next_; ++i)
    if (dice_->settled(static_cast<int>(i)))
      shown_[round_][static_cast<std::size_t>(throwers_[i])] =
          roll_.rounds[round_][static_cast<std::size_t>(throwers_[i])];
  if (!landedAt_)
    landedAt_ = now;
  if (next_ < throwers_.size()) {
    if (now - *landedAt_ >= kSecondsBetween)
      throwNext(now);
    return;
  }
  if (round_ + 1 < roll_.rounds.size()) {
    if (now - *landedAt_ >= kSecondsBetweenRounds)
      beginRound(round_ + 1, now);
    return;
  }
  if (!finishedAt_)
    finishedAt_ = now;
}

void FirstRollShow::hurry(double now) {
  if (!on_)
    return;
  if (finishedAt_) {
    finishedAt_ = now - kSecondsRead; // (read already: on to the game)
    return;
  }
  if (next_ < throwers_.size()) {
    throwNext(now); // (the moving die set down where it lands)
    return;
  }
  // (the round's last die moving, or the round done: the next round now)
  if (dice_ && dice_->moving())
    for (std::size_t i = 0; i < next_; ++i)
      dice_->settleNow(static_cast<int>(i));
  landedAt_ = now - kSecondsBetweenRounds;
  update(now);
}

bool FirstRollShow::done(double now) const {
  return on_ && finishedAt_ && now - *finishedAt_ >= kSecondsRead;
}

int FirstRollShow::shown(std::size_t round, int player) const {
  if (round >= shown_.size() || player < 0 || player >= kMaxPlayers)
    return 0;
  return shown_[round][static_cast<std::size_t>(player)];
}

// --------------------------------------------------------------- Home pay

void HomePayShow::saw(const std::vector<TableGame::Played> &moves, double now) {
  for (const TableGame::Played &move : moves) {
    if (move.action.kind == Action::Kind::RollHomeDie) {
      Pay pay{.player = move.player, .face = move.action.a};
      if (move.action.a >= 0 && move.action.a < kResources)
        pay.resource = static_cast<Resource>(move.action.a);
      queue_.push_back(pay);
    } else if (move.action.kind == Action::Kind::ChooseHome) {
      // (the star's choice, made: the latest of the player's still open)
      for (auto it = queue_.rbegin(); it != queue_.rend(); ++it)
        if (it->player == move.player && !it->resource) {
          it->resource = static_cast<Resource>(move.action.a);
          break;
        }
    }
  }
  if (!queue_.empty() && !startedAt_)
    startedAt_ = now;
}

std::optional<float> HomePayShow::seconds(double now) const {
  if (queue_.empty() || !startedAt_ || now < *startedAt_)
    return std::nullopt;
  return static_cast<float>(now - *startedAt_);
}

HomePayShow::Moment HomePayShow::at(float s) const {
  const auto part = [s](float from, float length) {
    return std::clamp((s - from) / length, 0.0f, 1.0f);
  };
  const bool choosing = current() && !current()->resource;
  // (waiting for the star's choice: the die stays settled until it's made)
  const float cardFrom = kDieSeconds;
  return {.up = part(0, kSecondsUp),
          .tumble = part(kSecondsUp, kSecondsTumble),
          .settle = part(kSecondsUp + kSecondsTumble, kSecondsSettle),
          .card = choosing ? 0 : part(cardFrom, kSecondsCard),
          .away = choosing ? 0 : part(cardFrom + kSecondsCard + kSecondsHold, kSecondsAway),
          .waitingForChoice = choosing && s >= kDieSeconds};
}

HomePayShow::Step HomePayShow::update(double now) {
  Step step;
  const auto s = seconds(now);
  if (!s)
    return step;
  // The star waiting for its choice: the clock stops at the die settled,
  // and the game goes on meanwhile (the player has to choose: a game held
  // couldn't)
  if (!current()->resource && *s > kDieSeconds) {
    startedAt_ = now - kDieSeconds;
    if (held_) {
      held_ = false;
      step.letGo = true;
    }
    return step;
  }
  if (!held_) {
    held_ = true;
    step.hold = true;
  }
  if (*s < kSeconds)
    return step;
  queue_.erase(queue_.begin());
  if (!queue_.empty()) {
    startedAt_ = now;
    return step;
  }
  startedAt_.reset();
  held_ = false;
  step.letGo = true;
  return step;
}

// --------------------------------------------------------------- Roll pay

void RollPayShow::saw(const std::vector<TableGame::Played> &moves, const State &before) {
  // (who's to be stolen from: as the game had it, then as the blockade
  // names them; the thief is the player whose turn it is)
  if (!moves.empty()) {
    victims_ = before.victims;
    offer_ = before.offer;
  }
  for (const TableGame::Played &move : moves) {
    switch (move.action.kind) {
    case Action::Kind::OfferTrade:
      offer_ = {.give = move.action.a, .get = move.action.b, .to = move.action.c};
      counterer_ = -1;
      break;
    case Action::Kind::CounterTrade:
      offer_.counterGive = move.action.a;
      offer_.counterGet = move.action.b;
      counterer_ = move.player;
      break;
    case Action::Kind::DeclineTrade:
      offer_.counterGive = offer_.counterGet = 0;
      break;
    case Action::Kind::AcceptTrade:
      // (the offerer taking a counter, or the one asked taking the offer)
      if (offer_.counterGive != 0 && counterer_ >= 0) {
        cross(before.turn, counterer_, offer_.counterGet);
        cross(counterer_, before.turn, offer_.counterGive);
      } else {
        cross(before.turn, move.player, offer_.give);
        cross(move.player, before.turn, offer_.get);
      }
      offer_ = {};
      break;
    case Action::Kind::Trade: { // (with the bank)
      Resources give{};
      give[static_cast<std::size_t>(move.action.a)] = tradeRateOf(before, before.turn);
      cross(before.turn, kBank, bundleOf(give));
      Resources get{};
      get[static_cast<std::size_t>(move.action.b)] = 1;
      cross(kBank, before.turn, bundleOf(get));
      break;
    }
    default:
      break;
    }
    if (move.action.kind == Action::Kind::MoveBlockade && move.action.b >= 0)
      victims_ = {move.action.b};
    if (move.action.kind == Action::Kind::Steal && !victims_.empty() && move.action.a >= 0 &&
        move.action.a < kResources) {
      cards_.push_back(
          {-1, before.turn, static_cast<Resource>(move.action.a), victims_.front(), true});
      victims_.erase(victims_.begin());
    }
    if (move.action.kind != Action::Kind::Roll)
      continue;
    const int roll = move.action.a + move.action.b;
    if (roll == 7)
      continue; // (the blockade's, not a pay)
    for (const Payout &pay : payouts(before, roll))
      for (int n = 0; n < pay.count; ++n)
        cards_.push_back({pay.planet, pay.player, pay.resource});
  }
}

void RollPayShow::cross(int from, int to, int bundle) {
  const Resources cards = resourcesOf(bundle);
  for (int r = 0; r < kResources; ++r)
    for (int n = 0; n < cards[static_cast<std::size_t>(r)]; ++n)
      cards_.push_back({-1, to, static_cast<Resource>(r), from});
}

float RollPayShow::seconds() const {
  if (cards_.empty())
    return 0;
  return kStagger * static_cast<float>(cards_.size() - 1) + kSecondsPop + kSecondsFly;
}

void RollPayShow::start(double now) {
  if (waiting()) {
    startAt_ = now;
    started_ = true;
  }
}

std::optional<RollPayShow::Moment> RollPayShow::at(std::size_t i, double now) const {
  if (i >= cards_.size() || !started_)
    return std::nullopt;
  const auto t = static_cast<float>(now - startAt_) - kStagger * static_cast<float>(i);
  if (t < 0 || t >= kSecondsPop + kSecondsFly)
    return std::nullopt;
  return Moment{Motion::along(Motion::Curve::Settle, std::min(1.0f, t / kSecondsPop)),
                Motion::along(Motion::Curve::Gentle,
                              std::clamp((t - kSecondsPop) / kSecondsFly, 0.0f, 1.0f))};
}

bool RollPayShow::moving(double now) const {
  return started_ && !cards_.empty() && now >= startAt_ && now < startAt_ + seconds();
}

RollPayShow::Step RollPayShow::update(double now) {
  Step step;
  if (!started_ || cards_.empty() || now < startAt_)
    return step;
  if (!held_) {
    held_ = true;
    step.hold = true;
  }
  if (now < startAt_ + seconds())
    return step;
  cards_.clear();
  held_ = started_ = false;
  step.letGo = true;
  return step;
}

// --------------------------------------------------------------- Card play

void CardPlayShow::saw(const std::vector<TableGame::Played> &moves, double now) {
  for (const TableGame::Played &move : moves) {
    std::optional<Art::Card> card;
    switch (move.action.kind) {
    case Action::Kind::PlayEnforcer:
      card = Art::Card::Enforcer;
      break;
    case Action::Kind::PlayCartel:
      card = Art::Card::InterstellarCartel;
      break;
    case Action::Kind::PlayShipyard:
      card = Art::Card::ShipyardSurplus;
      break;
    case Action::Kind::PlaySupplyDrop:
      card = Art::Card::SupplyDrop;
      break;
    case Action::Kind::RevealContract:
      card = cardOf(move.action.a);
      break;
    default:
      break;
    }
    if (!card)
      continue;
    queue_.push_back({move.player, *card});
    if (!startedAt_)
      startedAt_ = now;
  }
}

std::optional<CardPlayShow::Moment> CardPlayShow::at(double now) const {
  if (queue_.empty() || !startedAt_ || now < *startedAt_)
    return std::nullopt;
  const auto s = static_cast<float>(now - *startedAt_);
  if (s >= kSeconds)
    return std::nullopt;
  const auto part = [s](float from, float length) {
    return std::clamp((s - from) / length, 0.0f, 1.0f);
  };
  return Moment{
      .up = Motion::along(Motion::Curve::Settle, part(0, kSecondsUp)),
      .turned = Motion::along(Motion::Curve::EaseInOut, part(kSecondsUp * 0.3f, kSecondsUp)),
      .away =
          Motion::along(Motion::Curve::Anticipate, part(kSecondsUp + kSecondsHold, kSecondsAway))};
}

CardPlayShow::Step CardPlayShow::update(double now) {
  Step step;
  if (queue_.empty() || !startedAt_ || now < *startedAt_)
    return step;
  if (!held_) {
    held_ = true;
    step.hold = true;
  }
  if (now - *startedAt_ < kSeconds)
    return step;
  queue_.erase(queue_.begin());
  if (!queue_.empty()) {
    startedAt_ = now;
    return step;
  }
  startedAt_.reset();
  held_ = false;
  step.letGo = true;
  return step;
}

} // namespace CosmicHex::Handheld
