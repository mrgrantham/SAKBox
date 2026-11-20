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
//   CardsShow      Show resources: your cards coming in one after another,
//                  and going back off the same way
//   FirstRollShow  who goes first: every player's die thrown, ties again
//   HomePayShow    a home's pay on the home die: the die out of the home,
//                  its face, the card big, then off to the player's hand
//   RollPayShow    what a roll pays: a card from each paying planet to its
//                  owner, one a little after another
//
// Times are seconds on one clock the app keeps (real time: these run while
// the game is held). Each answers moving(now); the app's frame asks them all.
#pragma once

#include <CosmicHex/Dice/DiceInTurn.hpp>
#include <CosmicHex/Handheld/Discovery.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>

#include <ScratchPadd/Motion/Motion.hpp>

#include <functional>
#include <memory>
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
// Show resources: your cards easing in one after another (the piles from
// below, the action cards from above), and back off the same way

class CardsShow {
public:
  static constexpr float kSecondsCard = 0.25f, kStagger = 0.05f;
  static constexpr int kMostCards = 5 * 5 + 12; // (five piles of five, twelve cards)

  void open(double now);
  void close(double now);
  // Shown: open, or its cards still going off
  bool showing(double now) const;
  bool moving(double now) const { return showing(now); }
  // How far card `order` has come in (0 off the screen, 1 in place)
  float arrived(int order, double now) const;

private:
  ScratchPadd::Motion::Showing opened_, closing_;
};

// ---------------------------------------------------------------------------
// Who goes first (the user's): each player's die thrown into the glass in
// turn by the physics (Dice::DiceInTurn), settling on the number the game
// rolled for them, then the next; those tied throw again, in a new glass;
// then the order. Impatient (A): the next throw at once, the moving die set
// down where it lands; once all have landed, on to the game

class FirstRollShow {
public:
  static constexpr float kSecondsBetween = 0.5f;    // (after a die settles)
  static constexpr float kSecondsBetweenRounds = 1; // (the ties' new glass)
  static constexpr float kSecondsRead = 2.4f;       // (the order, read)

  // Started for `roll`, the throws from `seed`, the physics stepped
  // `stepSeconds` at a time (PlatformProfile::diceStep)
  void start(const FirstRoll &roll, std::uint64_t seed, double stepSeconds, double now);
  void stop();
  bool on() const { return on_; }
  bool moving() const { return on_; }
  // Each frame: the physics stepped to `now`, the next die thrown when it's due
  void update(double now);
  // A: the next die thrown now; once all have landed, done
  void hurry(double now);
  bool done(double now) const;

  // For drawing: this round's glass and whose die is which (its index the
  // die's), each player's numbers so far, and whether the order's known
  const Dice::DiceInTurn *dice() const { return dice_.get(); }
  const std::vector<int> &throwers() const { return throwers_; }
  int round() const { return round_; }
  // Player p's settled number in round r (0: none yet, or not thrown)
  int shown(std::size_t round, int player) const;
  bool allLanded() const { return finishedAt_.has_value(); }

private:
  void beginRound(std::size_t round, double now);
  void throwNext(double now);

  FirstRoll roll_;
  std::uint64_t seed_ = 0;
  double step_ = 1.0 / 60, simulated_ = 0;
  bool on_ = false;
  std::size_t round_ = 0, next_ = 0;
  std::vector<int> throwers_; // (this round's: die i is throwers_[i]'s)
  std::vector<std::vector<int>> shown_;
  std::unique_ptr<Dice::DiceInTurn> dice_;
  std::optional<double> landedAt_, finishedAt_;
};

// ---------------------------------------------------------------------------
// A home's pay, when the home die is on (Settings::homeDie): its die springs
// out of the home (a wind-up, then up along an arc), tumbles, settles on a
// face (a resource, or the star: the player's choice); the resource's card
// grows out of it, big, with what it is; then flies to the player's hand.
// Grounded in docs/research/animation-whimsy.md (anticipation, arcs,
// follow-through, timing)

