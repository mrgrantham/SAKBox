// Buttons.hpp - buttons on a touch screen, worked by touch or by the pad,
// drawn on a Canvas: a panel of them, each with a place, words or an icon,
// and whether it can be used now (dimmed when not). A touch finds the button
// under it; the D-pad moves a focus to the nearest usable button that way;
// a used button flashes, with a message if it has something to say (why it
// can't be used, say). With Input's menu roles (Confirm on the pad's own
// button), the same code works on every pad.
//
//   using namespace ScratchPadd;
//   Buttons::Panel panel;
//   panel.set({{.id = kPlay, .area = {78, 60, 100, 26}, .label = "Play"},
//              {.id = kQuit, .area = {78, 96, 100, 26}, .label = "Quit"}});
//   if (auto touched = panel.at(x, y)) choose(*touched);
//   if (edges.pressed(Input::Button::Down)) panel.move(Buttons::Direction::Down);
//   if (edges.pressed(pad.buttonFor(Input::Role::Confirm)) && panel.focus())
//   choose(*panel.focus()); panel.draw(canvas, style);
#pragma once

#include <ScratchPadd/Canvas/Canvas.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::Buttons {

using Colour = Canvas::Colour;

// A rectangle on the screen, in pixels
struct Area {
  float x = 0, y = 0, width = 0, height = 0;
  bool contains(int px, int py) const;
};

struct Button {
  int id = 0;        // the app's own (an enum's value, say)
  Area area;         // where it is
  std::string label; // its words (drawn when it has no icon)
  // A square picture for it (RGBA, iconSize a side), drawn in the middle
  const std::vector<Colour> *icon = nullptr;
  int iconSize = 0;
  bool usable = true; // dimmed, and can't be confirmed, when not
  // Its own face and edge colours, when it should stand out (a Play
  // button's green); otherwise the style's
  std::optional<Colour> face, edge;
};

// How a panel looks
struct Style {
  Colour face{31, 42, 64, 255};
  Colour edge{58, 74, 106, 255};
  Colour words{226, 232, 244, 255};
  Colour focus{255, 214, 90, 255}; // the D-pad's button: a double outline
  Colour flash{255, 214, 90, 255}; // a button just used
  Colour band{10, 15, 26, 255};    // the message's band
  Colour message{255, 214, 90, 255};
  float labelScale = 1;
  float dimmed = 0.35f; // an unusable button's icon, how strongly drawn
};

enum class Direction { Up, Down, Left, Right };

class Panel {
public:
  // The buttons now (the focus stays on the same id, if it's still there)
  void set(std::vector<Button> buttons);
  const std::vector<Button> &buttons() const { return buttons_; }
  const Button *find(int id) const;

  // The button at (x, y), usable or not (a touch on an unusable one can
  // still say why it can't be used)
  std::optional<int> at(int x, int y) const;

  // The D-pad's focus: none at first, then the first usable button
  std::optional<int> focus() const { return focus_; }
  void setFocus(std::optional<int> id);
  // The nearest usable button in `direction` from the focused one (its
  // middle, the closest along that way, then the closest across)
  void move(Direction direction);

  // A button used: it flashes, and `message` shows (if any) in the band
  void flash(int id, std::string message = {});
  void say(std::string message) { message_ = std::move(message); }
  void clearFeedback();
  bool showingFeedback() const { return flashed_.has_value() || !message_.empty(); }
  std::optional<int> flashed() const { return flashed_; }
  const std::string &message() const { return message_; }
  // Where the message goes when there is one (nothing drawn without)
  void setMessageArea(std::optional<Area> area) { messageArea_ = area; }

  // The buttons drawn on `canvas`, the message in its band
  void draw(Canvas &canvas, const Style &style) const;
  // Everything draw() draws on: each button (and the flash's outline round
  // it) and the message's band
  std::vector<Area> areas() const;

private:
  std::vector<Button> buttons_;
  std::optional<int> focus_, flashed_;
  std::string message_;
  std::optional<Area> messageArea_;
};

} // namespace ScratchPadd::Buttons
