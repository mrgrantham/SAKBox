// TurnBanner.hpp - whose turn it is, across the top screen as each turn
// starts (round 101, the user's: B, a band softly in the player's colour
// with turquoise edges, and moving): the band fades in, the words sweep in
// from the right and ease to a stop in the middle, hold, then speed off to
// the left as the band fades away. Motion's curves and a script time it.
//
//   TurnBanner banner(screenWidth);
//   banner.start(now, "YOUR TURN", "roll the dice", playerColour);
//   const auto look = banner.at(now);      // nothing once it's over
//   if (look) draw a band look->bandAlpha strong, the words at look->wordsX
#pragma once

#include <ScratchPadd/Motion/Motion.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <optional>
#include <string>

namespace CosmicHex::Handheld {

class TurnBanner {
public:
  explicit TurnBanner(float screenWidth);

  void start(double now, std::string title, std::string note, ScratchPadd::Scene::Colour colour);
  void stop() { started_.reset(); }
  bool showing(double now) const;

  struct Look {
    float bandAlpha; // 0 to 1: how strongly the band shows
    float wordsX;    // the words' middle, across the screen
    const std::string &title;
    const std::string &note;
    ScratchPadd::Scene::Colour colour;
  };
  std::optional<Look> at(double now) const;

  // How long it takes, all told
  float seconds() const { return script_.seconds(); }

private:
  float width_;
  ScratchPadd::Motion::Script script_;
  ScratchPadd::Motion::Script::Step fadeIn_, sweepIn_, hold_, sweepOut_, fadeOut_;
  std::optional<double> started_;
  std::string title_, note_;
  ScratchPadd::Scene::Colour colour_{};
};

} // namespace CosmicHex::Handheld