class HomePayShow {
public:
  // The parts, in seconds
  static constexpr float kSecondsUp = 0.55f;     // out of the home, along an arc
  static constexpr float kSecondsTumble = 0.8f;  // turning, faces flickering
  static constexpr float kSecondsSettle = 0.35f; // landing on its face (Settle)
  static constexpr float kSecondsCard = 0.4f;    // the card growing out of the die
  static constexpr float kSecondsHold = 1.1f;    // read it
  static constexpr float kSecondsAway = 0.5f;    // the card off to the hand
  static constexpr float kDieSeconds = kSecondsUp + kSecondsTumble + kSecondsSettle;
  static constexpr float kSeconds = kDieSeconds + kSecondsCard + kSecondsHold + kSecondsAway;

  // A pay: whose home, the die's face (0 to kResources - 1 a resource,
  // kResources the star), and the resource once known (the star's: when
  // the player has chosen)
  struct Pay {
    int player = 0;
    int face = 0;
    std::optional<Resource> resource;
  };
  // The moves made since the last look (TableGame::played): home die rolls
  // queued, a choice after the star filling it in
  void saw(const std::vector<TableGame::Played> &moves, double now);

  // Each frame: what the app should do (hold the game while one shows,
  // let it go after the last)
  struct Step {
    bool hold = false, letGo = false;
  };
  Step update(double now);

  const Pay *current() const { return queue_.empty() ? nullptr : &queue_.front(); }
  // Where the show is: seconds since it started; nothing when none shows
  std::optional<float> seconds(double now) const;
  bool moving(double now) const { return seconds(now).has_value(); }

  // The parts' progress at `seconds`, each 0 to 1
  struct Moment {
    float up, tumble, settle, card, away;
    bool waitingForChoice; // (the star, its resource not chosen yet: held)
  };
  Moment at(float seconds) const;

private:
  std::vector<Pay> queue_;
  std::optional<double> startedAt_;
  bool held_ = false;
};

// ---------------------------------------------------------------------------
// What a roll pays (payouts): a card for each resource, popping out of its
// planet (a settle), then flying along an arc to its owner (your hand at
// the bottom; a bot's row of the points bars), each a little after the
// last (overlap: docs/research/animation-whimsy.md)

class RollPayShow {
public:
  static constexpr float kSecondsPop = 0.2f, kSecondsFly = 0.6f, kStagger = 0.12f;
  struct Card {
    int planet; // (its index in State::planets; -1 for one stolen)
    int player;
    Resource resource;
    int from = -1;       // (stolen or traded: from this player, or the bank)
    bool secret = false; // (stolen: seen only by the two players)
  };
  static constexpr int kBank = -2; // (a card's `from` or `player`: the bank)
  // The moves made since the last look: a roll's pay queued (from `before`,
  // the game as it was when the dice were thrown), to start once `start`
  // is called (when the dice have gone: the user's, not over the roll); and
  // each steal (after the blockade moves, or the pirate hideout's), a card
  // flying from the victim to the thief; and each trade, the cards crossing
  // between the two players (or to and from the bank)
  void saw(const std::vector<TableGame::Played> &moves, const State &before);
  void start(double now);
  bool waiting() const { return !cards_.empty() && !started_; }
  const std::vector<Card> &cards() const { return cards_; }

  // Card i's moment: how far it's popped out (0 to 1, Settle) and how far
  // it's flown (0 to 1, Gentle); nothing before it starts or after it lands
  struct Moment {
    float pop, fly;
  };
  std::optional<Moment> at(std::size_t i, double now) const;
  bool moving(double now) const;
  float seconds() const; // (all told)

  struct Step {
    bool hold = false, letGo = false;
  };
  Step update(double now);

private:
  std::vector<Card> cards_;
  std::vector<int> victims_;                // (who the next steals are from, in order)
  TradeOffer offer_;                        // (the offer standing, as the moves make it)
  int counterer_ = -1;                      // (who made the counter standing)
  void cross(int from, int to, int bundle); // (a bundle's cards, from one to the other)
  double startAt_ = 0;
  bool started_ = false, held_ = false;
};

} // namespace CosmicHex::Handheld
