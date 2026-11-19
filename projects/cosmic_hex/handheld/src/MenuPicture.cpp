#include <CosmicHex/Handheld/MenuPicture.hpp>

#include <ScratchPadd/Scene/OverlayText.hpp>

#include <algorithm>
#include <cmath>

namespace CosmicHex::Handheld {

namespace {
using Colour = ScratchPadd::Scene::Colour;
constexpr Colour kBackground{16, 24, 42, 255};
constexpr Colour kButton{29, 42, 69, 255};
constexpr Colour kEdge{90, 107, 140, 255};
constexpr Colour kChosen{255, 214, 90, 255};
constexpr Colour kWords{200, 208, 222, 255};
constexpr Colour kPlay{42, 90, 58, 255};
constexpr Colour kWatch{42, 58, 90, 255};
namespace Scene = ScratchPadd::Scene;

// A line's letters' own width, unscaled
float textWidth(const std::string &text) { return Scene::Overlay::inkOf(text).width; }
} // namespace

MenuPicture::MenuPicture(int width, int height)
    : width_(width), height_(height),
      pixels_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {}

void MenuPicture::fillRect(float x, float y, float w, float h, Colour colour) {
  const int x0 = std::max(0, static_cast<int>(std::lround(x)));
  const int y0 = std::max(0, static_cast<int>(std::lround(y)));
  const int x1 = std::min(width_, static_cast<int>(std::lround(x + w)));
  const int y1 = std::min(height_, static_cast<int>(std::lround(y + h)));
  for (int py = y0; py < y1; ++py)
    for (int px = x0; px < x1; ++px)
      pixels_[(static_cast<std::size_t>(py) * static_cast<std::size_t>(width_)) +
              static_cast<std::size_t>(px)] = colour;
}

void MenuPicture::outline(float x, float y, float w, float h, Colour colour) {
  fillRect(x, y, w, 1, colour);
  fillRect(x, y + h - 1, w, 1, colour);
  fillRect(x, y, 1, h, colour);
  fillRect(x + w - 1, y, 1, h, colour);
}

void MenuPicture::text(const std::string &words, float x, float y, float w, float h, float scale,
                       Colour colour, bool centred) {
  // Centred by the letters' own pixels (Overlay::inkOf, as the overlay's
  // textCentred), down always and across when asked
  const Scene::Overlay::Ink ink = Scene::Overlay::inkOf(words, scale);
  const float left = centred ? x + (w - ink.width) / 2 - ink.left : x;
  const float top = y + (h - ink.height) / 2 - ink.top;
  for (const Scene::TextRectangle &r : Scene::textRectangles({left, top, words, colour, scale}))
    fillRect(r.x, r.y, std::max(1.0f, r.width), std::max(1.0f, r.height), colour);
}

void MenuPicture::draw(const StartMenu &menu) {
  std::fill(pixels_.begin(), pixels_.end(), kBackground);
  const float scale = width_ >= 300 ? 1.5f : 1.0f; // (the 3DS's wider screen: bigger words)
  const auto buttons = menu.buttons(width_, height_);
  for (const StartMenu::Button &button : buttons) {
    const bool chosen =
        menu.focus() == button.row &&
        (button.step != 0 || (button.starts == StartMenu::Outcome::Back) == menu.backChosen());
    if (button.step != 0) {
      fillRect(button.x, button.y, button.width, button.height, kButton);
      outline(button.x, button.y, button.width, button.height, chosen ? kChosen : kEdge);
      text(button.step < 0 ? "<" : ">", button.x, button.y, button.width, button.height,
           scale * 1.4f, chosen ? kChosen : kWords);
      continue;
    }
    const bool start = button.starts == StartMenu::Outcome::Start;
    fillRect(button.x, button.y, button.width, button.height, start ? kPlay : kWatch);
    if (chosen) {
      outline(button.x, button.y, button.width, button.height, kChosen);
      outline(button.x + 1, button.y + 1, button.width - 2, button.height - 2, kChosen);
    }
    text(start ? "Start" : "Back", button.x, button.y, button.width, button.height, scale * 1.6f,
         chosen ? kChosen : kWords);
  }
  // Each row's label (left) and value (between its arrows)
  for (int r = 0; r < static_cast<int>(StartMenu::Row::Start); ++r) {
    const auto row = static_cast<StartMenu::Row>(r);
    const StartMenu::Area area = menu.valueArea(row, width_, height_);
    const bool chosen = menu.focus() == row;
    text(menu.label(row), static_cast<float>(width_) * 0.05f, area.y, 0, area.height, scale,
         chosen ? kChosen : kWords, false);
    // (a long value smaller, to fit between the arrows with a little room)
    const std::string value = menu.value(row);
    const float fits = (area.width - 14) / textWidth(value);
    text(value, area.x, area.y, area.width, area.height, std::min(scale, fits),
         chosen ? kChosen : kWords);
  }
}

void MenuPicture::draw(const ButtonMenu &menu) {
  std::fill(pixels_.begin(), pixels_.end(), kBackground);
  const float scale = width_ >= 300 ? 1.6f : 1.2f;
  const auto buttons = menu.buttons(width_, height_);
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    const ButtonMenu::Button &b = buttons[i];
    const bool chosen = menu.focus() == i;
    fillRect(b.x, b.y, b.width, b.height, i == 0 ? kPlay : kButton);
    if (chosen) {
      outline(b.x, b.y, b.width, b.height, kChosen);
      outline(b.x + 1, b.y + 1, b.width - 2, b.height - 2, kChosen);
    } else {
      outline(b.x, b.y, b.width, b.height, kEdge);
    }
    text(menu.labels()[i], b.x, b.y, b.width, b.height, scale, chosen ? kChosen : kWords);
  }
}

} // namespace CosmicHex::Handheld
