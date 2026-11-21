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

// How long words take to fade in, once what they're over has settled (the
// user's: never on something still moving, and never just appearing)
inline constexpr float kSecondsWordsIn = 0.25f;

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
  // Card i's words fading in once it's turned over (0 to 1)
  float words(std::size_t i, double now) const;

private:
  ScratchPadd::Motion::Showing shown_;
  std::vector<Art::Card> cards_;
  bool closeUpDrawn_ = false;
};

// ---------------------------------------------------------------------------
// A view sliding down over the board, and back up when closed; another
// chosen while it's open, the last one lifts away and the next comes down
// (the user's: other players' views change with a movement, not a cut)

class PanelShow {
public:
  static constexpr float kSecondsSwitch = 0.3f;

  explicit PanelShow(float seconds) : seconds_(seconds) {}
  void open(int which, double now);
  void choose(int which, double now); // (another, while it's open)
  void close(double now);
  // Which to draw now and how far down (0 above the screen, 1 in place):
  // the one chosen, or the last, lifting away as another's chosen
  struct Drawn {
    int which;
    float down;
  };
  Drawn drawn(double now) const;
  // Shown: open, or still sliding back up
  bool showing(double now) const;
  bool moving(double now) const { return showing(now); }
  // How far down it is (0 above the screen, 1 in place), eased
  float down(double now) const;
  // Whether it's come all the way down and settled (covering the screen)
  bool inPlace(double now) const;
  int which() const { return which_; }

private:
  float seconds_;
  int which_ = -1, last_ = -1;
  ScratchPadd::Motion::Showing opened_, closing_, switched_;
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
  static constexpr float kSecondsRead = 3.2f;       // (the order: sorted, then read)
  static constexpr float kSecondsSort = 0.7f;       // (a player moving into order)
  static constexpr float kSortStagger = 0.12f;      // (each after the one before)
  static constexpr float kSecondsIn = 0.6f;         // (fading in over the board, first)

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

  // For drawing: this round's glass (die p, in lane p, player p's), who
  // throws this round, each player's numbers so far and when each came
  // (for the odometer), and whether the order's known
  const Dice::DiceInTurn *dice() const { return dice_.get(); }
  const std::vector<int> &throwers() const { return throwers_; }
  int round() const { return round_; }
  // Player p's settled number in round r (0: none yet, or not thrown)
  int shown(std::size_t round, int player) const;
  double shownAt(std::size_t round, int player) const;
  static constexpr double kDieSize = 0.85; // (the roll's are 1.2)
  bool allLanded() const { return finishedAt_.has_value(); }
  // How far the players have moved into their order once all have landed
  // (0 in their lanes, 1 in order: 1st on the left), after a moment: each
  // in turn (`order`: 0 the first to move), straight through time (the
  // view gives it its curve)
  float sorted(double now, int order = 0) const;
  // How far it's faded in over the board (0 to 1), the first die thrown
  // once it has (the user's: it popped up abruptly)
  float appeared(double now) const;
  // ...and its place (1st, 2nd...) fading in once it's there
  float sortedWords(double now, int order) const;

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
  std::vector<std::vector<double>> shownAt_;
  std::unique_ptr<Dice::DiceInTurn> dice_;
  std::optional<double> landedAt_, finishedAt_;
  double startedAt_ = 0;
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
    float words;           // (the card's words fading in once it's full size)
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
  // flying from the victim to the thief; each trade, the cards crossing
  // between the two players (or to and from the bank); and what's built,
  // its cost going to the bank (behind the ROUND hexagon)
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

// ---------------------------------------------------------------------------
// An action card played, or a Charter revealed: the card rises from its
// player (your hand, or their row of the points bars) face down, turning
// over as it comes, held big in the middle to be read, then off up and
// away (to the discards); one after another, the game held meanwhile

class CardPlayShow {
public:
  static constexpr float kSecondsUp = 0.5f, kSecondsHold = 1.4f, kSecondsAway = 0.4f;
  static constexpr float kSeconds = kSecondsUp + kSecondsHold + kSecondsAway;

