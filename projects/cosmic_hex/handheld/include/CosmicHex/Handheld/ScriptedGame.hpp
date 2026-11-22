// ScriptedGame.hpp - a whole game played with no screens, as the app plays
// it: the table (TableGame, paced as TablePadd paces it: a move each
// kSecondsAMove, a bot's search taking time on its thread, held while the
// shows hold it, its messages answered between moves), the shows
// (GameShows: the app's own), the touch panel (BottomScreen) and player
// 0's touches and buttons (ScriptedPlayer), on a clock of its own, a frame
// at a time. Every show is logged (ShowLog), so the runs find the shows cut
// short or skipped without the player asking (ScriptedGame_test, and the
// scripted_play tool's many seeds).
//
//   ScriptedGame game({.seed = 7});
//   game.run();
//   for (const ShowLog::Entry &entry : game.log().entries()) ...
#pragma once

#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/BottomScreen.hpp>
#include <CosmicHex/Handheld/GameShows.hpp>
#include <CosmicHex/Handheld/ScriptedPlayer.hpp>
#include <CosmicHex/Handheld/SettingsScreen.hpp>
#include <CosmicHex/Handheld/ShowLog.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>

namespace CosmicHex::Handheld {

struct ScriptedGameSettings {
  std::uint64_t seed = 1;
  int players = 4;
  bool person = true; // (player 0 played by the script; else four bots, watched)
  Animations animations = Animations::Full;
  double frameSeconds = 1.0 / 60;   // (the app's frames: a DS's are often slower)
  double thinkSeconds = 0.8;        // (a bot's move, on the table's thread: TablePadd's botSeconds)
  double tapSeconds = 0.5;          // (between the player's touches and buttons)
  double messageSeconds = 1.0 / 60; // (a message between the screens and the table)
  int botIterations = 12;           // (the bots' search: few, for speed)
  // How often (percent of the player's chances) B is pressed while
  // something animates, as an impatient player does: 0, never
  int impatience = 0;
  double mostSeconds = 3 * 3600; // (a game stopped there if it hasn't ended)
};

class ScriptedGame {
public:
  explicit ScriptedGame(const ScriptedGameSettings &settings);
  // (it stays where it's made: the shows' hooks hold on to it)
  ScriptedGame(const ScriptedGame &) = delete;
  ScriptedGame &operator=(const ScriptedGame &) = delete;

  // One frame on; false once the game's over (its winner celebrated) or
  // out of time
  bool frame();
  // The whole game
  void run();

  bool over() const { return over_; }
  double seconds() const { return now_; }
  int inputs() const { return inputs_; }
  const ShowLog &log() const { return log_; }
  const TableGame &table() const { return table_; }
  const GameShows &shows() const { return shows_; }
  const BottomScreen &screen() const { return screen_; }
  bool tableHeld() const { return tableHeld_; }

  // Each frame, before the shows move on (a test's own look, or input)
  void onFrame(std::function<void(ScriptedGame &)> look) { onFrame_ = std::move(look); }
  // The player's input, as the app takes it: a tap on the touch panel, or a
  // button (B first takes what's animating to its end, as the app's does)
  void apply(const ScriptedInput &input);

private:
  using Time = GameShows::RealClock::time_point;
  Time at(double seconds) const;
  // The table's thread: its messages, and a move each kSecondsAMove
  void runTable();
  void sendSnapshot(double arrives);
  void post(std::function<void()> message);

  ScriptedGameSettings settings_;
  double now_ = 0;
  TableGame table_;
  GameShows shows_;
  BottomScreen screen_;
  BoardPicture map_;
  ScriptedPlayer player_;
  SeededRandom::Generator impatient_; // (when B is pressed)
  ShowLog log_;
  // The table: held, busy (a bot thinking) till when, its next move's
  // time, its messages waiting, and the snapshots on their way
  bool tableHeld_ = true;
  double tableBusyUntil_ = 0, nextMove_ = 0;
  std::size_t playedSent_ = 0;
  struct Message {
    double at;
    std::function<void()> act;
  };
  std::deque<Message> messages_;
  struct OnItsWay {
    double arrives;
    TableSnapshot snapshot;
  };
  std::deque<OnItsWay> snapshots_;
  double nextInput_ = 0;
  int inputs_ = 0;
  bool over_ = false;
  std::function<void(ScriptedGame &)> onFrame_;
};

} // namespace CosmicHex::Handheld
