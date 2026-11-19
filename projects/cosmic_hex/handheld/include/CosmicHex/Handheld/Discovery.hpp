// Discovery.hpp - a tile turned over, shown big on the top screen (the
// user's: "show a large view of the finds and planets on the top screen
// when they get discovered"; designed in the roadmap's discovery mock-up).
// The tile turns over in the middle of the screen, its name and what it
// gives beside it; a planet's number token drops onto it and settles;
// it holds, then shrinks back into its place on the board. Only then does
// the bottom screen's map show it, fading in.
//
//   const auto found = Discovery::between(before, after); // the tiles turned over
//   const Discovery::Moment m = Discovery::at(seconds);    // where the show is
//   m.turn, m.chip, m.shrink, m.fade                      // each 0 to 1
//
// No drawing here: the app draws from the Moment (and it's tested on its
// own).
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Rules.hpp>

#include <string>
#include <vector>

namespace CosmicHex::Handheld::Discovery {

// A tile turned over: where, its faces, its number (a planet's, else 0),
// and its words
struct Found {
  Hex hex;
  bool planet = true; // (else a deep-space find)
  Art::Face faceDown, face;
  int number = 0;
  int by = 0;        // the player whose ship turned it over
  std::string name;  // "Gas giant", "Ancient ruins"
  std::string gives; // "Fuel on a 6", "+2 badges"
};

// The tiles turned over from `before` to `after`, planets first
std::vector<Found> between(const State &before, const State &after);

// The show's parts, in seconds
inline constexpr float kSecondsTurn = 0.45f;  // turning over, big
inline constexpr float kSecondsChip = 0.4f;   // the number dropping on
inline constexpr float kSecondsHold = 1.5f;   // read it
inline constexpr float kSecondsShrink = 0.4f; // back into its place
inline constexpr float kSecondsFade = 0.4f;   // the bottom screen's tile fading in
inline constexpr float kSeconds =
    kSecondsTurn + kSecondsChip + kSecondsHold + kSecondsShrink + kSecondsFade;

// Where the show is, `seconds` in: each part 0 (not begun) to 1 (done),
// eased (Motion)
struct Moment {
  float turn = 0;   // the big tile: face down at 0, face up at 1
  float chip = 0;   // the token's drop (Settle: a small bounce)
  float shrink = 0; // 0 big in the middle, 1 in its place on the board
  float fade = 0;   // the bottom screen's tile: 0 face down, 1 shown
  bool done = false;
};
Moment at(float seconds);

} // namespace CosmicHex::Handheld::Discovery
