// MenuScreens.hpp - the app's menus on the touch panel, and what they
// keep: the title's buttons (Play, Settings, Quit), the pause menu's
// (Resume, Restart, Title, Quit), the setup (StartMenu, its board shown on
// the stage), and the settings (your name, how much the game animates),
// both kept on the card. ScreenPadd owns one, and takes each step of its
// ScreenFlow the menus ask for (Hooks), as they ask.
#pragma once

#include "AppBasics.hpp"
#include "ScreenLayout.hpp"

#include <CosmicHex/Handheld/ButtonMenu.hpp>
#include <CosmicHex/Handheld/MenuPicture.hpp>
#include <CosmicHex/Handheld/ScreenFlow.hpp>
#include <CosmicHex/Handheld/SettingsScreen.hpp>
#include <CosmicHex/Handheld/StartMenu.hpp>
#include <CosmicHex/Rules.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>
#include <ScratchPadd/Input/Gamepad.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace CosmicHex::Handheld::App {

class MenuScreens {
public:
  // What the menus ask of the app that owns them
  struct Hooks {
    std::function<void(ScreenFlow::Event)> step; // a step of the flow, taken at once
    std::function<void()> animationsChanged;     // (Settings: the clocks paced again)
    std::function<void(const char *file, std::string_view bytes)> save; // kept on the card
  };
  // `flow`: the app's, read for which screen shows
  MenuScreens(const ScreenFlow &flow, Hooks hooks);

  // Whether a menu shows on the touch panel (the title, the pause menu, the
  // setup, the settings)
  bool showing() const;
  // The buttons on a menu screen: the title's and the pause menu's (up,
  // down, Confirm), the setup's (up, down, left, right, L and R for the
  // board, Confirm to start, Back), the settings' (and START, done). True
  // when the menu's picture changed
  bool press(const ScratchPadd::Input::Edges &pressed, ScratchPadd::Input::Button confirm,
             ScratchPadd::Input::Button back, double now);
  // A tap on a menu screen (as the stylus comes down)
  void tap(int x, int y, double now);
  // The picture of the menu showing, drawn (the settings' into `canvas`)
  std::span<const Scene::Colour> draw(ScratchPadd::Canvas &canvas, double now);
  // The menus' last picture: the setup's, as a game starts (the touch panel
  // fades from it to the map)
  std::span<const Scene::Colour> lastPicture() const { return picture_.pixels(); }
  // A settings key's glow still fading
  bool settingsPressing(double now) const { return settings_.pressing(now); }
  // Each change of screen: the pause menu's focus back on Resume
  void screenChanged() { pauseMenu_.setFocus(0); }
  // The -watch builds: the setup's Mode row set to watching
  void chooseWatching();

  // The game as the setup has it
  const StartMenu &setup() const { return setup_; }
  // Your name, as you set it in the settings (or the console's owner's)
  const std::string &yourName() const { return yourName_; }
  // How much the game animates (the user's: Full, Quick or Off)
  Animations animations() const { return animations_; }

  // The setup's board as a game would start on it (shown on the stage),
  // and the board sliding out when it changes
  const State &preview() const { return preview_; }
  const State &previewBefore() const { return previewBefore_; }
  // How far the slide from one to the other has gone, 0 to 1 (1: none)
  float slideThrough();
  // Which way it slides (+1: right was pressed, the new board comes in
  // from the right)
  int slideWay() const { return slideWay_; }

private:
  // A button on the title (Play, Settings, Quit) or the pause menu
  // (Resume, Restart, Title, Quit) chosen
  void choose(std::size_t button);
  // The settings' answer: your name changed (kept on the card), or back to
  // the title
  void settingsOutcome(SettingsScreen::Outcome outcome);
  // The setup's menu: Start or Back
  void setupOutcome(StartMenu::Outcome outcome);
  // The preview, made again; a slide when its board changed (the old one
  // out, the new one in, the way the arrow pointed: slideWay_)
  void updatePreview();
  static State previewOf(const StartMenu &menu);
  static std::optional<std::string> savedName();
  static Animations savedAnimations();

  static constexpr const char *kNameFile = "name.txt";
  static constexpr const char *kAnimationsFile = "animations.txt";

  const ScreenFlow &flow_;
  Hooks hooks_;
  // Who you are: as you set it in the settings, kept on the card, or else
  // the console's owner name
  std::string consoleName_;
  std::string yourName_;
  Animations animations_;
  ButtonMenu titleMenu_{{"Play", "Settings", "Quit"}};
  // The settings (from the title): your name, changed on a keyboard
  SettingsScreen settings_{kTouchWidth, kTouchHeight};
  ButtonMenu pauseMenu_{{"Resume", "Restart", "Title", "Quit"}};
  StartMenu setup_;
  MenuPicture picture_{kTouchWidth, kTouchHeight};
  State preview_;
  State previewBefore_; // (the board sliding out)
  bool sliding_ = false;
  int slideWay_ = 1;
  Clock::time_point slideStarted_;
};

} // namespace CosmicHex::Handheld::App
