// TopShows.hpp - what the top screen shows over the board for a while, each
// on its own: its state, its timing, and whether it's moving (so the
// screen knows to keep drawing: StillFrames). Drawing is apart, in
// TopViews.hpp, so these are tested without a screen.
//
//   DiscoveryShow  tiles turned over, each lifted, turned and set back
//                  (Discovery.hpp's timeline), the game held meanwhile
//   DealtShow      the two cards dealt after buying, thrown in and turned
//   PanelShow      a view sliding down over the board and back up (another
//                  player's)
//   FirstRollShow  who goes first: every player's die thrown, ties again
//
// Times are seconds on one clock the app keeps (real time: these run while
// the game is held). Each answers moving(now); the app's frame asks them all.
#pragma once

#include <CosmicHex/Handheld/Discovery.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>

#include <ScratchPadd/Motion/Motion.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace CosmicHex::Handheld {

// ---------------------------------------------------------------------------
// Tiles turned over, shown one after another

class DiscoveryShow {
public:
  // A move arrived (`before` to `after`): the tiles it turned over queued,
  // the first shown from `startAt` (once the ship's slide is done). A new
  // move also forgets which tiles were shown big last time
  void arrived(const State &before, const State &after, double startAt);

  // Each frame. Before a tile starts, `closeUp` draws its face sharp (it
  // takes a moment on a DS, so the show starts after it, while nothing
  // moves). Says what the app should do now
  struct Step {
    bool hold = false;      // hold the game (the first tile has started)
    bool letGo = false;     // let it go (the last tile is done)
    bool redrawMap = false; // the bottom map's tile is fading in
  };
  Step update(double now, const std::function<void(Art::Face)> &closeUp);

  // The tile showing and where its show is; nothing before it starts
  const Discovery::Found *current() const;
  std::optional<Discovery::Moment> moment(double now) const;
  bool moving(double now) const { return moment(now).has_value(); }
  bool waiting() const { return !queue_.empty(); }

  // For drawing the board: a tile turned over but not back in its place
  // yet (face down there), off the board now (flying: drawn by the show),
  // and shown big since the last move (so it doesn't flip on the board too)
  bool stillToShow(Hex hex, double now) const;
  bool flying(Hex hex, double now) const;
  bool shownBig(Hex hex) const;
  // For the bottom map: the tiles waiting their turn (face down there), and
  // the one showing, fading in once it's back on the board
  std::vector<Hex> waitingAfterCurrent() const;
  struct Revealing {
    Hex hex;
    float shown;
  };
  std::optional<Revealing> revealing(double now) const;

private:
  std::vector<Discovery::Found> queue_;
  std::optional<double> startsAt_;
  bool closeUpDrawn_ = false, held_ = false;
  std::vector<Hex> shownBig_;
};

// ---------------------------------------------------------------------------
// The two cards dealt after buying: thrown in one after the other, then
// turned over one after the other

class DealtShow {
public:
  static constexpr float kSecondsThrow = 0.45f, kSecondsTurn = 0.4f;
  static constexpr float kThrowStagger = 0.15f, kTurnStagger = 0.35f, kTurnAfter = 0.2f;

  // The cards to keep one of (shown till they're gone: stop())
  void deal(const std::vector<Art::Card> &cards, double now);
  void stop() { shown_.stop(); }
  bool on() const { return shown_.on(); }
  bool moving() const { return on(); }
  const std::vector<Art::Card> &cards() const { return cards_; }
  // Before they're thrown: their close-ups drawn (a moment on a DS), then
  // the throw starts over from `now`
  bool needsCloseUp() const { return on() && !closeUpDrawn_; }
  void closeUpDrawn(double now);

  // Card i: how far thrown in (0 to 1, eased) and turned over (0 to 1)
  float thrown(std::size_t i, double now) const;
  float turned(std::size_t i, double now) const;

private:
  ScratchPadd::Motion::Showing shown_;
  std::vector<Art::Card> cards_;
  bool closeUpDrawn_ = false;
};

// ---------------------------------------------------------------------------
// A view sliding down over the board, and back up when closed

class PanelShow {
public:
  explicit PanelShow(float seconds) : seconds_(seconds) {}
  void open(int which, double now);
  void choose(int which) { which_ = which; } // (another, while it's open)
  void close(double now);
  // Shown: open, or still sliding back up
  bool showing(double now) const;
  bool moving(double now) const { return showing(now); }
  // How far down it is (0 above the screen, 1 in place), eased
  float down(double now) const;
  int which() const { return which_; }

private:
  float seconds_;
  int which_ = -1;
  ScratchPadd::Motion::Showing opened_, closing_;
};

// ---------------------------------------------------------------------------
// Who goes first: every player's die thrown at once, tumbling and settling;
// those tied throw again on the next row; then the order

class FirstRollShow {
public:
  static constexpr float kSecondsRound = 1.4f, kSecondsTumble = 0.9f;
  static constexpr float kSecondsRead = 2.6f;
  static constexpr std::size_t kRoundsShown = 3;

  void start(double now) { shown_.start(now); }
  void stop() { shown_.stop(); }
  bool on() const { return shown_.on(); }
  bool moving() const { return on(); }
  float seconds(double now) const { return static_cast<float>(shown_.seconds(now)); }
  // How long it shows for `roll`; whether that's up
  static float duration(const FirstRoll &roll);
  bool done(double now, const FirstRoll &roll) const {
    return on() && seconds(now) >= duration(roll);
  }
  // Round r's dice: how far through their tumble (0 to 1; nothing before
  // they're thrown); whether the last round has settled (the order shows)
  static std::optional<float> tumble(std::size_t round, float seconds);
  static bool settled(const FirstRoll &roll, float seconds);

private:
  ScratchPadd::Motion::Showing shown_;
};

} // namespace CosmicHex::Handheld
