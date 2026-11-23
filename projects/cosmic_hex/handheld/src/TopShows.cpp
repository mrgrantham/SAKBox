#include <CosmicHex/Handheld/TopShows.hpp>

#include <CosmicHex/Handheld/CardsHeld.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>

#include <algorithm>
#include <cmath>

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
  const Discovery::Moment m = Discovery::at(elapsed(now));
  step.redrawMap = m.fade > 0;
  if (m.done) {
    shownBig_.push_back(queue_.front().hex);
    queue_.erase(queue_.begin());
    closeUpDrawn_ = released_ = false;
    waited_ = 0;
    if (queue_.empty()) {
      startsAt_.reset();
      step.redrawMap = true;
    } else {
      startsAt_ = now;
    }
  }
  // (held while a tile shows, but not while its choice is made in the game)
  const bool busy = !queue_.empty() && !waitingForChoice(now);
  if (busy != held_) {
    held_ = busy;
    (busy ? step.hold : step.letGo) = true;
  }
  return step;
}

void DiscoveryShow::waitFor(double until) {
  if (!queue_.empty() && !closeUpDrawn_ && (!startsAt_ || *startsAt_ < until))
    startsAt_ = until;
}

bool DiscoveryShow::started(double now) const {
  return !queue_.empty() && startsAt_ && now >= *startsAt_ && closeUpDrawn_;
}

float DiscoveryShow::elapsed(double now) const {
  const double since = now - startsAt_.value_or(now);
  if (!queue_.front().choiceAfter)
    return static_cast<float>(since);
  if (!released_)
    return static_cast<float>(std::min(since, static_cast<double>(Discovery::kSecondsToDown)));
  return static_cast<float>(since - waited_);
}

bool DiscoveryShow::choiceStillToShow(double now) const {
  for (std::size_t i = 0; i < queue_.size(); ++i)
    if (queue_[i].choiceAfter)
      return i > 0 || !started(now) || elapsed(now) < Discovery::kSecondsToWords;
  return false;
}

bool DiscoveryShow::waitingForChoice(double now) const {
  return started(now) && queue_.front().choiceAfter && !released_ &&
         elapsed(now) >= Discovery::kSecondsToWords;
}

void DiscoveryShow::release(double now) {
  if (!started(now) || !queue_.front().choiceAfter || released_)
    return;
  released_ = true;
  waited_ = std::max(0.0, now - startsAt_.value_or(now) - Discovery::kSecondsToDown);
}

const Discovery::Found *DiscoveryShow::current() const {
  return queue_.empty() ? nullptr : &queue_.front();
}

