// SettingsScreen.hpp - the settings, on the bottom screen from the title:
// your name (your console's owner name at first, the user's), Change to
// type another on a keyboard (ScratchPadd::OnScreenKeyboard); how much the
// game animates (Animations: Full, Quick or Off, the user's: for playing
// through quickly); and Back.
//
//   SettingsScreen settings(256, 192);
//   settings.open("James", true, Animations::Full); // the name, whether it's the console's
//   settings.touch(x, y, now) / settings.press(Key::Confirm, now)
//   if (outcome == Outcome::NameChanged) save settings.name()
//   if (outcome == Outcome::AnimationsChanged) save settings.animations()
//   settings.draw(canvas, now);
//
// Drawing and touches only: the app keeps the settings (on the card).
#pragma once

#include <ScratchPadd/Buttons/Buttons.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>
#include <ScratchPadd/OnScreenKeyboard/OnScreenKeyboard.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace CosmicHex::Handheld {

// How much the game animates: in full; quickly (twice as fast); or off
// (each animation straight to its end, as B skips one)
enum class Animations { Full, Quick, Off };
// Its word (on the screen, and on the card), and back
std::string_view wordOf(Animations animations);
std::optional<Animations> animationsFrom(std::string_view word);
// How fast animations run against real time
double rateOf(Animations animations);

class SettingsScreen {
public:
  SettingsScreen(int width, int height);

  // Shown, with `name`, which is the console's own when `fromConsole`
  void open(std::string name, bool fromConsole, Animations animations);
  const std::string &name() const { return name_; }
  Animations animations() const { return animations_; }
  bool typing() const { return keyboard_.has_value(); }

  using Key = ScratchPadd::OnScreenKeyboard::Key;
  enum class Outcome { None, Back, NameChanged, AnimationsChanged };
  Outcome press(Key key, double now);
  Outcome touch(int x, int y, double now);

  void draw(ScratchPadd::Canvas &canvas, double now);
  // Whether something on it is still moving (a button's press glow)
  bool pressing(double now) const;

  static constexpr std::size_t kLongestName = 10; // (a DS's own allows 10)

private:
  enum Id : int { IdChange = 1, IdAnimations, IdBack };
  void setButtons();
  Outcome typed(); // (after a key on the keyboard: done typing?)

  int width_, height_;
  std::string name_;
  bool fromConsole_ = false;
  Animations animations_ = Animations::Full;
  ScratchPadd::Buttons::Panel buttons_;
  std::optional<ScratchPadd::OnScreenKeyboard> keyboard_;
};

} // namespace CosmicHex::Handheld
