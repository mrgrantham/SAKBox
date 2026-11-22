#include <CosmicHex/Handheld/GameWords.hpp>
#include <CosmicHex/Handheld/TradeRoom.hpp>

#include <algorithm>
#include <array>
#include <numeric>

namespace CosmicHex::Handheld {

namespace {
using Kind = Action::Kind;
constexpr int kYou = 0; // (the player at the console)
int total(const Resources &r) { return std::accumulate(r.begin(), r.end(), 0); }
// "2 Fuel and 1 Water"
std::string words(const Resources &r) {
  std::string out;
  int parts = 0;
  for (int i = 0; i < kResources; ++i) {
    const int n = r[static_cast<std::size_t>(i)];
    if (n == 0)
      continue;
    out += (parts++ > 0 ? " and " : "") + std::to_string(n) + " " +
           std::string(GameWords::resource(static_cast<std::size_t>(i)));
  }
  return parts == 0 ? "nothing" : out;
}
bool has(const std::vector<Action> &legal, Kind kind) {
  return std::any_of(legal.begin(), legal.end(), [&](const Action &a) { return a.kind == kind; });
}
} // namespace

void TradeRoom::update(const State &state, const std::vector<Action> &legal, bool yourMove) {
  const bool sameTurn = state.turn == state_.turn && state.round == state_.round;
  state_ = state;
  legal_ = legal;
  yourMove_ = yourMove;
  if (!sameTurn) {
    give_ = get_ = {};
    partner_ = kBank;
  }
  // (what's picked can't be more than you hold now)
  for (std::size_t r = 0; r < give_.size(); ++r)
    give_[r] = std::min(give_[r], held()[r]);
  // Answering: a player's offer to you, or their counter to yours
  answering_ = counterToYours_ = false;
  changed_.clear();
  const TradeOffer &offer = state.offer;
  if (yourMove && state.phase == Phase::TradeReply && offer.replier == kYou) {
    answering_ = true;
    from_ = state.turn;
    theyGive_ = resourcesOf(offer.give);
    theyWant_ = resourcesOf(offer.get);
  } else if (yourMove && state.phase == Phase::TradeCounter && state.turn == kYou) {
    answering_ = counterToYours_ = true;
    from_ = offer.replier;
    theyGive_ = resourcesOf(offer.counterGive);
    theyWant_ = resourcesOf(offer.counterGet);
    // (what they changed from your offer: they give you what you asked
    // for, and want what you gave)
    const Resources asked = resourcesOf(offer.get), gave = resourcesOf(offer.give);
    for (int r = 0; r < kResources; ++r) {
      const auto i = static_cast<std::size_t>(r);
      if (theyGive_[i] != asked[i] || theyWant_[i] != gave[i])
        changed_.push_back(static_cast<Resource>(r));
    }
  }
  // (a counter only while there's an offer to you to counter)
  countering_ = countering_ && canCounter();
}

std::vector<int> TradeRoom::partners() const {
  std::vector<int> out{kBank};
  for (int i = 1; i < state_.players; ++i)
    out.push_back((kYou + i) % state_.players);
  return out;
}

std::string TradeRoom::whyClosed(int partner) const {
  if (countering_)
    return ""; // (the one you're countering)
  if (!yourMove_)
    return "Trading: on your turn";
  if (partner == kBank)
    return has(legal_, Kind::Trade) || has(legal_, Kind::EndTrading) ? ""
                                                                     : "The bank: after your roll";
  if (state_.phase != Phase::Trading)
    return state_.tradingDone
               ? "Offers to players: before trading with the bank, building or moving"
               : "Offers to players: after your roll";
  if (total(state_.player[static_cast<std::size_t>(partner)].resources) == 0)
    return names_[partner] + " has no cards";
  return "";
}

void TradeRoom::choosePartner(int partner) {
  if (partner == partner_)
    return;
  partner_ = partner;
  give_ = get_ = {};
}

const Resources &TradeRoom::held() const {
  return state_.player[static_cast<std::size_t>(kYou)].resources;
}

int TradeRoom::rate() const { return tradeRateOf(state_, kYou); }

void TradeRoom::add(Side side, Resource resource) {
  const auto r = static_cast<std::size_t>(resource);
  Resources &picked = side == Side::Give ? give_ : get_;
  if (partner_ == kBank) {
    // (one kind each way: the rate given, 1 got; and not the same kind)
    picked = {};
    picked[r] = side == Side::Give ? rate() : 1;
    Resources &other = side == Side::Give ? get_ : give_;
    other[r] = 0;
    return;
  }
  if (side == Side::Give && picked[r] >= held()[r])
    return;           // (no more than you hold)
  if (picked[r] < 15) // (a bundle holds up to 15 of each)
    ++picked[r];
}

void TradeRoom::remove(Side side, Resource resource) {
  const auto r = static_cast<std::size_t>(resource);
  Resources &picked = side == Side::Give ? give_ : get_;
  if (partner_ == kBank)
    picked[r] = 0;
  else if (picked[r] > 0)
    --picked[r];
}

std::string TradeRoom::deal() const {
  if (total(give_) == 0 && total(get_) == 0)
    return partner_ == kBank
               ? "The bank: " + std::to_string(rate()) + " of one kind for 1 of another"
               : "Offer P" + std::to_string(partner_ + 1) + " some cards for some of theirs";
  return words(give_) + " for " + words(get_) +
         (partner_ == kBank ? ", from the bank"
          : countering_     ? ", back to " + names_[partner_]
                            : ", offered to " + names_[partner_]);
}

std::string TradeRoom::headline() const {
  return names_.upperOf(from_) + (counterToYours_ ? " COUNTERS" : " OFFERS A TRADE");
}

std::string TradeRoom::offerWords() const {
  return words(theyGive_) + " for your " + words(theyWant_);
}

std::string TradeRoom::whyNot() const {
  if (const std::string closed = whyClosed(partner_); !closed.empty())
    return closed;
  if (total(give_) == 0)
    return "Pick what you give";
  if (total(get_) == 0)
    return "Pick what you get";
  for (std::size_t r = 0; r < give_.size(); ++r)
    if (give_[r] > held()[r])
      return "You hold " + std::to_string(held()[r]) + " " + std::string(GameWords::resource(r));
  return "";
}

std::vector<Action> TradeRoom::moves() const {
  if (!whyNot().empty())
    return {};
  if (partner_ == kBank) {
    int give = -1, get = -1;
    for (int r = 0; r < kResources; ++r) {
      if (give_[static_cast<std::size_t>(r)] > 0)
        give = r;
      if (get_[static_cast<std::size_t>(r)] > 0)
        get = r;
    }
    std::vector<Action> out;
    if (state_.phase == Phase::Trading)
      out.push_back({Kind::EndTrading}); // (the bank's trades come after the players')
    out.push_back({Kind::Trade, give, get});
    return out;
  }
  if (countering_)
    return {{Kind::CounterTrade, bundleOf(give_), bundleOf(get_)}};
  return {{Kind::OfferTrade, bundleOf(give_), bundleOf(get_), partner_}};
}

void TradeRoom::startCountering() {
  if (!canCounter())
    return;
  countering_ = true;
  partner_ = from_;
  get_ = theyGive_;
  give_ = theyWant_;
  // (what's picked can't be more than you hold)
  for (std::size_t r = 0; r < give_.size(); ++r)
    give_[r] = std::min(give_[r], held()[r]);
}

std::optional<Action> TradeRoom::accept() const {
  if (!answering_)
    return std::nullopt;
  for (const Action &a : legal_)
    if (a.kind == Kind::AcceptTrade)
      return a;
  return std::nullopt;
}

std::optional<Action> TradeRoom::decline() const {
  if (!answering_)
    return std::nullopt;
  for (const Action &a : legal_)
    if (a.kind == Kind::DeclineTrade)
      return a;
  return std::nullopt;
}

} // namespace CosmicHex::Handheld
