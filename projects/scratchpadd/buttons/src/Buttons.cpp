#include <ScratchPadd/Buttons/Buttons.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ScratchPadd::Buttons {

bool Area::contains(int px, int py) const {
  const auto fx = static_cast<float>(px), fy = static_cast<float>(py);
  return fx >= x && fx < x + width && fy >= y && fy < y + height;
}

void Panel::set(std::vector<Button> buttons) {
  buttons_ = std::move(buttons);
  if (focus_ && !find(*focus_))
    focus_.reset();
  if (flashed_ && !find(*flashed_))
    flashed_.reset();
}

const Button *Panel::find(int id) const {
  for (const Button &button : buttons_)
    if (button.id == id)
      return &button;
  return nullptr;
}

std::optional<int> Panel::at(int x, int y) const {
  for (const Button &button : buttons_)
    if (button.area.contains(x, y))
      return button.id;
  return std::nullopt;
}

void Panel::setFocus(std::optional<int> id) { focus_ = id && find(*id) ? id : std::nullopt; }

void Panel::move(Direction direction) {
  const Button *from = focus_ ? find(*focus_) : nullptr;
  if (!from) {
    // (no focus yet: the first usable one)
    for (const Button &button : buttons_)
      if (button.usable) {
        focus_ = button.id;
        return;
      }
    return;
  }
  const auto middle = [](const Area &a) {
    return std::pair{a.x + a.width / 2, a.y + a.height / 2};
  };
  const auto [fx, fy] = middle(from->area);
  const Button *best = nullptr;
  float bestAlong = std::numeric_limits<float>::max(), bestAcross = bestAlong;
  for (const Button &button : buttons_) {
    if (!button.usable || &button == from)
      continue;
    const auto [x, y] = middle(button.area);
    float along = 0, across = 0;
    switch (direction) {
    case Direction::Up:
      along = fy - y, across = std::fabs(x - fx);
      break;
    case Direction::Down:
      along = y - fy, across = std::fabs(x - fx);
      break;
    case Direction::Left:
      along = fx - x, across = std::fabs(y - fy);
      break;
    case Direction::Right:
      along = x - fx, across = std::fabs(y - fy);
      break;
    }
    if (along <= 0.5f)
      continue; // (not that way)
    if (along < bestAlong - 0.5f || (std::fabs(along - bestAlong) <= 0.5f && across < bestAcross)) {
      best = &button;
      bestAlong = along;
      bestAcross = across;
    }
  }
  if (best)
    focus_ = best->id;
}

void Panel::flash(int id, std::string message) {
  flashed_ = id;
  message_ = std::move(message);
}

void Panel::clearFeedback() {
  flashed_.reset();
  message_.clear();
}

void Panel::draw(Canvas &canvas, const Style &style) const {
  for (const Button &button : buttons_) {
    const Area &a = button.area;
    canvas.fillRect(a.x, a.y, a.width, a.height, button.face.value_or(style.face));
    canvas.outline(a.x, a.y, a.width, a.height, button.edge.value_or(style.edge));
    if (button.icon && button.iconSize > 0) {
      const int left =
          static_cast<int>(std::lround(a.x + (a.width - static_cast<float>(button.iconSize)) / 2));
      const int top =
          static_cast<int>(std::lround(a.y + (a.height - static_cast<float>(button.iconSize)) / 2));
      canvas.blend(*button.icon, button.iconSize, button.iconSize, left, top,
                   button.usable ? 1.0f : style.dimmed);
    } else if (!button.label.empty()) {
      const bool focused = focus_ == button.id;
      Colour words = focused ? style.focus : style.words;
      if (!button.usable) {
        words.r = static_cast<std::uint8_t>(words.r * style.dimmed + 0.5f);
        words.g = static_cast<std::uint8_t>(words.g * style.dimmed + 0.5f);
        words.b = static_cast<std::uint8_t>(words.b * style.dimmed + 0.5f);
      }
      canvas.textCentred(a.x + a.width / 2, a.y + a.height / 2, button.label, words,
                         style.labelScale);
    }
    if (focus_ == button.id) {
      canvas.outline(a.x, a.y, a.width, a.height, style.focus);
      canvas.outline(a.x + 1, a.y + 1, a.width - 2, a.height - 2, style.focus);
    }
    if (flashed_ == button.id)
      canvas.outline(a.x - 1, a.y - 1, a.width + 2, a.height + 2, style.flash, 2);
  }
  if (!message_.empty() && messageArea_) {
    const Area &m = *messageArea_;
    canvas.fillRect(m.x, m.y, m.width, m.height, style.band);
    canvas.outline(m.x, m.y, m.width, m.height, style.message);
    canvas.textCentred(m.x + m.width / 2, m.y + m.height / 2, message_, style.message);
  }
}

std::vector<Area> Panel::areas() const {
  std::vector<Area> out;
  for (const Button &button : buttons_) {
    const Area &a = button.area;
    out.push_back({a.x - 2, a.y - 2, a.width + 4, a.height + 4}); // (the flash's outline too)
  }
  if (messageArea_)
    out.push_back(*messageArea_);
  return out;
}

} // namespace ScratchPadd::Buttons