std::optional<Discovery::Moment> DiscoveryShow::moment(double now) const {
  if (!started(now))
    return std::nullopt;
  return Discovery::at(elapsed(now));
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

namespace {
constexpr float kWholeTurn = 6.2831853f;
// A part's progress at `seconds`: 0 before `from`, 1 after `from + length`
float partOf(double seconds, double from, float length) {
  return std::clamp(static_cast<float>((seconds - from) / length), 0.0f, 1.0f);
}
} // namespace

void HomePayShow::saw(const std::vector<TableGame::Played> &moves, const State &state, double now) {
  for (const TableGame::Played &move : moves) {
    if (move.action.kind == Action::Kind::RollHomeDie) {
      Pay pay{.player = move.player, .face = move.action.a, .seenAt = now, .paidUpAt = now};
      if (move.action.a >= 0 && move.action.a < kResources)
        pay.resource = static_cast<Resource>(move.action.a);
      if (pays_.empty()) {
        outAt_ = now;
        out_ = false;
      }
      pays_.push_back(pay);
    } else if (move.action.kind == Action::Kind::ChooseHome) {
      // (the star's choice, made: the latest of the player's still open)
      for (auto it = pays_.rbegin(); it != pays_.rend(); ++it)
        if (it->player == move.player && !it->resource) {
          it->resource = static_cast<Resource>(move.action.a);
          it->chosenAt = now;
          break;
        }
    }
  }
  if (pays_.empty())
    return;
  // Whether the home that paid last pays again: the dealer rolls its die
  // next (its die then waits out for it)
  Pay &last = pays_.back();
  const bool more = state.phase == Phase::HomeDieDraw && state.turn == last.player;
  if (last.moreAfter && !more)
    last.paidUpAt = now;
  last.moreAfter = more;
}

std::optional<double> HomePayShow::Timeline::leavesAt() const {
  if (spins.size() < pays || spins.empty() || !spins.back().cardAt)
    return std::nullopt;
  return *spins.back().cardAt + kSecondsCard + kSecondsHold;
}

std::optional<double> HomePayShow::Timeline::endAt() const {
  if (!backAt)
    return std::nullopt;
  return *backAt + kSecondsHome;
}

HomePayShow::Timeline HomePayShow::timeline() const {
  Timeline t{.outAt = outAt_};
  if (pays_.empty())
    return t;
  const int player = pays_.front().player;
  while (t.pays < pays_.size() && pays_[t.pays].player == player)
    ++t.pays;
  // Each spin once the one before's card leaves (the first once the die's
  // out), and not before its roll came; its card once it's landed and its
  // resource is known
  std::optional<double> due = outAt_ + kSecondsOut;
  for (std::size_t i = 0; i < t.pays && due; ++i) {
    const Pay &pay = pays_[i];
    Spin spin{.at = std::max(*due, pay.seenAt)};
    const double landed = spin.at + kSecondsSpin;
    if (pay.resource)
      spin.cardAt = pay.chosenAt ? std::max(landed, *pay.chosenAt) : landed;
    t.spins.push_back(spin);
    due = spin.cardAt ? std::optional(*spin.cardAt + kSecondsCard + kSecondsHold) : std::nullopt;
  }
  // Back as the last card leaves, once it's known no more come: another
  // home's pay queued, or the game said so
  const std::optional<double> leaves = t.leavesAt();
  if (!leaves)
    return t;
  const Pay &last = pays_[t.pays - 1];
  if (t.pays < pays_.size())
    t.backAt = std::max(*leaves, pays_[t.pays].seenAt);
  else if (!last.moreAfter)
    t.backAt = std::max(*leaves, last.paidUpAt);
  return t;
}

bool HomePayShow::waitingForGame(const Timeline &t, double now) const {
  // (the star landed, its choice to make)
  for (const Spin &spin : t.spins)
    if (!spin.cardAt)
      return now >= spin.at + kSecondsSpin;
  // (the last card gone, the next roll to come)
  const std::optional<double> leaves = t.leavesAt();
  return !t.backAt && leaves && now >= *leaves;
}

HomePayShow::Step HomePayShow::update(double now) {
  // The homes whose dice are back and settled, done with (the next home's
  // die out once the last is back)
  for (Timeline t = timeline(); !pays_.empty(); t = timeline()) {
    const std::optional<double> end = t.endAt();
    if (!end || now < *end)
      break;
    pays_.erase(pays_.begin(), pays_.begin() + static_cast<std::ptrdiff_t>(t.pays));
    out_ = false;
    if (!pays_.empty())
      outAt_ = std::max(*end, pays_.front().seenAt);
  }
  out_ = out_ || moving(now);
  Step step;
  const bool busy = moving(now) && !waitingForGame(timeline(), now);
  if (busy != held_) {
    held_ = busy;
    (busy ? step.hold : step.letGo) = true;
  }
  return step;
}

void HomePayShow::waitFor(double until) {
  if (!pays_.empty() && !out_ && outAt_ < until)
    outAt_ = until;
}

bool HomePayShow::moving(double now) const {
  if (pays_.empty() || now < outAt_)
    return false;
  const std::optional<double> end = timeline().endAt();
  return !end || now < *end;
}

std::optional<int> HomePayShow::dieOutOf(double now) const {
  const auto die = this->die(now);
  return die && die->out ? std::optional(pays_.front().player) : std::nullopt;
}

float HomePayShow::spinEnd(float from, int face) {
  // (the side's turn, a whole turn at a time below where it's from, till
  // it's at least a whole turn on)
  float to = homeDieTurn(face);
  while (to > from - kWholeTurn)
    to -= kWholeTurn;
  while (to < from - 2 * kWholeTurn)
    to += kWholeTurn;
  return to;
}

float HomePayShow::spinAt(float from, int face, float seconds) {
  using Motion::Curve;
  const float to = spinEnd(from, face);
  const float t = seconds / kSecondsSpin;
  constexpr float kWindUpEnds = 0.15f, kLandsAt = 0.85f;
  // Anticipation: back a little first, slowly, and held a moment
  const float wound = from + kWindTurn;
  if (t <= 0)
    return from;
  if (t < kWindUpEnds)
    return from + kWindTurn * Motion::along(Curve::EaseInOut, t / kWindUpEnds);
  // Off fast, slowing to the side (slow in) and carried past it a little
  const float past = to - kOvershoot;
  if (t < kLandsAt)
    return wound + (past - wound) *
                       Motion::along(Curve::EaseOut, (t - kWindUpEnds) / (kLandsAt - kWindUpEnds));
  // Follow-through: back to it, settling
  if (t < 1)
    return past + kOvershoot * Motion::along(Curve::EaseInOut, (t - kLandsAt) / (1 - kLandsAt));
  return to;
}

float HomePayShow::turnAt(const Timeline &t, double now) const {
  float turn = 0;
  for (std::size_t i = 0; i < t.spins.size() && now >= t.spins[i].at; ++i)
    turn = spinAt(turn, pays_[i].face, static_cast<float>(now - t.spins[i].at));
  return turn;
}

std::optional<HomePayShow::Die> HomePayShow::die(double now) const {
  if (!moving(now))
    return std::nullopt;
  using Motion::Curve;
  const Timeline t = timeline();
  Die die{.turn = turnAt(t, now), .out = true};
  // (space rippling where it leaves, from the wind-up into the lift)
  if (now < t.outAt + kSecondsRipple)
    die.ripple = partOf(now, t.outAt, kSecondsRipple);
  const double lifting = t.outAt + kSecondsWindUp;
  if (now < lifting) {
    // Anticipation: down into the board first, slow in and out, held a
    // moment at the bottom before it leaps (a crouch)
    die.sink = kDip * Motion::along(Curve::EaseInOut, partOf(now, t.outAt, kSecondsWindUp));
    return die;
  }
  if (!t.backAt || now < *t.backAt) {
    // Up out of its place and along its arc to face you, slow out and slow
    // in (Gentle); the dip taken back up as it rises
    die.lifted = Motion::along(Curve::Gentle, partOf(now, lifting, kSecondsLift));
    die.sink = kDip * (1 - std::min(1.0f, die.lifted / kHomeDieRisen));
    return die;
  }
  // Back: along the arc to over its place, turning face up, its spin
  // unwound to the nearest whole turn (its tile lies as it did); slow out
  // and in
  const double dropping = *t.backAt + kSecondsBack, touches = dropping + kSecondsDrop;
  if (now < dropping) {
    const float back = Motion::along(Curve::Gentle, partOf(now, *t.backAt, kSecondsBack));
    const float home = std::round(die.turn / kWholeTurn) * kWholeTurn;
    die.lifted = 1 - (1 - kHomeDieRisen) * back;
    die.turn += (home - die.turn) * back;
    return die;
  }
  die.turn = std::round(die.turn / kWholeTurn) * kWholeTurn;
  if (now < touches) {
    // Dropping into its place, faster and faster (timing: a heavy thing falls)
    die.lifted =
        kHomeDieRisen * (1 - Motion::along(Curve::EaseIn, partOf(now, dropping, kSecondsDrop)));
    return die;
  }
  // Follow-through: a sink a hair past its place and back (a thud, not a
  // bounce: it's heavy), and space rippling where it seats
  const float seated = partOf(now, touches, kSecondsSeat);
  die.sink = kSeatSink * std::sin(3.14159265f * seated);
  die.out = seated < 1;
  if (now < touches + kSecondsRipple)
    die.ripple = partOf(now, touches, kSecondsRipple);
  return die;
}

std::optional<HomePayShow::Card> HomePayShow::card(double now) const {
  const Timeline t = timeline();
  for (std::size_t i = 0; i < t.spins.size(); ++i) {
    const std::optional<double> at = t.spins[i].cardAt;
    const std::optional<Resource> resource = pays_[i].resource; // (known: it has a card)
    if (!at || !resource)
      continue;
    const double leaves = *at + kSecondsCard + kSecondsHold;
    if (now < *at || now >= leaves + kSecondsAway)
      continue;
    return Card{.order = static_cast<int>(i),
                .resource = *resource,
                .grown = partOf(now, *at, kSecondsCard),
                .away = partOf(now, leaves, kSecondsAway),
                .words = partOf(now, *at + kSecondsCard, kSecondsWordsIn)};
  }
  return std::nullopt;
}

bool HomePayShow::waitingForChoice(double now) const {
  for (const Spin &spin : timeline().spins)
    if (!spin.cardAt)
      return now >= spin.at + kSecondsSpin;
  return false;
}

// ---------------------------------------------------------------- Salvage

void SalvageShow::start(int player, int howMany, int you) {
  player_ = player;
  howMany_ = std::max(1, howMany);
  you_ = you;
  picks_.clear();
  begunAt_.reset();
}

void SalvageShow::saw(const std::vector<TableGame::Played> &moves, const State &state, int you,
                      double now) {
  for (const TableGame::Played &move : moves) {
    if (move.action.kind != Action::Kind::TakeSalvage)
      continue;
    if (!on())
      start(move.player, state.settings.salvageResources, you);
    if (move.player != player_)
      continue;
    const auto resource = static_cast<Resource>(move.action.a);
    // (the first pick not taken yet: yours, as picked, unless it differs;
    // else a new one)
    const auto open = std::ranges::find_if(picks_, [](const Pick &p) { return !p.takenAt; });
    if (open != picks_.end()) {
      if (open->resource != resource)
        *open = Pick{.resource = resource, .at = now};
      open->takenAt = now;
    } else if (static_cast<int>(picks_.size()) < howMany_) {
      picks_.push_back({.resource = resource, .at = now, .takenAt = now});
    }
  }
  if (state.phase == Phase::Salvage && !on())
    start(state.turn, state.settings.salvageResources, you);
}

void SalvageShow::begin(double now) {
  if (on() && !begunAt_)
    begunAt_ = now;
}

void SalvageShow::choosing(const std::vector<Resource> &picked, double now) {
  if (!on() || player_ != you_ ||
      std::ranges::any_of(picks_, [](const Pick &p) { return p.takenAt.has_value(); }))
    return;
  // (those still picked as they were stay; from the first changed on, afresh)
  std::size_t same = 0;
  while (same < picks_.size() && same < picked.size() && picks_[same].resource == picked[same])
    ++same;
  picks_.erase(picks_.begin() + static_cast<std::ptrdiff_t>(same), picks_.end());
  for (std::size_t i = same; i < picked.size() && static_cast<int>(i) < howMany_; ++i)
    picks_.push_back({.resource = picked[i], .at = now});
}

double SalvageShow::inAt(std::size_t pick) const {
  return std::max(picks_[pick].at, begunAt_.value_or(picks_[pick].at));
}

std::optional<double> SalvageShow::awayAt() const {
  if (!begunAt_ || static_cast<int>(picks_.size()) < howMany_)
    return std::nullopt;
  double away = 0;
  for (std::size_t i = 0; i < picks_.size(); ++i) {
    const std::optional<double> taken = picks_[i].takenAt;
    if (!taken)
      return std::nullopt;
    away = std::max({away, *taken, inAt(i) + paced(kSecondsIn + kSecondsRead)});
  }
  return away;
}

std::optional<double> SalvageShow::endAt() const {
  const std::optional<double> away = awayAt();
  if (!away)
    return std::nullopt;
  return *away + paced(kStagger * static_cast<float>(howMany_ - 1) + kSecondsAway);
}

bool SalvageShow::waitingForChoice(double now) const {
  return on() && begunAt_ && now >= *begunAt_ && picks_.empty();
}

std::vector<SalvageShow::Card> SalvageShow::cards(double now) const {
  std::vector<Card> shown;
  if (!begunAt_)
    return shown;
  const std::optional<double> away = awayAt();
  for (std::size_t i = 0; i < picks_.size(); ++i) {
    const double in = inAt(i);
    const float gone = away ? partOf(now, *away + paced(kStagger * static_cast<float>(i)),
                                     static_cast<float>(paced(kSecondsAway)))
                            : 0.0f;
    if (now < in || gone >= 1)
      continue;
    shown.push_back(
        {.slot = static_cast<int>(i),
         .resource = picks_[i].resource,
         .in = partOf(now, in, static_cast<float>(paced(kSecondsIn))),
         .words = partOf(now, in + paced(kSecondsIn), static_cast<float>(paced(kSecondsWordsIn))),
         .away = gone});
  }
  return shown;
}

float SalvageShow::dim(double now) const {
  if (!begunAt_ || picks_.empty())
    return 0;
  double first = inAt(0);
  for (std::size_t i = 1; i < picks_.size(); ++i)
    first = std::min(first, inAt(i));
  const float in = partOf(now, first, static_cast<float>(paced(kSecondsIn)));
  const std::optional<double> away = awayAt();
  const float gone = away ? partOf(now, *away + paced(kStagger * static_cast<float>(howMany_ - 1)),
                                   static_cast<float>(paced(kSecondsAway)))
                          : 0.0f;
  return kWordsDim * in * (1 - gone);
}

bool SalvageShow::moving(double now) const {
  if (!begunAt_ || picks_.empty())
    return false;
  for (std::size_t i = 0; i < picks_.size(); ++i)
    if (now < inAt(i) + paced(kSecondsIn + kSecondsWordsIn))
      return true;
  const std::optional<double> end = endAt();
  return end && now < *end;
}

bool SalvageShow::holding() const { return on() && awayAt().has_value(); }

SalvageShow::Step SalvageShow::update(double now) {
  if (const std::optional<double> end = endAt(); end && now >= *end) {
    player_ = -1; // (landed: over)
    picks_.clear();
    begunAt_.reset();
  }
  Step step;
  const bool busy = holding();
  if (busy != held_) {
    held_ = busy;
    (busy ? step.hold : step.letGo) = true;
  }
  return step;
}

// --------------------------------------------------------------- Roll pay

void RollPayShow::saw(const std::vector<TableGame::Played> &moves, const State &before,
                      double now) {
  const std::size_t had = cards_.size();
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
  // (joining a flight under way: from now, each after the last, so none
  // is first seen part way along)
  if (started_)
    for (std::size_t i = had; i < cards_.size(); ++i)
      cards_[i].at = std::max(now, i == 0 ? now : cards_[i - 1].at + kStagger);
}

void RollPayShow::cross(int from, int to, int bundle) {
  const Resources cards = resourcesOf(bundle);
  for (int r = 0; r < kResources; ++r)
    for (int n = 0; n < cards[static_cast<std::size_t>(r)]; ++n)
      cards_.push_back({-1, to, static_cast<Resource>(r), from});
}

double RollPayShow::seconds() const {
  if (cards_.empty())
    return 0;
  if (started_)
    return endsAt() - startAt_;
  return kStagger * static_cast<double>(cards_.size() - 1) + kSecondsPop + kSecondsFly;
}

double RollPayShow::endsAt() const {
  return cards_.empty() ? startAt_ : cards_.back().at + kSecondsPop + kSecondsFly;
}

void RollPayShow::start(double now) {
  if (!waiting())
    return;
  startAt_ = now;
  started_ = true;
  for (std::size_t i = 0; i < cards_.size(); ++i)
    cards_[i].at = now + kStagger * static_cast<double>(i);
}

std::optional<RollPayShow::Moment> RollPayShow::at(std::size_t i, double now) const {
  if (i >= cards_.size() || !started_)
    return std::nullopt;
  const auto t = static_cast<float>(now - cards_[i].at);
  if (t < 0 || t >= kSecondsPop + kSecondsFly)
    return std::nullopt;
  return Moment{Motion::along(Motion::Curve::Settle, std::min(1.0f, t / kSecondsPop)),
                Motion::along(Motion::Curve::Gentle,
                              std::clamp((t - kSecondsPop) / kSecondsFly, 0.0f, 1.0f))};
}

bool RollPayShow::moving(double now) const {
  return started_ && !cards_.empty() && now >= startAt_ && now < endsAt() - kSlack;
}

RollPayShow::Step RollPayShow::update(double now) {
  Step step;
  if (!started_ || cards_.empty() || now < startAt_)
    return step;
  if (!held_) {
    held_ = true;
    step.hold = true;
  }
  if (now < endsAt() - kSlack)
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

void CardPlayShow::waitFor(double until) {
  if (!queue_.empty() && startedAt_ && !shown_ && *startedAt_ < until)
    startedAt_ = until;
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
  shown_ = true;
  if (!held_) {
    held_ = true;
    step.hold = true;
  }
  if (now - *startedAt_ < kSeconds)
    return step;
  queue_.erase(queue_.begin());
  shown_ = false;
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

bool HandShow::queued() const {
  return std::any_of(queued_.begin(), queued_.end(), [](int n) { return n > 0; });
}

void HandShow::start(double now) {
  if (!queued() || showing(now))
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
