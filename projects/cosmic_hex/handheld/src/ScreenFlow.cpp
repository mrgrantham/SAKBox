#include <CosmicHex/Handheld/ScreenFlow.hpp>

namespace CosmicHex::Handheld {

ScreenFlow::Step ScreenFlow::on(Event event) {
  // Each screen's events, as the diagram in the header draws them
  Step step;
  const auto go = [&](Screen to) {
    step.changed = to != screen_;
    screen_ = to;
  };
  switch (screen_) {
  case Screen::Title:
    if (event == Event::Play)
      go(Screen::Setup);
    else if (event == Event::Settings)
      go(Screen::Settings);
    else if (event == Event::Quit)
      step.quit = true;
    break;
  case Screen::Setup:
    if (event == Event::Start) {
      go(Screen::Game);
      step.startGame = true;
    } else if (event == Event::Back) {
      go(Screen::Title);
    }
    break;
  case Screen::Game:
    if (event == Event::Pause) {
      go(Screen::Paused);
      step.holdChanged = step.holdGame = true;
    } else if (event == Event::GameWon) {
      go(Screen::Won);
    }
    break;
  case Screen::Paused:
    if (event == Event::Resume) {
      go(Screen::Game);
      step.holdChanged = true; // (holdGame false: carry on)
    } else if (event == Event::Restart) {
      go(Screen::Game);
      step.stopGame = step.startGame = true;
      step.holdChanged = true;
    } else if (event == Event::ToTitle) {
      go(Screen::Title);
      step.stopGame = true;
      step.holdChanged = true;
    } else if (event == Event::Quit) {
      step.quit = true;
    }
    break;
  case Screen::Settings:
    if (event == Event::Back)
      go(Screen::Title);
    break;
  case Screen::Won:
    if (event == Event::Continue) {
      go(Screen::Title);
      step.stopGame = true;
    }
    break;
  }
  return step;
}

} // namespace CosmicHex::Handheld
