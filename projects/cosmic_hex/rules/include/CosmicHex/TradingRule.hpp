// TradingRule.hpp - how the bots trade with each other (Settings::
// playerTrading), by a simple rule rather than by search (the offers
// possible are far too many to search), as agreed: start with
// the rule, and measure.
//
// A player's next build is the first of a colony, a station, a ship and a
// card they could use and are at most two resources short of. They offer
// one of what they hold most beyond it for one of what they're short of
// (then two for one), each offer once a turn, and stop when there's none
// left to make. Another player accepts an offer that brings their own next
// build closer and costs only what they hold beyond it, unless the
// offerer's within two points of winning (Characteristics of Games: trading
// in a game of three or more invites kingmaking). The bots don't make
// counter-offers; they answer a person's by the same rule.
#pragma once

#include <CosmicHex/Rules.hpp>

namespace CosmicHex {

// What a player's next build is short of, and what they hold beyond it
struct TradeNeeds {
  Resources shortfall{}, spare{};
  bool wantsSomething = false; // (a build two resources short at most)
};
TradeNeeds tradeNeedsOf(const State &state, int player);

// The bot's move in a trading step (Phase::Trading, TradeReply or
// TradeCounter): an offer or the end of trading; accepting or declining
Action tradingRuleMove(const State &state);

} // namespace CosmicHex
