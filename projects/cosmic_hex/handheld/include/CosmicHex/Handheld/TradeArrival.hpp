// TradeArrival.hpp - an offer to you (or a counter to yours) arriving (the
// user's: it used to just appear on the touch panel): its banner comes in on
// the top screen, and once the banner's words have come in and settled the
// trade screen slides down over the board, in place well before the words
// leave. It reads a time given to it, the clock the banner runs on, so
// Animations Quick speeds it up with the banner, and skip() (B, or
// Animations Off) puts the screen in place at once.
//
//   TradeArrival arrival;
//   if (arrival.saw(room.answering(), now))        // each new look at the game
//     banner.start(now, room.headline(), room.offerWords(), colourOf(room.from()));
//   const float down = arrival.down(now);          // 0: the board still; 1: in place
#pragma once

#include <ScratchPadd/Banner/Banner.hpp>
#include <ScratchPadd/Motion/Motion.hpp>

namespace CosmicHex::Handheld {

class TradeArrival {
public:
  // When the screen starts down after the banner starts (its words in and
  // settled), and how long it takes to come down
  static constexpr double kStartsAfter =
      ScratchPadd::Banner::kWordsInAfter + ScratchPadd::Banner::kWordsIn;
  static constexpr float kSeconds = 0.35f;

  // A new look at the game, at `now`: whether there's an answer to make
  // (TradeRoom::answering). True when one has just arrived: the screen
  // comes down from now on, after the banner the caller starts now
  bool saw(bool answering, double now);
  // How far down the screen has come: 0 not yet (the board still shows), 1
  // in place, eased to a stop; 1 when nothing is arriving
  float down(double now) const;
  // Coming down, or waiting to (what B skips)
  bool moving(double now) const;
  // Its banner waits its turn (a show has the stage): the board still shows
  // until after(), and the screen comes down from there
  void waitForBanner();
  // Its banner starts (or a slide's words come in as though it did) at
  // `bannerAt`: the screen comes down timed from then
  void after(double bannerAt);
  // In place at once (B, Animations Off)
  void skip() { since_.stop(); }

private:
  bool answering_ = false;
  ScratchPadd::Motion::Showing since_; // (the answer's arrival, while it comes in)
};

} // namespace CosmicHex::Handheld
