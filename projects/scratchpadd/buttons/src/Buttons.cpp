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
  if (pressed_ && !find(*pressed_))
    pressed_.reset();
}

const Button *Panel::find(int id) const {
  for (const Button &button : buttons_)
    if (button.id == id)
      return &button;
  return nullptr;
}

std::optional<int> Panel::at(int x, int y) const {
  for (const Button &button : buttons_)
    // (one fading out, no longer usable, lets the touch pass; one fading
    // in answers at once, so a quick tap isn't lost)
    if ((button.shown >= 1 || button.usable) && button.area.contains(x, y))
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
  // The nearest button within 45 degrees of the way pressed, if there is
  // one (a keyboard's staggered rows: right from Q is W, not A below it);
  // else, as before, the nearest along that way, then across
  const Button *best = nullptr, *inCone = nullptr;
  float bestAlong = std::numeric_limits<float>::max(), bestAcross = bestAlong;
  float bestDistance = std::numeric_limits<float>::max();
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
    if (across <= along && along * along + across * across < bestDistance) {
      inCone = &button;
      bestDistance = along * along + across * across;
    }
    if (along < bestAlong - 0.5f || (std::fabs(along - bestAlong) <= 0.5f && across < bestAcross)) {
      best = &button;
      bestAlong = along;
      bestAcross = across;
    }
  }
  if (inCone)
    best = inCone;
  if (best)
    focus_ = best->id;
}

void Panel::press(int id, double now, std::string message) {
  pressed_ = id;
  pressedAt_ = now;
  if (!message.empty())
    message_ = std::move(message);
}

float Panel::pressGlow(double now) const {
  if (!pressed_)
    return 0;
  const double t = (now - pressedAt_) / kPressSeconds;
  if (t < 0 || t >= 1)
    return 0;
  return static_cast<float>(t < 0.2 ? t / 0.2 : 1 - (t - 0.2) / 0.8);
}

void Panel::clearFeedback() { message_.clear(); }

// A button fading in or out: its face, edge and icon see-through over
// what's under it (no focus, glow or toggle: it can't be touched yet)
void Panel::drawFading(Canvas &canvas, const Style &style, const Button &button) {
  const Area &a = button.area;
  canvas.blendRect(a.x, a.y, a.width, a.height, button.face.value_or(style.face), button.shown);
  const Colour edge = button.edge.value_or(style.edge);
  canvas.blendRect(a.x, a.y, a.width, 1, edge, button.shown);
  canvas.blendRect(a.x, a.y + a.height - 1, a.width, 1, edge, button.shown);
  canvas.blendRect(a.x, a.y + 1, 1, a.height - 2, edge, button.shown);
  canvas.blendRect(a.x + a.width - 1, a.y + 1, 1, a.height - 2, edge, button.shown);
  if (button.icon && button.iconSize > 0) {
    const int left =
        static_cast<int>(std::lround(a.x + (a.width - static_cast<float>(button.iconSize)) / 2));
    const int top =
        static_cast<int>(std::lround(a.y + (a.height - static_cast<float>(button.iconSize)) / 2));
    canvas.blend(*button.icon, button.iconSize, button.iconSize, left, top,
                 (button.usable ? 1.0f : style.dimmed) * button.shown);
  }
}

void Panel::draw(Canvas &canvas, const Style &style) const {
  for (const Button &button : buttons_) {
    const Area &a = button.area;
    if (button.shown <= 0)
      continue;
    if (button.shown < 1) {
      drawFading(canvas, style, button);
      continue;
    }
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
        words.r = static_cast<std::uint8_t>(std::lround(words.r * style.dimmed));
        words.g = static_cast<std::uint8_t>(std::lround(words.g * style.dimmed));
        words.b = static_cast<std::uint8_t>(std::lround(words.b * style.dimmed));
      }
      canvas.textCentred(a.x + a.width / 2, a.y + a.height / 2, button.label, words,
                         style.labelScale);
    }
    if (focus_ == button.id) {
      canvas.outline(a.x, a.y, a.width, a.height, style.focus);
      canvas.outline(a.x + 1, a.y + 1, a.width - 2, a.height - 2, style.focus);
    }
    if (button.on)
      canvas.outline(a.x - 1, a.y - 1, a.width + 2, a.height + 2, style.on, 2);
    if (pressed_ == button.id) {
      if (const float glow = pressGlow(now_); glow > 0)
        canvas.blendRect(a.x + 1, a.y + 1, a.width - 2, a.height - 2, style.press,
                         style.pressOpacity * glow);
    }
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
    out.push_back({a.x - 2, a.y - 2, a.width + 4, a.height + 4}); // (the on outline too)
  }
  if (messageArea_)
    out.push_back(*messageArea_);
  return out;
}

} // namespace ScratchPadd::Buttons
