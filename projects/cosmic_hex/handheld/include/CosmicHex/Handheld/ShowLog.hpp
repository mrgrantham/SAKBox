// ShowLog.hpp - every show over the board a game brought, as a run saw it:
// when each started, how long it moved and how much of that was seen, and
// whether it was cut short and why. GameShows tells it, each frame, how
// each show stands (ShowSeen), and when something cuts shows short (B, the
// Animations setting, a new show in an old one's place); the scripted
// player's runs (ScriptedPlay) read it to find the shows that were cut
// short or skipped without the player asking.
//
//   ShowLog log;
//   shows.setLog(&log);                          // (GameShows)
//   ... a game played ...
//   for (const ShowLog::Entry &entry : log.entries())
//     if (entry.cut() != ShowLog::Cut::None) ...
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace CosmicHex::Handheld {

// The shows, each a kind of thing shown over the board for a while
enum class ShowKind : std::uint8_t {
  Banner,      // the turn's, the placing's, a prompt's
  Dice,        // the roll's physics dice, thrown to pulled away
  Pieces,      // the board's pieces moving (BoardMotion)
  Discovery,   // a tile turned over, shown big
  HomePay,     // a home's pay on its die
  CardFlights, // cards flying: a roll's pay, the starting hands, steals, trades, spending
  CardPlay,    // an action card played, a Charter revealed
  Dealt,       // the two cards dealt after buying
  FirstRoll,   // who goes first
  Hand,        // your cards gained, dropping into your hand
  Celebration  // the winner's ships flying round
};
inline constexpr std::size_t kShowKinds = 11;
std::string_view nameOf(ShowKind kind);

// How a show stands this frame (GameShows::seen)
struct ShowSeen {
  ShowKind kind{};
  bool on = false;      // started or waiting its turn (queued)
  bool moving = false;  // moving now
  bool visible = false; // moving and drawn (not under another show)
  double key = 0;       // which one: when it started, on its clock (a new key, a new show)
  double elapsed = 0;   // seconds since it started, on its clock
  double rate = 1;      // its clock's seconds a real second (Quick: 2)
};

class ShowLog {
public:
  // `keepEntries`: each show kept (a run on the Mac); else only the counts
  // (a console's memory)
  explicit ShowLog(bool keepEntries = true) : keepEntries_(keepEntries) {}

  // Why a show was cut short, or not seen whole (None: it played)
  enum class Cut : std::uint8_t {
    None,
    SkipPressed,   // B took it to its end (the player asked)
    AnimationsOff, // the setting has every show at its end (the player asked)
    SkippedQueued, // B, for another show, took this one waiting its turn too
    Replaced,      // a new show of its kind started over it
    Covered,       // another show was drawn in its place for part of it
    LateStart,     // first seen part way through (its start already past)
    NeverSeen      // started and done without a frame drawn of it
  };
  static std::string_view nameOf(Cut cut);
  // Whether the player asked for it (B, the Animations setting)
  static bool asked(Cut cut) { return cut == Cut::SkipPressed || cut == Cut::AnimationsOff; }

  struct Entry {
    ShowKind kind{};
    double startedAt = 0, endedAt = 0; // (real seconds: the run's clock)
    double movingSeconds = 0, seenSeconds = 0;
    double lateBy = 0; // (seconds into it when first seen moving; 0 when seen from its start)
    bool skipPressed = false, animationsOff = false, skippedQueued = false, replaced = false;
    bool everMoved = false;
    ShowKind coveredBy{}; // (what was drawn in its place, the first time)
    bool covered = false;
    std::string note; // (what the game was doing when it started: GameShows' context)
    // The first reason it wasn't seen whole, the player's own first
    Cut cut() const;
  };

  // Each frame, at `now` (real seconds), `step` since the last: how each
  // show stands, and which show is drawn over the board (for what covered
  // one that wasn't)
  void frame(double now, double step, std::span<const ShowSeen> shows, ShowKind onTop);
  // B pressed, or the Animations setting, taking every show to its end:
  // those moving are skipped; those waiting are skipped before they began
  void skipped(bool pressed);
  // A new show of `kind` starting over the one moving
  void replaced(ShowKind kind);
  // What the game's doing, noted on shows starting from now (the snapshot's
  // moves, say)
  void setContext(std::string context) { context_ = std::move(context); }
  // Every show still open, closed (the run's end)
  void finish(double now);

  const std::vector<Entry> &entries() const { return entries_; }
  // Shows of each kind ended, and cut short for each reason
  std::size_t count(ShowKind kind, Cut cut) const;
  std::size_t count(Cut cut) const;
  std::size_t shows() const;
  // A table of the counts, kind by reason (for a run's report)
  std::string summary() const;

private:
  static constexpr std::size_t kCuts = 8;
  struct Open {
    bool on = false;
    Entry entry;
    double key = 0;
  };
  void close(std::size_t kind, double now);

  bool keepEntries_;
  std::array<Open, kShowKinds> open_{};
  std::vector<Entry> entries_;
  std::array<std::array<std::size_t, kCuts>, kShowKinds> counts_{};
  std::string context_;
  double now_ = 0;
  bool skippedSinceFrame_ = false; // (B since the last frame)
};

} // namespace CosmicHex::Handheld
