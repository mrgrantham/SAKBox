#include <CosmicHex/Handheld/ButtonMenu.hpp>

#include <algorithm>

namespace CosmicHex::Handheld {

std::optional<std::size_t> ButtonMenu::press(Key key) {
  if (labels_.empty())
    return std::nullopt;
  switch (key) {
  case Key::Up:
    focus_ = (focus_ + labels_.size() - 1) % labels_.size();
    return std::nullopt;
  case Key::Down:
    focus_ = (focus_ + 1) % labels_.size();
    return std::nullopt;
  case Key::Confirm:
    return focus_;
  }
  return std::nullopt;
}

std::vector<ButtonMenu::Button> ButtonMenu::buttons(int width, int height) const {
  // A column 60% of the screen wide, each button up to a fifth of its
  // height, with a gap between, centred
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const auto count = static_cast<float>(std::max<std::size_t>(labels_.size(), 1));
  const float each = std::min(h / 5, h * 0.9f / count);
  const float tall = each * 0.8f, column = each * count;
  std::vector<Button> out;
  out.reserve(labels_.size());
  for (std::size_t i = 0; i < labels_.size(); ++i)
    out.push_back({w * 0.2f, (h - column) / 2 + each * static_cast<float>(i) + (each - tall) / 2,
                   w * 0.6f, tall});
  return out;
}

std::optional<std::size_t> ButtonMenu::touch(int x, int y, int width, int height) {
  const std::optional<int> touched = panel(width, height, std::nullopt).at(x, y);
  if (!touched)
    return std::nullopt;
  focus_ = static_cast<std::size_t>(*touched);
  return focus_;
}

ScratchPadd::Buttons::Panel
ButtonMenu::panel(int width, int height,
                  std::optional<ScratchPadd::Buttons::Colour> firstFace) const {
  std::vector<ScratchPadd::Buttons::Button> out;
  const std::vector<Button> places = buttons(width, height);
  for (std::size_t i = 0; i < places.size(); ++i) {
    const Button &b = places[i];
    out.push_back({.id = static_cast<int>(i),
                   .area = {b.x, b.y, b.width, b.height},
                   .label = labels_[i],
                   .face = i == 0 ? firstFace : std::nullopt});
  }
  ScratchPadd::Buttons::Panel panel;
  panel.set(std::move(out));
  if (!labels_.empty())
    panel.setFocus(static_cast<int>(focus_));
  return panel;
}

} // namespace CosmicHex::Handheld
