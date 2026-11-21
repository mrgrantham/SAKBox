// Buttons.hpp - buttons on a touch screen, worked by touch or by the pad,
// drawn on a Canvas: a panel of them, each with a place, words or an icon,
// and whether it can be used now (dimmed when not). A touch finds the button
// under it; the D-pad moves a focus to the nearest usable button that way;
// a used button flashes, with a message if it has something to say (why it
// can't be used, say). Each is drawn raised, lit from the top left, and
// sinks a pixel when pressed (Style::bevel). With Input's menu roles
// (Confirm on the pad's own button), the same code works on every pad.
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
  // A toggle that's on (Show resources open, say): outlined in the style's
  // `on` colour for as long as it is. The highlight means chosen or on,
  // never just touched (the user's)
  bool on = false;
  // Its own face and edge colours, when it should stand out (a Play
  // button's green); otherwise the style's
  std::optional<Colour> face, edge;
  // How much of it shows (0 to 1), for a button fading in or out: drawn
  // see-through over what's under it. One fading in (usable) answers a
  // touch at once; one fading out (not usable) lets it pass
  float shown = 1;
};

// How a button stands out from what's under it
enum class Bevel {
  Flat,   // a face with a one-pixel edge all round
  Raised, // a face lit along its top and left, shaded along its bottom and
          // right, with a dark drop under it; pressed, it sinks a pixel
};

// How a panel looks
struct Style {
  Colour face{31, 42, 64, 255};
  Colour edge{58, 74, 106, 255};
  Colour words{226, 232, 244, 255};
  Colour focus{255, 214, 90, 255}; // the D-pad's button: a double outline
  Colour on{255, 214, 90, 255};    // a toggle that's on (Button::on)
  // A press: a see-through white over the button's inside, in quickly and
  // out (just to show the touch was taken and nothing froze)
  Colour press{255, 255, 255, 255};
  float pressOpacity = 0.35f;
  Colour band{10, 15, 26, 255}; // the message's band
  Colour message{255, 214, 90, 255};
  float labelScale = 1;
  float dimmed = 0.35f; // an unusable button's icon, how strongly drawn
  // Raised (the default): the top and left edge a little lighter than the
  // edge colour (a button's own edge too: a player's colour, the chosen
  // tab's amber), the bottom and right the face darkened, and `dropPixels`
  // of `drop` under the face. All of it stays inside the button's area, in
  // whole pixels, so touches and areas() are the same either way
  Bevel bevel = Bevel::Raised;
  Colour drop{6, 9, 16, 255};
  int dropPixels = 2;
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

  // A button pressed at `now` (seconds, on the caller's clock): it glows
  // for kPressSeconds, and `message` shows (if any) in the band until
  // cleared
  static constexpr double kPressSeconds = 0.25;
  void press(int id, double now, std::string message = {});
  // How strongly the press glows at `now` (0 to 1: in over the first fifth,
  // out over the rest), and whether it still does: redraw while it does
  float pressGlow(double now) const;
  bool pressing(double now) const { return pressGlow(now) > 0; }
  std::optional<int> pressed() const { return pressed_; }
  // The time draw() shows the press at
  void setTime(double now) { now_ = now; }
  void say(std::string message) { message_ = std::move(message); }
  void clearFeedback();
  bool showingFeedback() const { return !message_.empty(); }
  const std::string &message() const { return message_; }
  // Where the message goes when there is one (nothing drawn without)
  void setMessageArea(std::optional<Area> area) { messageArea_ = area; }

  // The buttons drawn on `canvas`, the message in its band
  void draw(Canvas &canvas, const Style &style) const;
  // (draw's work for a button part shown: Button::shown under 1)
  static void drawFading(Canvas &canvas, const Style &style, const Button &button);
  // Everything draw() draws on: each button (and the flash's outline round
  // it) and the message's band
  std::vector<Area> areas() const;
  // Where a button's face is drawn, inside its area: all of it when flat;
  // raised, above its drop, a pixel lower while it's pressed (`sunk`)
  static Area faceOf(const Button &button, const Style &style, bool sunk);

private:
  // Whether a button shows pressed now: sunk, its glow over it
  bool sunk(const Button &button) const;
  void drawMessage(Canvas &canvas, const Style &style) const; // (the band, if any)
  std::vector<Button> buttons_;
  std::optional<int> focus_, pressed_;
  double pressedAt_ = 0, now_ = 0;
  std::string message_;
  std::optional<Area> messageArea_;
};

} // namespace ScratchPadd::Buttons
