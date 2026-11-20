#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/SettingsScreen.hpp>

namespace CosmicHex::Handheld {

namespace {
const ScratchPadd::Buttons::Style kButtons{.face = {31, 42, 64, 255},
                                           .edge = {58, 74, 106, 255},
                                           .words = Palette::kWords,
                                           .focus = Palette::kHighlight};
} // namespace

SettingsScreen::SettingsScreen(int width, int height) : width_(width), height_(height) {
  setButtons();
}

void SettingsScreen::open(std::string name, bool fromConsole) {
  name_ = std::move(name);
  fromConsole_ = fromConsole;
  keyboard_.reset();
  setButtons();
  buttons_.setFocus(kChange);
}

void SettingsScreen::setButtons() {
  const float k = static_cast<float>(width_) / 256;
  buttons_.set({{.id = kChange, .area = {16 * k, 88 * k, 96 * k, 22 * k}, .label = "Change"},
                {.id = kBack, .area = {16 * k, 160 * k, 96 * k, 22 * k}, .label = "Back"}});
}

SettingsScreen::Outcome SettingsScreen::typed() {
  if (!keyboard_ || keyboard_->outcome() != ScratchPadd::OnScreenKeyboard::Outcome::Done)
    return Outcome::None;
  const std::string typed = keyboard_->text();
  keyboard_.reset();
  if (typed.empty() || typed == name_)
    return Outcome::None; // (nothing typed, or the same: kept)
  name_ = typed;
  fromConsole_ = false;
  return Outcome::NameChanged;
}

SettingsScreen::Outcome SettingsScreen::press(Key key, double now) {
  if (keyboard_) {
    keyboard_->press(key, now);
    return typed();
  }
  switch (key) {
  case Key::Up:
    buttons_.move(ScratchPadd::Buttons::Direction::Up);
    break;
  case Key::Down:
    buttons_.move(ScratchPadd::Buttons::Direction::Down);
    break;
  case Key::Back:
    return Outcome::Back;
  case Key::Confirm:
    if (const auto focused = buttons_.focus()) {
      buttons_.press(*focused, now);
      if (*focused == kBack)
        return Outcome::Back;
      keyboard_.emplace(name_, kLongestName);
      keyboard_->layOut(width_, height_);
    }
    break;
  default:
    break;
  }
  return Outcome::None;
}

SettingsScreen::Outcome SettingsScreen::touch(int x, int y, double now) {
  if (keyboard_) {
    keyboard_->touch(x, y, now);
    return typed();
  }
  const auto touched = buttons_.at(x, y);
  if (!touched)
    return Outcome::None;
  buttons_.setFocus(*touched);
  return press(Key::Confirm, now);
}

void SettingsScreen::draw(ScratchPadd::Canvas &canvas, double now) {
  if (keyboard_) {
    keyboard_->draw(canvas, {.keys = kButtons}, now);
    return;
  }
  const float k = static_cast<float>(width_) / 256;
  canvas.clear(Palette::kSpace);
  canvas.textCentred(static_cast<float>(width_) / 2, 18 * k, "SETTINGS", Palette::kWords,
                     k >= 1.2f ? 2.5f : 2);
  canvas.text(16 * k, 44 * k, "Your name", Palette::kDimWords);
  canvas.outline(16 * k, 54 * k, 224 * k, 26 * k, Palette::kAccent);
  canvas.text(24 * k, 62 * k, name_, Palette::kWords, k >= 1.2f ? 2.0f : 1.5f);
  if (fromConsole_)
    canvas.text(144 * k, 44 * k, "from the console", Palette::kDimWords);
  buttons_.setTime(now);
  buttons_.draw(canvas, kButtons);
}

bool SettingsScreen::pressing(double now) const {
  return keyboard_ ? keyboard_->pressing(now) : buttons_.pressing(now);
}

} // namespace CosmicHex::Handheld
