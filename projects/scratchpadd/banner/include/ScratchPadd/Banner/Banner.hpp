// Banner.hpp - a moment's message across the middle of a screen (Cosmic
// Hex's whose turn it is: the user's, a band softly in the player's colour
// with edges in the app's accent colour, and moving): the band fades in,
// the words sweep in from the right and ease to a stop in the middle, hold,
// then speed off to the left as the band fades away. draw() draws it on an
// overlay.
//
// A banner can show several messages in turn on the one band, its slides:
// each one's words sweep off as the next one's come in, and the band fades
// only after the last. A slide can wait: it stays until what it asks for is
// done (release()), so a prompt ("ROLL THE DICE") holds on the screen until
// the player acts (the user's). A slide can be added to a banner that's
// still showing (then()): the turn's banner, then the prompt on the same
// band.
//
//   Banner banner(screenWidth);
//   banner.start(now, "YOUR TURN", "", playerColour);          // one slide
//   banner.then(now, {"ROLL THE DICE", "touch Roll", true}, playerColour);
//   banner.release(now);                                       // rolled
//   if (const auto look = banner.at(now))                      // nothing once it's over
//     Banner::draw(overlay, *look, style);
#pragma once

#include <ScratchPadd/Motion/Motion.hpp>
#include <ScratchPadd/Scene/Overlay.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd {

class Banner {
public:
  // How a slide moves (seconds): the band in, the words in, held, off, the
  // band away
  static constexpr double kBandIn = 0.25, kWordsInAfter = 0.1, kWordsIn = 0.45, kHold = 1.3,
                          kWordsOut = 0.35, kBandOutAfter = 0.1, kBandOut = 0.3;

  struct Slide {
    std::string title, note;
    bool waits = false; // (held until release(), rather than for kHold)
  };

  explicit Banner(float screenWidth);

  // A banner of these slides (or one, timed), from `now`
  void start(double now, std::vector<Slide> slides, Scene::Colour colour);
  void start(double now, std::string title, std::string note, Scene::Colour colour);
  // A slide after the ones showing, on the same band (or a new banner if
  // none is, or its band is already going)
  void then(double now, Slide slide, Scene::Colour colour);
  // What a waiting slide waits for is done: it goes on
  void release(double now);
  // Straight on to the slide that waits (B: the timed ones before it
  // skipped), or, with none, gone
  void hurry(double now);
  void stop();

  bool showing(double now) const;
  // Held on a slide that waits (still: what's under it can go on)
  bool waiting(double now) const;
  // Showing and not waiting: what waits for a banner waits for this
  bool moving(double now) const { return showing(now) && !waiting(now); }
  // Seconds until it's gone, or until a waiting slide has come in and
  // settled (0 when either is now)
  double remaining(double now) const;

  struct Look {
    float bandAlpha; // 0 to 1: how strongly the band shows
    float wordsX;    // the words' middle, across the screen
    const std::string &title;
    const std::string &note;
    Scene::Colour colour;
  };
  std::optional<Look> at(double now) const;

  // How long a banner of one timed slide takes, all told
  static float seconds() {
    return static_cast<float>(kWordsInAfter + kWordsIn + kHold +
                              std::max(kWordsOut, kBandOutAfter + kBandOut));
  }

  // How a band looks on a screen
  struct Style {
    float screenWidth = 256, screenHeight = 192;
    float scale = 1;                         // (the screen's width / 256)
    Scene::Colour accent{80, 220, 200, 255}; // (its edges)
    Scene::Colour back{12, 20, 36, 255};     // (under the colour)
    float titleScale = 1.5f;
  };
  // `look` drawn: a band across the screen just above the middle, `look`'s
  // colour over the style's back, edged in its accent; the title and the
  // note centred at look.wordsX
  static void draw(Scene::Overlay &overlay, const Look &look, const Style &style);

private:
  struct Shown {
    Slide slide;
    std::optional<double> leaves; // (its words start off: set by release/hurry, else worked out)
  };
  // When slide `i`'s words come in, and start off (infinity: waiting)
  double entersAt(std::size_t i) const;
  double leavesAt(std::size_t i) const;
  // When the band has gone (infinity while a slide waits)
  double endsAt() const;
  // The slide waiting and not yet released, if any
  std::optional<std::size_t> waitingSlide() const;

  float width_;
  std::optional<double> started_;
  std::vector<Shown> slides_;
  Scene::Colour colour_{};
};

} // namespace ScratchPadd
