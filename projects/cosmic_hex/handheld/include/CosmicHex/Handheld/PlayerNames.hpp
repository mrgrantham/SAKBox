// PlayerNames.hpp - who's who at the table, by name rather than seat
// number (the user's: a roll for who goes first means something when you
// see that Josh became the second to play). The person playing is named
// as their console's owner (ScratchPadd::OwnerName), or "You"; the bots
// take short star names, a different few each game, never the person's.
// Each player keeps their seat's colour.
//
//   const PlayerNames names = PlayerNames::forGame(4, 0, "James", seed);
//   names[2]          // "Orion"
//   names.shortOf(2)  // at most kShortLetters, for narrow tabs
#pragma once

#include <CosmicHex/Rules.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace CosmicHex::Handheld {

class PlayerNames {
public:
  // `person`: the seat played by a person (-1: all bots), named `yours`
  // ("You" when empty); the others from the stars, chosen by `seed`
  static PlayerNames forGame(int players, int person, std::string_view yours, std::uint64_t seed);

  const std::string &operator[](int player) const;
  // As many letters as fit a narrow place (a tab): kShortLetters
  static constexpr std::size_t kShortLetters = 7;
  std::string shortOf(int player) const;
  // Upper case, for banners ("ORION'S TURN")
  std::string upperOf(int player) const;

private:
  std::array<std::string, kMaxPlayers> names_;
};

} // namespace CosmicHex::Handheld
