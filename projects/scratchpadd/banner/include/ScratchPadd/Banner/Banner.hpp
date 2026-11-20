// Banner.hpp - a moment's message across the middle of a screen (Cosmic
// Hex's whose turn it is: the user's, a band softly in the player's colour
// with edges in the app's accent colour, and moving): the band fades in,
// the words sweep in from the right and ease to a stop in the middle, hold,
// then speed off to the left as the band fades away. Motion's curves and a
// script time it; draw() draws it on an overlay.
//
//   Banner banner(screenWidth);
//   banner.start(now, "YOUR TURN", "roll the dice", playerColour);
//   if (const auto look = banner.at(now))  // nothing once it's over
//     Banner::draw(overlay, *look, style);
#pragma once

#include <ScratchPadd/Motion/Motion.hpp>
#include <ScratchPadd/Scene/Overlay.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <optional>
#include <string>

namespace ScratchPadd {

class Banner {
public:
  explicit Banner(float screenWidth);

  void start(double now, std::string title, std::string note, Scene::Colour colour);
  void stop() { started_.reset(); }
  bool showing(double now) const;

  struct Look {
    float bandAlpha; // 0 to 1: how strongly the band shows
    float wordsX;    // the words' middle, across the screen
    const std::string &title;
    const std::string &note;
    Scene::Colour colour;
  };
  std::optional<Look> at(double now) const;

  // How long it takes, all told
  float seconds() const { return script_.seconds(); }

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
  float width_;
  Motion::Script script_;
  Motion::Script::Step fadeIn_, sweepIn_, hold_, sweepOut_, fadeOut_;
  std::optional<double> started_;
  std::string title_, note_;
  Scene::Colour colour_{};
};

} // namespace ScratchPadd
