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

namespace {
// An area in whole pixels (its edges rounded as Canvas rounds them), so a
// one-pixel edge stays one pixel at any scale
Area wholePixels(const Area &a) {
  const float left = std::round(a.x), top = std::round(a.y);
  return {left, top, std::round(a.x + a.width) - left, std::round(a.y + a.height) - top};
}

// The raised look's edges: the lit one a fifth of the way from the edge
// colour to white, the shaded one the face at half its brightness
Colour lit(Colour c) {
  const auto towardWhite = [](std::uint8_t v) {
    return static_cast<std::uint8_t>(v + (255 - v) / 5);
  };
  return {towardWhite(c.r), towardWhite(c.g), towardWhite(c.b), c.a};
}
Colour shaded(Colour c) {
  return {static_cast<std::uint8_t>(c.r / 2), static_cast<std::uint8_t>(c.g / 2),
          static_cast<std::uint8_t>(c.b / 2), c.a};
}

// Paints rectangles solid, or see-through for a button fading in or out
struct Painter {
  Canvas &canvas;
  float opacity = 1;
  void rect(float x, float y, float w, float h, Colour colour) const {
    if (opacity >= 1)
      canvas.fillRect(x, y, w, h, colour);
    else
      canvas.blendRect(x, y, w, h, colour, opacity);
  }
};

void drawFlatBody(const Painter &paint, const Area &a, Colour face, Colour edge) {
  paint.rect(a.x, a.y, a.width, a.height, face);
  paint.rect(a.x, a.y, a.width, 1, edge);
  paint.rect(a.x, a.y + a.height - 1, a.width, 1, edge);
  paint.rect(a.x, a.y + 1, 1, a.height - 2, edge);
  paint.rect(a.x + a.width - 1, a.y + 1, 1, a.height - 2, edge);
}

// The gap a sunk face leaves above it and the drop under the face, both in
// the drop's colour (every pixel of the area painted: nothing stale shows
// where the face was), then the face and its two-tone edges
void drawRaisedBody(const Painter &paint, const Area &area, const Area &f, const Style &style,
                    Colour face, Colour edge, bool sunk) {
  paint.rect(area.x, area.y, area.width, f.y - area.y, style.drop);
  const float faceBottom = f.y + f.height;
  paint.rect(area.x, faceBottom, area.width, area.y + area.height - faceBottom, style.drop);
  paint.rect(f.x, f.y, f.width, f.height, face);
  // (pressed, the light from the top left falls on its bottom and right)
  const Colour topLeft = sunk ? shaded(face) : lit(edge);
  const Colour bottomRight = sunk ? lit(edge) : shaded(face);
  paint.rect(f.x, f.y, f.width, 1, topLeft);
  paint.rect(f.x, f.y + 1, 1, f.height - 1, topLeft);
  paint.rect(f.x + 1, faceBottom - 1, f.width - 1, 1, bottomRight);
  paint.rect(f.x + f.width - 1, f.y + 1, 1, f.height - 2, bottomRight);
}

// A button's face and edges, flat or raised; where its face went
Area drawBody(const Painter &paint, const Style &style, const Button &button, bool sunk) {
  const Area area = wholePixels(button.area);
  const Area face = Panel::faceOf(button, style, sunk);
  const Colour faceColour = button.face.value_or(style.face);
  const Colour edgeColour = button.edge.value_or(style.edge);
  if (style.bevel == Bevel::Flat)
    drawFlatBody(paint, area, faceColour, edgeColour);
  else
    drawRaisedBody(paint, area, face, style, faceColour, edgeColour, sunk);
  return face;
}

// A button's icon, in the middle of its face
void drawIcon(Canvas &canvas, const Button &button, const Area &face, float opacity) {
  const auto size = static_cast<float>(button.iconSize);
  const int left = static_cast<int>(std::lround(face.x + (face.width - size) / 2));
  const int top = static_cast<int>(std::lround(face.y + (face.height - size) / 2));
  canvas.blend(*button.icon, button.iconSize, button.iconSize, left, top, opacity);
}

// A button's words, in the middle of its face: the focus colour when the
// D-pad is on it, darker when it can't be used
void drawLabel(Canvas &canvas, const Style &style, const Button &button, const Area &face,
               bool focused) {
  Colour words = focused ? style.focus : style.words;
  if (!button.usable) {
    words.r = static_cast<std::uint8_t>(std::lround(words.r * style.dimmed));
    words.g = static_cast<std::uint8_t>(std::lround(words.g * style.dimmed));
    words.b = static_cast<std::uint8_t>(std::lround(words.b * style.dimmed));
  }
  canvas.textCentred(face.x + face.width / 2, face.y + face.height / 2, button.label, words,
                     style.labelScale);
}
} // namespace

Area Panel::faceOf(const Button &button, const Style &style, bool sunk) {
  const Area area = wholePixels(button.area);
  if (style.bevel == Bevel::Flat)
    return area;
  // (the drop no taller than leaves the face room for its edges)
  const auto drop = static_cast<float>(
      std::clamp(style.dropPixels, 0, std::max(0, static_cast<int>(area.height) - 4)));
  const float sink = sunk && drop > 0 ? 1.0f : 0.0f;
  return {area.x, area.y + sink, area.width, area.height - drop};
}

bool Panel::sunk(const Button &button) const {
  return pressed_ == button.id && pressGlow(now_) > 0;
}

// A button fading in or out: its face, edge and icon see-through over
// what's under it (no focus, glow or toggle: it can't be touched yet)
void Panel::drawFading(Canvas &canvas, const Style &style, const Button &button) {
  const Area face = drawBody({canvas, button.shown}, style, button, false);
  if (button.icon && button.iconSize > 0)
    drawIcon(canvas, button, face, (button.usable ? 1.0f : style.dimmed) * button.shown);
}

void Panel::draw(Canvas &canvas, const Style &style) const {
  for (const Button &button : buttons_) {
    if (button.shown <= 0)
      continue;
    if (button.shown < 1) {
      drawFading(canvas, style, button);
      continue;
    }
    const bool pressedNow = sunk(button);
    const bool focused = focus_ == button.id;
    const Area face = drawBody({canvas}, style, button, pressedNow);
    if (button.icon && button.iconSize > 0)
      drawIcon(canvas, button, face, button.usable ? 1.0f : style.dimmed);
    else if (!button.label.empty())
      drawLabel(canvas, style, button, face, focused);
    if (focused) {
      canvas.outline(face.x, face.y, face.width, face.height, style.focus);
      canvas.outline(face.x + 1, face.y + 1, face.width - 2, face.height - 2, style.focus);
    }
    const Area &a = button.area;
    if (button.on)
      canvas.outline(a.x - 1, a.y - 1, a.width + 2, a.height + 2, style.on, 2);
    if (pressedNow)
      canvas.blendRect(face.x + 1, face.y + 1, face.width - 2, face.height - 2, style.press,
                       style.pressOpacity * pressGlow(now_));
  }
  drawMessage(canvas, style);
}

void Panel::drawMessage(Canvas &canvas, const Style &style) const {
  if (message_.empty() || !messageArea_)
    return;
  const Area &m = *messageArea_;
  canvas.fillRect(m.x, m.y, m.width, m.height, style.band);
  canvas.outline(m.x, m.y, m.width, m.height, style.message);
  canvas.textCentred(m.x + m.width / 2, m.y + m.height / 2, message_, style.message);
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
