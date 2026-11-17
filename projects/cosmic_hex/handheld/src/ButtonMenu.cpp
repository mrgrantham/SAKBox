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
  const std::vector<Button> all = buttons(width, height);
  const float fx = static_cast<float>(x), fy = static_cast<float>(y);
  for (std::size_t i = 0; i < all.size(); ++i) {
    const Button &b = all[i];
    if (fx >= b.x && fx < b.x + b.width && fy >= b.y && fy < b.y + b.height) {
      focus_ = i;
      return i;
    }
  }
  return std::nullopt;
}

} // namespace CosmicHex::Handheld
