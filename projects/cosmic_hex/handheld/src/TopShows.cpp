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

float DealtShow::words(std::size_t i, double now) const {
  const auto since = static_cast<float>(shown_.seconds(now));
  return std::clamp(
      (since - kSecondsThrow - kTurnAfter - kTurnStagger * static_cast<float>(i) - kSecondsTurn) /
          kSecondsWordsIn,
      0.0f, 1.0f);
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

void PanelShow::choose(int which, double now, int direction) {
  if (which == which_)
    return;
  // (in place: the carousel turns; still coming down, it's a cut)
  last_ = inPlace(now) ? which_ : -1;
  which_ = which;
  direction_ = direction < 0 ? -1 : 1;
  switched_.start(now);
}

std::vector<PanelShow::Drawn> PanelShow::drawn(double now) const {
  if (last_ < 0 || !switched_.on() || closing_.on())
    return {{which_, down(now)}};
  const float t = static_cast<float>(switched_.seconds(now)) / kSecondsSwitch;
  if (t >= 1)
    return {{which_, down(now)}};
  // (both together, side by side: the last out one way, the next in from
  // the other, eased at both ends)
  const float moved = Motion::along(Motion::Curve::Gentle, t);
  const auto way = static_cast<float>(direction_);
  return {{last_, 1, -way * moved}, {which_, 1, way * (1 - moved)}};
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

bool PanelShow::inPlace(double now) const {
  return opened_.on() && !closing_.on() && opened_.seconds(now) >= seconds_;
}

float PanelShow::down(double now) const {
  if (!opened_.on())
    return 0;
  const Motion::Showing &from = closing_.on() ? closing_ : opened_;
  // (Gentle: its movement spread evenly over the middle. On a DS the board
  // under it holds the frame rate near 20 a second while it moves, so a
  // curve that moves most of the way early, as Settle does, shows as a few
  // big jumps)
  const float t = std::min(1.0f, static_cast<float>(from.seconds(now)) / seconds_);
  const float moved = Motion::along(Motion::Curve::Gentle, t);
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
  shownAt_.assign(roll.rounds.size(), std::vector<double>(kMaxPlayers, 0));
  finishedAt_.reset();
  startedAt_ = now;
  if (on_) {
    beginRound(0, now);
    landedAt_ = now - kSecondsBetween + kSecondsIn; // (the first once it's faded in)
  }
}

float FirstRollShow::appeared(double now) const {
  return std::clamp(static_cast<float>(now - startedAt_) / kSecondsIn, 0.0f, 1.0f);
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
  // (a lane each, the user's: each die down its own lane from the top,
  // walls between, so none hits another; smaller dice than the roll's)
  dice_ = std::make_unique<Dice::DiceInTurn>(
      step_, Dice::DiceLanes{.count = static_cast<int>(faces.size()), .dieSize = kDieSize});
  simulated_ = now;
  landedAt_ = now - kSecondsBetween; // (the first thrown at once)
}

void FirstRollShow::throwNext(double now) {
  if (!dice_ || next_ >= throwers_.size())
    return;
  const int player = throwers_[next_];
  dice_->throwDie(player, seed_ * 31 + round_ * 7 + next_,
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
  for (std::size_t i = 0; i < next_; ++i) {
    const auto player = static_cast<std::size_t>(throwers_[i]);
    if (dice_->settled(throwers_[i]) && shown_[round_][player] == 0) {
      shown_[round_][player] = roll_.rounds[round_][player];
      shownAt_[round_][player] = now;
    }
  }
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
      dice_->settleNow(throwers_[i]);
  landedAt_ = now - kSecondsBetweenRounds;
  update(now);
}

float FirstRollShow::sorted(double now, int order) const {
  if (!finishedAt_)
    return 0;
  constexpr float kPause = 0.4f; // (the last number read first)
  const float t =
      (static_cast<float>(now - *finishedAt_) - kPause - kSortStagger * static_cast<float>(order)) /
      kSecondsSort;
  return std::clamp(t, 0.0f, 1.0f);
}

float FirstRollShow::sortedWords(double now, int order) const {
  if (!finishedAt_)
    return 0;
  constexpr float kPause = 0.4f; // (as sorted's)
  const float since = static_cast<float>(now - *finishedAt_) - kPause -
                      kSortStagger * static_cast<float>(order) - kSecondsSort;
  return std::clamp(since / kSecondsWordsIn, 0.0f, 1.0f);
}

bool FirstRollShow::done(double now) const {
  return on_ && finishedAt_ && now - *finishedAt_ >= kSecondsRead;
}

double FirstRollShow::shownAt(std::size_t round, int player) const {
  if (round >= shownAt_.size() || player < 0 || player >= kMaxPlayers)
    return 0;
  return shownAt_[round][static_cast<std::size_t>(player)];
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
          .waitingForChoice = choosing && s >= kDieSeconds,
          .words = choosing ? 0 : part(cardFrom + kSecondsCard, kSecondsWordsIn)};
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
  // (the starting hand's cards dealt before these moves: the next goes to
  // the next player round the table, as the rules deal them)
  int startCardsDealt = before.settings.startCards * before.players - before.startCardsLeft;
  for (const TableGame::Played &move : moves) {
    switch (move.action.kind) {
    // The starting hand: from the bank (the ROUND hexagon, the user's),
    // face down to everyone but its player, as at a table
    case Action::Kind::DealStartCard: {
      const int to = (before.first + startCardsDealt++) % std::max(1, before.players);
      cards_.push_back({.planet = -1,
                        .player = to,
                        .resource = static_cast<Resource>(move.action.a),
                        .from = kBank,
                        .secret = true});
      break;
    }
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
    // Resources spent: to the bank (behind the ROUND hexagon, the user's)
    case Action::Kind::BuildShip:
      cross(before.turn, kBank, bundleOf(before.settings.shipCost));
      break;
    case Action::Kind::Settle:
      cross(before.turn, kBank, bundleOf(before.settings.stationCost));
      break;
    case Action::Kind::BuildColony:
      cross(before.turn, kBank, bundleOf(before.settings.colonyCost));
      break;
    case Action::Kind::BuyContract:
      cross(before.turn, kBank, bundleOf(before.settings.contractCost));
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
          Motion::along(Motion::Curve::Anticipate, part(kSecondsUp + kSecondsHold, kSecondsAway)),
      .words = part(kSecondsUp * 1.3f, kSecondsWordsIn)};
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

// ---------------------------------------------------------------- Picking

void PickShow::show(std::optional<Resource> resource, double now) {
  if (resource == shown_)
    return;
  if (!resource) { // (the choice made, or gone)
    shown_.reset();
    leaving_.reset();
    return;
  }
  // (the first: in from below the middle; then from the side moved to)
  direction_ = shown_ && static_cast<int>(*resource) < static_cast<int>(*shown_) ? -1 : 1;
  leaving_ = shown_;
  shown_ = resource;
  since_ = now;
}

std::optional<PickShow::Moment> PickShow::at(double now) const {
  if (!shown_)
    return std::nullopt;
  const float t = std::clamp(static_cast<float>(now - since_) / kSecondsSlide, 0.0f, 1.0f);
  const auto words =
      std::clamp(static_cast<float>(now - since_ - kSecondsSlide) / kSecondsWordsIn, 0.0f, 1.0f);
  return Moment{*shown_, t < 1 ? leaving_ : std::nullopt, Motion::along(Motion::Curve::EaseOut, t),
                words, direction_};
}

// (until its name has faded in: the screen's drawn again till then)
bool PickShow::moving(double now) const {
  return shown_ && now - since_ < kSecondsSlide + kSecondsWordsIn;
}

// ------------------------------------------------------------- Cards gained

void HandShow::gained(const Resources &added) {
  for (std::size_t r = 0; r < queued_.size(); ++r)
    queued_[r] += added[r];
}

bool HandShow::waiting() const {
  return !startedAt_ && std::any_of(queued_.begin(), queued_.end(), [](int n) { return n > 0; });
}

void HandShow::start(double now) {
  if (!waiting())
    return;
  added_ = queued_;
  queued_ = {};
  startedAt_ = now;
}

float HandShow::seconds() const {
  int cards = 0;
  for (const int n : added_)
    cards += n;
  return 2 * kSecondsPiles + kStagger * static_cast<float>(std::max(0, cards - 1)) + kSecondsCard +
         kSecondsHold;
}

bool HandShow::showing(double now) const {
  return startedAt_ && now >= *startedAt_ && now - *startedAt_ < seconds();
}

float HandShow::piles(double now) const {
  if (!startedAt_ || !showing(now))
    return 0;
  const auto t = static_cast<float>(now - *startedAt_);
  if (t < kSecondsPiles)
    return Motion::along(Motion::Curve::EaseOut, t / kSecondsPiles);
  const float leaving = (t - (seconds() - kSecondsPiles)) / kSecondsPiles;
  return leaving > 0 ? 1 - Motion::along(Motion::Curve::EaseIn, leaving) : 1;
}

float HandShow::card(Resource r, int index, double now) const {
  if (!startedAt_ || index < 0)
    return 1;
  // (the gained cards in order: all of one resource, then the next)
  int order = 0;
  for (int q = 0; q < static_cast<int>(r); ++q)
    order += added_[static_cast<std::size_t>(q)];
  order += index;
  const auto t =
      static_cast<float>(now - *startedAt_) - kSecondsPiles - kStagger * static_cast<float>(order);
  return Motion::along(Motion::Curve::Settle, std::clamp(t / kSecondsCard, 0.0f, 1.0f));
}

// ------------------------------------------------------------ Corner pointer

float CornerPointer::shownAt(double now) const {
  const float t = std::clamp(static_cast<float>(now - fadedAt_) / kSecondsFade, 0.0f, 1.0f);
  const float target = on_ ? 1.0f : 0.0f;
  return fadeFrom_ + (target - fadeFrom_) * Motion::along(Motion::Curve::Gentle, t);
}

void CornerPointer::aim(std::optional<Corner> corner, double now) {
  // (fading in or away: from wherever the last fade had got to)
  if (corner.has_value() != on_) {
    fadeFrom_ = shownAt(now);
    fadedAt_ = now;
    on_ = corner.has_value();
  }
  if (!corner)
    return;
  if (!to_ || shownAt(now) == 0) { // (appearing: at its corner, no glide)
    from_ = to_ = corner;
    glidedAt_ = now - kSecondsGlide;
    return;
  }
  if (*corner != *to_) {
    // (from where it is now: the glide's end, close enough mid-glide)
    from_ = to_;
    to_ = corner;
    glidedAt_ = now;
  }
}

void CornerPointer::tap(double now) {
  // (once it's arrived: a tap mid-glide waits for the glide's end)
  tapAt_ = std::max(now, glidedAt_ + kSecondsGlide);
}

std::optional<CornerPointer::Look> CornerPointer::at(double now) const {
  const float shown = shownAt(now);
  if (!from_ || !to_ || shown <= 0) // (both set together, by aim)
    return std::nullopt;
  const float t = std::clamp(static_cast<float>(now - glidedAt_) / kSecondsGlide, 0.0f, 1.0f);
  Look look{*from_, *to_, Motion::along(Motion::Curve::EaseOut, t), shown};
  if (tapAt_ && now >= *tapAt_) {
    // A small lift, a quick drop to the touch, then back up settling
    const auto since = static_cast<float>(now - *tapAt_);
    const float dropFrom = kSecondsLift, touches = kSecondsLift + kSecondsDrop,
                settled = touches + kSecondsSettle;
    if (since < dropFrom)
      look.dip = -kLift * Motion::along(Motion::Curve::EaseOut, since / kSecondsLift);
    else if (since < touches)
      look.dip = -kLift + ((1 + kLift) *
                           Motion::along(Motion::Curve::EaseIn, (since - dropFrom) / kSecondsDrop));
    else if (since < settled)
      look.dip = 1 - Motion::along(Motion::Curve::Settle, (since - touches) / kSecondsSettle);
    // (the dust from the moment it touches, as long as a building's)
    const auto touched = static_cast<float>(now - *tapAt_) - kSecondsLift - kSecondsDrop;
    if (touched >= 0 && touched < kSecondsDust)
      look.dust = touched / kSecondsDust;
  }
  return look;
}

bool CornerPointer::moving(double now) const {
  const bool tapping = tapAt_ && now - *tapAt_ < kSecondsLift + kSecondsDrop +
                                                     std::max(kSecondsSettle, kSecondsDust);
  return now - fadedAt_ < kSecondsFade || now - glidedAt_ < kSecondsGlide || tapping;
}

} // namespace CosmicHex::Handheld
