// ButtonMenu.hpp - a column of buttons on the touch screen, worked by touch
// or by the buttons: the title's Play and Quit, and the pause
// menu's Resume, Restart, Title and Quit. Up and down move between them, A
// chooses, a touch on one chooses it. MenuPicture draws it.
//
//   ButtonMenu pause({"Resume", "Restart", "Title", "Quit"});
//   if (auto chosen = pause.press(ButtonMenu::Key::Confirm)) ... // its index
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

class ButtonMenu {
public:
  enum class Key : std::uint8_t { Up, Down, Confirm };

  explicit ButtonMenu(std::vector<std::string> labels) : labels_(std::move(labels)) {}

  const std::vector<std::string> &labels() const { return labels_; }
  std::size_t focus() const { return focus_; }
  void setFocus(std::size_t focus) { focus_ = focus < labels_.size() ? focus : 0; }

  // A button pressed: the one chosen (A), or nothing
  std::optional<std::size_t> press(Key key);

  // Where the buttons are on a screen this size: a column in the middle
  struct Button {
    float x, y, width, height;
  };
  std::vector<Button> buttons(int width, int height) const;
  // A touch at (x, y): the button touched, chosen
  std::optional<std::size_t> touch(int x, int y, int width, int height);

private:
  std::vector<std::string> labels_;
  std::size_t focus_ = 0;
};

} // namespace CosmicHex::Handheld
