// TestGames.hpp - games set up for the rules' tests: a game at a player's
// main part (on hexes, or on corners and edges), the start played through,
// planets turned over by hand, and the small lookups the tests share.
#pragma once

#include <CosmicHex/Rules.hpp>

#include <cstddef>
#include <functional>
#include <vector>

namespace CosmicHex::TestGames {

// A resource as an index into Resources
constexpr std::size_t of(Resource resource) { return static_cast<std::size_t>(resource); }

// Whether the actions hold this action, or any of this kind
bool has(const std::vector<Action> &actions, const Action &action);
bool hasKind(const std::vector<Action> &actions, Action::Kind kind);

// How many resources the player holds
int cardsOf(const State &state, int player);

// Plays on until `until` holds (checked before each action), the quickest
// way through each turn: trading and the turn ended as soon as they can be,
// else the first legal action; the dealer's draws at random (a fixed
// seed). Whether it got
// there within `most` actions
bool playOnUntil(State &state, const std::function<bool(const State &)> &until, int most = 2000);

// ---- On hexes (the rules' first version: stations docked on planets)

// A game at player 0's main part, after their home income and a quiet roll
// (a 3: nothing turned over yet pays)
State atMain(int players = 2);
// Player 0's trading step (Settings::playerTrading), after the same start
State atTrading(int players = 3);

// The planet (or zone tile) on a hex, or -1
int planetIndex(const State &state, Hex hex);
int siteIndex(const State &state, Hex hex);
// Turns a planet over by hand, as a reveal would
void flip(State &state, int planet, PlanetKind kind, int number);
// Docks a station of the player's on the planet, in its first free dock (a
// colony if asked), as settling would
void dock(State &state, int planet, int player, bool colony = false);

// ---- On corners and edges (Settings::corners)

// Corners, on layout R2 (layouts/layouts.txt: clusters of 3, 2 and 1)
Settings cornerSettings();
// Plays the start by taking each placement's first choice
State afterSetup(int players = 4, int first = 0);
// A game in its main part, player 0's turn, every planet turned over as a
// gas giant numbered 6, and 5 of each resource to spend
State cornersAtMain();

// The player's first ship (ships are placed in reverse order, so it may not
// be first in the list)
int firstShipOf(const State &state, int player);
int shipsOwned(const State &state, int player);
// A corner touching exactly this many planets, none of them a home
Corner cornerTouching(const State &state, int planetsTouched);
// How many planets (and homes) a corner touches
int planetsAt(const State &state, Corner corner);
bool touchesHex(Corner corner, Hex hex);
// No station on the corner or next to it (the distance rule)
bool clearOfStations(const State &state, Corner corner);

} // namespace CosmicHex::TestGames
