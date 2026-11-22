// CardPlaces.hpp - where the cards flying over the top screen start and
// land, for every show that flies one (a roll's pay, the starting hand
// dealt, steals, trades, what's paid to the bank, a home's pay, a card
// played): one place for each player, the bank and a player's home, so
// no show can aim somewhere of its own.
//
// - You: your hand, below the screen.
// - Another player: the middle of their row of the points bars (the HUD's
//   rows: PointsBars::rowOf), where the card lands and goes.
// - The bank: behind the ROUND hexagon, top left (the HUD's, fixed on the
//   screen).
// - What a player pays to the bank leaves from their home on the board
//   (their home planet, or the one exhausted that was), seen where the
//   camera has it now.
//
//   const CardPlaces::Point to = CardPlaces::at(frame, state, card.player, you);
#pragma once

#include <CosmicHex/Handheld/TopViews.hpp>
#include <CosmicHex/Rules.hpp>

#include <optional>

namespace CosmicHex::Handheld::CardPlaces {

using Point = ScreenPoint;

// The bank, as a card's player or `from`
inline constexpr int kBank = RollPayShow::kBank;

// The bank's place on a screen `width` wide: the ROUND hexagon's middle
Point bank(float width);
// Your hand, below a screen `width` x `height`
Point yourHand(float width, float height);
// Another player's: the middle of their row of the points bars
Point playerRow(const State &state, float width, int player);
// Where cards to or from `who` (a player, or kBank) go, `you` the player
// whose hand is below (none, -1, while watching)
Point of(const State &state, float width, float height, int who, int you);

// The planet a player's payments leave from: their home, or the one that
// was theirs until it was exhausted; none when they have neither
std::optional<Hex> homeOf(const State &state, int player);

// `of` on a frame's screen
Point at(const TopFrame &frame, const State &state, int who, int you);
// Where a payment from `payer` to the bank leaves: their home on the
// screen as the camera has it this frame (following it as it moves), else
// their place (`at`)
Point paidFrom(const TopFrame &frame, const State &state, int payer, int you);

// Where a flying card (RollPayShow's) starts: the planet paying it, the
// payer's home when it's paid to the bank, else whoever it's from (`at`).
// It lands at `at` its player
Point flightStart(const TopFrame &frame, const State &state, const RollPayShow::Card &card,
                  int you);

} // namespace CosmicHex::Handheld::CardPlaces