  struct Play {
    int player = 0;
    Art::Card card{};
  };
  // The moves made since the last look: each card played or Charter revealed
  void saw(const std::vector<TableGame::Played> &moves, double now);

  const Play *current() const { return queue_.empty() ? nullptr : &queue_.front(); }
  // The current one's moment: risen (0 to 1), turned over (0 to 1), gone
  // (0 to 1); nothing when none shows
  struct Moment {
    float up, turned, away;
    float words; // (the card's words fading in once it's turned over)
  };
  std::optional<Moment> at(double now) const;
  bool moving(double now) const { return at(now).has_value(); }

  struct Step {
    bool hold = false, letGo = false;
  };
  Step update(double now);

private:
  std::vector<Play> queue_;
  std::optional<double> startedAt_;
  bool held_ = false;
};

// ---------------------------------------------------------------------------
// Choosing a resource (the user's): the card in focus on the picker shown
// big, the next sliding in as you move to it and the last sliding out, from
// the side you moved towards

class PickShow {
public:
  static constexpr float kSecondsSlide = 0.3f;

  // Each frame: the card in focus now (nothing: no choice open)
  void show(std::optional<Resource> resource, double now);

  struct Moment {
    Resource shown;
    std::optional<Resource> leaving;
    float in;      // (0 just arrived at the side, 1 in the middle)
    float words;   // (its name fading in once it's in the middle)
    int direction; // (1: moving right, so in from the right)
  };
  std::optional<Moment> at(double now) const;
  bool moving(double now) const;

private:
  std::optional<Resource> shown_, leaving_;
  double since_ = 0;
  int direction_ = 1;
};

// ---------------------------------------------------------------------------
// Cards gained (the user's): your hand shown as Show resources shows it, the
// piles rising into view, each new card sliding down from above into the
// back of its pile, one after another; a moment to see them, then the piles
// going back down

class HandShow {
public:
  static constexpr float kSecondsPiles = 0.3f; // (the piles in, and out)
  static constexpr float kSecondsCard = 0.35f, kStagger = 0.12f;
  static constexpr float kSecondsHold = 0.6f;

  // Cards gained: queued, shown from `start` (when the top screen is free)
  void gained(const Resources &added);
  void start(double now);
  bool waiting() const;
  bool showing(double now) const;
  bool moving(double now) const { return showing(now); }

  // How far the piles are up (0 below the screen, 1 in place)
  float piles(double now) const;
  // How far gained card `index` of resource `r` has slid in (0 above the
  // screen, 1 in its pile); 1 for cards held before
  float card(Resource r, int index, double now) const;
  const Resources &added() const { return added_; }

private:
  float seconds() const; // (all told)
  Resources added_{}, queued_{};
  std::optional<double> startedAt_;
};

// ---------------------------------------------------------------------------
// The board's pointer (the user's): a chevron over the corner in focus,
// fading in gently when the game wants you to choose on the board (where to
// place, build or move a ship) and away while pieces move, things are built
// and turns end; gliding from corner to corner as the focus moves

class CornerPointer {
public:
  static constexpr float kSecondsFade = 0.35f, kSecondsGlide = 0.25f;

  // Each frame: the corner it should point at now, or nothing (fading away)
  void aim(std::optional<Corner> corner, double now);

  struct Look {
    Corner from, to; // (gliding from one to the other)
    float along;     // 0 at `from`, 1 at `to`, eased
    float shown;     // 0 gone, 1 fully in, eased
  };
  // Nothing once it's faded away (or before it's first aimed)
  std::optional<Look> at(double now) const;
  bool moving(double now) const;

private:
  float shownAt(double now) const;

  std::optional<Corner> from_, to_;
  bool on_ = false;
  double fadedAt_ = 0, glidedAt_ = 0;
  float fadeFrom_ = 0; // (how shown it was when the fade turned)
};

} // namespace CosmicHex::Handheld
