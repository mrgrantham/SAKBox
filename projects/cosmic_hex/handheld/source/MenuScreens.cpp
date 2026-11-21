#include "MenuScreens.hpp"

#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/TableGame.hpp>
#include <ScratchPadd/OwnerName/OwnerName.hpp>
#include <ScratchPadd/Storage/Files.hpp>
#include <ScratchPadd/Storage/Folders.hpp>

#include <array>
#include <utility>

namespace CosmicHex::Handheld::App {

namespace Input = ScratchPadd::Input;
using Screen = ScreenFlow::Screen;

MenuScreens::MenuScreens(const ScreenFlow &flow, Hooks hooks)
    : flow_(flow), hooks_(std::move(hooks)), consoleName_(ScratchPadd::OwnerName::get()),
      yourName_(savedName().value_or(consoleName_)), animations_(savedAnimations()),
      preview_(previewOf(setup_)), previewBefore_(preview_) {}

bool MenuScreens::showing() const {
  const Screen screen = flow_.screen();
  return screen == Screen::Title || screen == Screen::Paused || screen == Screen::Setup ||
         screen == Screen::Settings;
}

bool MenuScreens::press(const Input::Edges &pressed, Input::Button confirm, Input::Button back,
                        double now) {
  using Input::Button;
  bool changed = false;
  switch (flow_.screen()) {
  case Screen::Title:
  case Screen::Paused: {
    ButtonMenu &menu = flow_.screen() == Screen::Title ? titleMenu_ : pauseMenu_;
    std::optional<std::size_t> chosen;
    for (const auto &[button, key] : {std::pair{Button::Up, ButtonMenu::Key::Up},
                                      std::pair{Button::Down, ButtonMenu::Key::Down},
                                      std::pair{confirm, ButtonMenu::Key::Confirm}})
      if (pressed.pressed(button)) {
        chosen = menu.press(key);
        changed = true;
      }
    if (chosen)
      choose(*chosen);
    break;
  }
  case Screen::Setup: {
    using Key = StartMenu::Key;
    StartMenu::Outcome outcome = StartMenu::Outcome::None;
    for (const auto &[button, key] :
         {std::pair{Button::Up, Key::Up}, std::pair{Button::Down, Key::Down},
          std::pair{Button::Left, Key::Left}, std::pair{Button::Right, Key::Right},
          std::pair{Button::L, Key::PreviousBoard}, std::pair{Button::R, Key::NextBoard},
          std::pair{confirm, Key::Confirm}, std::pair{back, Key::Back}})
      if (pressed.pressed(button) && outcome == StartMenu::Outcome::None) {
        if (key == Key::Left || key == Key::PreviousBoard)
          slideWay_ = -1;
        if (key == Key::Right || key == Key::NextBoard)
          slideWay_ = 1;
        outcome = setup_.press(key);
        changed = true;
        updatePreview();
      }
    setupOutcome(outcome);
    break;
  }
  case Screen::Settings: {
    using Key = SettingsScreen::Key;
    for (const auto &[button, key] :
         {std::pair{Button::Up, Key::Up}, std::pair{Button::Down, Key::Down},
          std::pair{Button::Left, Key::Left}, std::pair{Button::Right, Key::Right},
          std::pair{confirm, Key::Confirm}, std::pair{back, Key::Back},
          std::pair{Button::Start, Key::Done}})
      if (pressed.pressed(button)) {
        settingsOutcome(settings_.press(key, now));
        changed = true;
      }
    break;
  }
  case Screen::Game:
  case Screen::Won:
    break;
  }
  return changed;
}

void MenuScreens::tap(int x, int y, double now) {
  const Screen screen = flow_.screen();
  if (screen == Screen::Setup) {
    slideWay_ = x > kTouchWidth / 2 ? 1 : -1; // (the right arrow, or the left)
    setupOutcome(setup_.touch(x, y, kTouchWidth, kTouchHeight));
    updatePreview();
  } else if (screen == Screen::Settings) {
    settingsOutcome(settings_.touch(x, y, now));
  } else if (screen == Screen::Title || screen == Screen::Paused) {
    ButtonMenu &menu = screen == Screen::Title ? titleMenu_ : pauseMenu_;
    if (const auto chosen = menu.touch(x, y, kTouchWidth, kTouchHeight))
      choose(*chosen);
  }
}

std::span<const Scene::Colour> MenuScreens::draw(ScratchPadd::Canvas &canvas, double now) {
  const Screen screen = flow_.screen();
  if (screen == Screen::Title || screen == Screen::Paused) {
    picture_.draw(screen == Screen::Title ? titleMenu_ : pauseMenu_);
    return picture_.pixels();
  }
  if (screen == Screen::Setup) {
    picture_.draw(setup_);
    return picture_.pixels();
  }
  settings_.draw(canvas, now);
  return canvas.pixels();
}

void MenuScreens::chooseWatching() {
  for (int i = 0; i < 4; ++i) // (down to the Mode row: watch)
    setup_.press(StartMenu::Key::Down);
  setup_.press(StartMenu::Key::Right);
}

// (the setup's board slides on the dice's curve: slideAcross)
float MenuScreens::slideThrough() {
  const float through = sliding_ ? slideAcross(static_cast<float>(secondsSince(slideStarted_))) : 1;
  if (through >= 1)
    sliding_ = false;
  return through;
}

void MenuScreens::choose(std::size_t button) {
  using Event = ScreenFlow::Event;
  if (flow_.screen() == Screen::Title) {
    static constexpr std::array<Event, 3> kTitle{Event::Play, Event::Settings, Event::Quit};
    if (button < kTitle.size())
      hooks_.step(kTitle[button]);
    if (flow_.screen() == Screen::Settings)
      settings_.open(yourName_, yourName_ == consoleName_ && !consoleName_.empty(), animations_);
    updatePreview();
    return;
  }
  static constexpr std::array<Event, 4> kPause{Event::Resume, Event::Restart, Event::ToTitle,
                                               Event::Quit};
  if (button < kPause.size())
    hooks_.step(kPause[button]);
}

void MenuScreens::settingsOutcome(SettingsScreen::Outcome outcome) {
  if (outcome == SettingsScreen::Outcome::NameChanged) {
    yourName_ = settings_.name();
    hooks_.save(kNameFile, yourName_);
  } else if (outcome == SettingsScreen::Outcome::AnimationsChanged) {
    animations_ = settings_.animations();
    hooks_.animationsChanged();
    hooks_.save(kAnimationsFile, wordOf(animations_));
  } else if (outcome == SettingsScreen::Outcome::Back) {
    hooks_.step(ScreenFlow::Event::Back);
  }
}

void MenuScreens::setupOutcome(StartMenu::Outcome outcome) {
  if (outcome == StartMenu::Outcome::Start)
    hooks_.step(ScreenFlow::Event::Start);
  else if (outcome == StartMenu::Outcome::Back)
    hooks_.step(ScreenFlow::Event::Back);
}

void MenuScreens::updatePreview() {
  State next = previewOf(setup_);
  const bool changed = next.settings.homeHexes != preview_.settings.homeHexes ||
                       next.settings.planetHexes != preview_.settings.planetHexes;
  if (changed && flow_.screen() == Screen::Setup) {
    previewBefore_ = preview_;
    sliding_ = true;
    slideStarted_ = Clock::now();
  }
  preview_ = std::move(next);
}

State MenuScreens::previewOf(const StartMenu &menu) {
  TableSettings settings{.players = menu.players(), .board = menu.boardItself()};
  return CosmicHex::newGame(settings.players, rulesFor(settings));
}

std::optional<std::string> MenuScreens::savedName() {
  const auto bytes =
      ScratchPadd::Storage::loadFile(ScratchPadd::Storage::Folders::forApp().files + kNameFile);
  if (!bytes || bytes->empty())
    return std::nullopt;
  return std::string(bytes->begin(), bytes->end());
}

Animations MenuScreens::savedAnimations() {
  const auto bytes = ScratchPadd::Storage::loadFile(ScratchPadd::Storage::Folders::forApp().files +
                                                    kAnimationsFile);
  if (!bytes)
    return Animations::Full;
  return animationsFrom(std::string(bytes->begin(), bytes->end())).value_or(Animations::Full);
}

} // namespace CosmicHex::Handheld::App
