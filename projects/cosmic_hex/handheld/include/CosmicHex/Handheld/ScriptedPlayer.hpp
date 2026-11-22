// ScriptedPlayer.hpp - player 0's moves made through the touch panel, as a
// person makes them: what the bottom screen shows read (its mode, its
// buttons, the places it marks), and a touch or a button chosen from it,
// one at a time. A choice of where is random (by its seed), the rest
// sensible: roll when the dice wait, build what can be built, move a ship,
// else end the turn; pick, keep, steal and answer when a choosing screen
// opens. The scripted runs on the Mac (ScriptedGame) and the DS's
// self-playing build (cosmic-hex-ds-scripted) both ask it what to do next.
//
//   ScriptedPlayer player(seed);
//   const ScriptedInput input = player.next(snapshot, bottomScreen, map, yourRollWaits);
//   // input.kind: a tap at (input.x, input.y) on the touch panel, or a
//   // press of input.key (BottomScreen's keys: the pad's)
#pragma once

#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/TableSnapshot.hpp>

#include <SeededRandom/Generator.hpp>

#include <cstdint>

namespace CosmicHex::Handheld {

struct ScriptedInput {
  enum class Kind : std::uint8_t { None, Tap, Press };
  Kind kind = Kind::None;
  int x = 0, y = 0;        // (a tap: where, in the touch panel's pixels)
  BottomScreen::Key key{}; // (a press)
};

class ScriptedPlayer {
public:
  explicit ScriptedPlayer(std::uint64_t seed);

  // What to do next, looking at the game (`snapshot`), the touch panel
  // (`screen`) and the map on it (`map`: where each corner is); nothing
  // while it isn't your move
  ScriptedInput next(const TableSnapshot &snapshot, const BottomScreen &screen,
                     const BoardPicture &map, bool yourRollWaits);

private:
  ScriptedInput onTheBoard(const TableSnapshot &snapshot, const BottomScreen &screen,
                           const BoardPicture &map, bool yourRollWaits);
  ScriptedInput placing(const BottomScreen &screen, const BoardPicture &map);
  ScriptedInput building();
  bool chance(int percent);

  SeededRandom::Generator random_;
  // This turn (round and whose): what's been tried on the board, and the
  // build screen's row being gone to
  int round_ = -1, turn_ = -1;
  int tries_ = 0;
  int buildRow_ = 0, buildSteps_ = 0;
  // The same screen for many inputs: something isn't working (Back, then
  // End Turn)
  std::uint64_t lastLook_ = 0;
  int sameLook_ = 0;
};

} // namespace CosmicHex::Handheld
