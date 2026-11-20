// SettingsScreen.hpp - the settings, on the bottom screen from the title:
// your name (your console's owner name at first, the user's), Change to
// type another on a keyboard (ScratchPadd::OnScreenKeyboard), and Back.
//
//   SettingsScreen settings(256, 192);
//   settings.open("James", true);              // the name, and whether it's the console's
//   settings.touch(x, y, now) / settings.press(Key::Confirm, now)
//   if (outcome == Outcome::NameChanged) save settings.name()
//   settings.draw(canvas, now);
//
// Drawing and touches only: the app keeps the name (on the card).
#pragma once

#include <ScratchPadd/Buttons/Buttons.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>
#include <ScratchPadd/OnScreenKeyboard/OnScreenKeyboard.hpp>

#include <optional>
#include <string>

namespace CosmicHex::Handheld {

class SettingsScreen {
public:
  SettingsScreen(int width, int height);

  // Shown, with `name`, which is the console's own when `fromConsole`
  void open(std::string name, bool fromConsole);
  const std::string &name() const { return name_; }
  bool typing() const { return keyboard_.has_value(); }

  using Key = ScratchPadd::OnScreenKeyboard::Key;
  enum class Outcome { None, Back, NameChanged };
  Outcome press(Key key, double now);
  Outcome touch(int x, int y, double now);

  void draw(ScratchPadd::Canvas &canvas, double now);
  // Whether something on it is still moving (a button's press glow)
  bool pressing(double now) const;

  static constexpr std::size_t kLongestName = 10; // (a DS's own allows 10)

private:
  enum Id : int { kChange = 1, kBack };
  void setButtons();
  Outcome typed(); // (after a key on the keyboard: done typing?)

  int width_, height_;
  std::string name_;
  bool fromConsole_ = false;
  ScratchPadd::Buttons::Panel buttons_;
  std::optional<ScratchPadd::OnScreenKeyboard> keyboard_;
};

} // namespace CosmicHex::Handheld
