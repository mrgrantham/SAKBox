// BotStyles.hpp - ways for Cosmic Hex's bots to play, for testing the rules
// against different players (round 41): the same search (GameBots::Ismcts),
// with the bot's own moves in its imagined games (rollouts) leaning one way.
//
//   Plain     every move as likely: the bots as they were
//   Saver     holds on to resources for the costliest builds (colonies and
//             Charters): ends its turn more, spends on cheap things less.
//             Tests the Imperial Tithe, which only bites big hands.
//   Rusher    builds whatever it can at once: ships, stations, colonies
//   Explorer  flies out and turns tiles over: moves and ships, badges
//
//   GameBots::Ismcts<CosmicHex::Game> bot({.rolloutWeight = CosmicHex::rolloutWeightFor(style)});
//
// The weights are each move's likelihood against the others (1 is plain);
// they lean the bot's search, they don't script it: it still picks the move
// whose imagined games went best.
#pragma once

#include <CosmicHex/Rules.hpp>

#include <array>
#include <functional>
#include <optional>
#include <string_view>

namespace CosmicHex {

enum class BotStyle { Plain, Saver, Rusher, Explorer };

inline constexpr std::array<std::string_view, 4> kBotStyleNames = {"plain", "saver", "rusher",
                                                                   "explorer"};

inline std::string_view nameOf(BotStyle style) {
  return kBotStyleNames[static_cast<std::size_t>(style)];
}

inline std::optional<BotStyle> botStyleNamed(std::string_view name) {
  for (std::size_t i = 0; i < kBotStyleNames.size(); ++i)
    if (kBotStyleNames[i] == name)
      return static_cast<BotStyle>(i);
  return std::nullopt;
}

// How likely a move is in a bot's rollouts, for its style
inline double styleWeight(BotStyle style, const Action &action) {
  using Kind = Action::Kind;
  switch (style) {
  case BotStyle::Plain:
    return 1.0;
  case BotStyle::Saver:
    switch (action.kind) {
    case Kind::EndTurn:
      return 4.0; // keeps what it has
    case Kind::BuildColony:
      return 3.0; // what it saves for
    case Kind::BuyContract:
      return 2.0;
    case Kind::BuildShip: // cheap things, rarely
    case Kind::Trade:
      return 0.3;
    case Kind::Settle:
      return 0.5;
    default:
      return 1.0;
    }
  case BotStyle::Rusher:
    switch (action.kind) {
    case Kind::BuildShip:
      return 3.0;
    case Kind::Settle:
      return 4.0;
    case Kind::BuildColony:
      return 3.0;
    case Kind::BuyContract:
    case Kind::Trade:
      return 2.0; // (whatever gets the next build)
    case Kind::EndTurn:
      return 0.3;
    default:
      return 1.0;
    }
  case BotStyle::Explorer:
    switch (action.kind) {
    case Kind::Move:
      return 3.0;
    case Kind::BuildShip:
    case Kind::CashBadges:
      return 2.0;
    case Kind::Settle: // keeps its ships flying
    case Kind::EndTurn:
      return 0.5;
    default:
      return 1.0;
    }
  }
  return 1.0;
}

// The style as GameBots::Ismcts' rolloutWeight (empty for Plain: the
// bots' usual random rollouts)
inline std::function<double(const State &, const Action &)> rolloutWeightFor(BotStyle style) {
  if (style == BotStyle::Plain)
    return {};
  return [style](const State &, const Action &action) { return styleWeight(style, action); };
}

} // namespace CosmicHex
