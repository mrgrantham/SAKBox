// TradeRoom.hpp - what the trade room does, from the rules: a tab for each
// partner (the bank, and each other player while your turn's trading with
// players is open), the cards you give and get, the deal in words and why
// it can't be made, and the moves that make it. When a player offers you a
// trade, or answers yours with a counter-offer, it's that answer instead:
// what they give and want, what changed, and Accept or Decline; and an
// offer to you can be countered: the same cards picked as for an offer,
// starting from theirs turned round, sent back as a counter-offer for them
// to accept or decline. The screen (ChoiceScreens::drawTrade) shows it;
// the rules check every move.
//
//   room.update(state, legal, yourMove);
//   room.choosePartner(TradeRoom::kBank);
//   room.add(TradeRoom::Side::Give, Resource::Minerals);
//   if (room.whyNot().empty()) for (const Action &a : room.moves()) send(a);
#pragma once

#include <CosmicHex/Handheld/PlayerNames.hpp>
#include <CosmicHex/Rules.hpp>

#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

class TradeRoom {
public:
  static constexpr int kBank = -1; // (a partner: the bank; else a player)
  enum class Side { Give, Get };

  // Who's who (the tabs and the words say names, not seat numbers)
  void setNames(PlayerNames names) { names_ = std::move(names); }
  const PlayerNames &names() const { return names_; }

  // A new look at the game (your move: `yourMove`, as player 0)
  void update(const State &state, const std::vector<Action> &legal, bool yourMove);

  // The partners in tab order: the bank, then the other players
  std::vector<int> partners() const;
  // Whether a partner can be traded with now (and why not, when not)
  bool open(int partner) const { return whyClosed(partner).empty(); }
  std::string whyClosed(int partner) const;
  int partner() const { return partner_; }
  void choosePartner(int partner);

  // The cards picked to give and to get. With the bank: one kind given
  // (as many as its rate), one got; with a player, any cards
  const Resources &give() const { return give_; }
  const Resources &get() const { return get_; }
  void add(Side side, Resource resource);
  void remove(Side side, Resource resource);
  const Resources &held() const;

  // The deal in words ("4 Minerals for 1 Food, from the bank"), and why it
  // can't be made now (empty when it can)
  std::string deal() const;
  std::string whyNot() const;
  // What makes it, in order: with the bank, ending the turn's offers to
  // players first if they're still open; with a player, the offer (or,
  // countering, the counter-offer)
  std::vector<Action> moves() const;

  // Answering: an offer to you, or a counter to yours (the screen opens by
  // itself). Who from; what they'd give you and want from you; the kinds
  // that changed from your offer (a counter); Accept and Decline
  bool answering() const { return answering_; }
  bool counterToYours() const { return counterToYours_; } // (not an offer to you)
  int from() const { return from_; }
  // What's being answered, for its banner: "VEGA OFFERS A TRADE" (or
  // "VEGA COUNTERS"), and the deal: "2 Fuel for your 1 Water"
  std::string headline() const;
  std::string offerWords() const;
  const Resources &theyGive() const { return theyGive_; }
  const Resources &theyWant() const { return theyWant_; }
  const std::vector<Resource> &changed() const { return changed_; }
  std::optional<Action> accept() const;
  std::optional<Action> decline() const;

  // Countering an offer to you (the rules let its answerer counter it once;
  // a counter to yours can only be accepted or declined): the cards picked
  // as for an offer, starting from theirs turned round (you give what they
  // want, and get what they give); moves() then makes the counter-offer.
  // Back to the offer as it was with stopCountering()
  bool canCounter() const { return answering_ && !counterToYours_; }
  void startCountering();
  void stopCountering() { countering_ = false; }
  bool countering() const { return countering_; }
  // The offer to answer showing (answering, and not countering it)
  bool showingOffer() const { return answering_ && !countering_; }

private:
  PlayerNames names_;
  State state_;
  std::vector<Action> legal_;
  bool yourMove_ = false;
  int partner_ = kBank;
  Resources give_{}, get_{};
  bool answering_ = false, counterToYours_ = false, countering_ = false;
  int from_ = -1;
  Resources theyGive_{}, theyWant_{};
  std::vector<Resource> changed_;
  int rate() const;
};

} // namespace CosmicHex::Handheld
