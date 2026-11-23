// TestTables.hpp - games for the handheld's tests, played on to where a
// test wants them: a game on the agreed rules played by each player's
// first legal move (the dealer by chance), a table with you at it, and a
// state on its way to the screens on its own
//
//   const State main = TestTables::atFirstMain();
//   TableGame game(TestTables::yourTable(5));
//   TestTables::playedOnUntil(game, [](const TableGame &g) { return g.waitingForDice(); });
#pragma once

#include <CosmicHex/Handheld/TableGame.hpp>
#include <CosmicHex/Handheld/TableSnapshot.hpp>
#include <CosmicHex/Rules.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace CosmicHex::Handheld::TestTables {

// A four-player game on the agreed rules (boardR3), each player making
// their first legal move and the dealer drawing from `seed`, until `until`
// says stop, someone wins, or `limit` moves are made
State playedUntil(const std::function<bool(const State &)> &until, std::uint64_t seed = 11,
                  int limit = 4000);
// ...buying a card whenever one can be bought
State playedBuyingUntil(const std::function<bool(const State &)> &until, std::uint64_t seed = 23,
                        int limit = 6000);
// The first main part of a turn (the start's placing done)
State atFirstMain(std::uint64_t seed = 11);
// An offer to you: the last seat's to anyone, one of a resource they hold
// for one of the next along, which you (next round the table) answer first
State offeredToYou(std::uint64_t seed = 11);

// You at a table (player 0) with three bots that think little, the dice
// thrown on the screen (physicsDice), as the app's table is
TableSettings yourTable(std::uint64_t seed);
// The table played on until `until` says stop: the dealer's draws and the
// bots' moves, your moves the first you have, the dice showing a 2 and a 4.
// False if it never does in `limit` moves
bool playedOnUntil(TableGame &game, const std::function<bool(const TableGame &)> &until,
                   int limit = 400);

// `state` as the screens get it with `played` just made: nobody's move,
// the dice not waiting
TableSnapshot snapshotWith(State state, std::vector<TableGame::Played> played = {});

} // namespace CosmicHex::Handheld::TestTables
