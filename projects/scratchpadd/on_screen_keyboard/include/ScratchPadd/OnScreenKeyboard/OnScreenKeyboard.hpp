// OnScreenKeyboard.hpp - typing a short text on a touch screen, or with a pad: a
// field above a QWERTY keyboard, drawn on a Canvas with the Buttons
// library (so a touch finds its key, the D-pad moves between keys, and a
// press glows). For a name, a save's title, a high score's initials.
//
//   OnScreenKeyboard keyboard("James", 10);           // its text so far, its most
//   keyboard.touch(x, y, now) / keyboard.press(OnScreenKeyboard::Key::Confirm, now)
//   keyboard.draw(canvas, style, now);
//   if (keyboard.outcome() == OnScreenKeyboard::Outcome::Done) use keyboard.text()
//
// The keys: the letters, Shift (the next letter in capitals; the first
// letter is a capital by itself), Space, Del and Done. On a pad: the D-pad
// moves, Confirm types the focused key, Back deletes, Done (Start) finishes.
#pragma once

#include <ScratchPadd/Buttons/Buttons.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>

#include <cstddef>
#include <string>

namespace ScratchPadd {

class OnScreenKeyboard {
public:
  explicit OnScreenKeyboard(std::string text = {}, std::size_t most = 10);

  // Where it goes: `width` x `height` pixels (sized for a DS's 256 x 192,
  // scaled to others)
  void layOut(int width, int height);

  enum class Key { Up, Down, Left, Right, Confirm, Back, Done };
  void press(Key key, double now);
  void touch(int x, int y, double now);

  enum class Outcome { Typing, Done };
  Outcome outcome() const { return outcome_; }
  const std::string &text() const { return text_; }
  bool shifted() const { return shift_; }

  struct Style {
    Canvas::Colour background{18, 24, 38, 255};
    Canvas::Colour field{64, 220, 200, 255}; // (its outline)
    Canvas::Colour words{226, 232, 244, 255};
    Buttons::Style keys;
  };
  void draw(Canvas &canvas, const Style &style, double now);
  // Whether a key's press is still glowing (draw again meanwhile)
  bool pressing(double now) const { return panel_.pressing(now); }

private:
  // The keys' ids: a letter is its character; the others below
  enum Special : int { KeyShift = 1, KeySpace, KeyDelete, KeyDone };
  void type(int id);
  void setKeys();

  std::string text_;
  std::size_t most_;
  bool shift_ = false;
  Outcome outcome_ = Outcome::Typing;
  int width_ = 256, height_ = 192;
  Buttons::Panel panel_;
};

} // namespace ScratchPadd
