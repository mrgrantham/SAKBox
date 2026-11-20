#include <CosmicHex/Handheld/TopShows.hpp>

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

float FirstRollShow::duration(const FirstRoll &roll) {
  return kSecondsRound * static_cast<float>(std::min(roll.rounds.size(), kRoundsShown)) +
         kSecondsRead;
}

std::optional<float> FirstRollShow::tumble(std::size_t round, float seconds) {
  const float startsAt = kSecondsRound * static_cast<float>(round);
  if (seconds < startsAt)
    return std::nullopt;
  return std::min(1.0f, (seconds - startsAt) / kSecondsTumble);
}

bool FirstRollShow::settled(const FirstRoll &roll, float seconds) {
  const std::size_t rounds = std::min(roll.rounds.size(), kRoundsShown);
  return rounds > 0 && seconds >= kSecondsRound * static_cast<float>(rounds - 1) + kSecondsTumble;
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
  if (!held_) {
    held_ = true;
    step.hold = true;
  }
  // The star waiting for its choice: the clock stops at the die settled
  if (!current()->resource && *s > kDieSeconds) {
    startedAt_ = now - kDieSeconds;
    return step;
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

} // namespace CosmicHex::Handheld
