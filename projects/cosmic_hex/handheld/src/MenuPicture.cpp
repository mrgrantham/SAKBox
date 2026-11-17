#include <CosmicHex/Handheld/MenuPicture.hpp>

#include <stb/stb_easy_font.h>

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
constexpr float kLetterHeight = 7; // (stb_easy_font's capitals, unscaled)

// A letter's rectangle, unscaled (stb_easy_font draws each letter as a few)
struct Rect {
  float x0, y0, x1, y1;
};

// stb_easy_font's rectangles for a line, unscaled, from (0, 0)
std::vector<Rect> letterRects(const std::string &text) {
  // (its vertices: x, y, z and a colour, 16 bytes; 4 a rectangle, at most
  // 11 rectangles a letter, as OverlayText counts)
  struct Vertex {
    float x, y, z;
    unsigned char colour[4];
  };
  std::string copy = text;
  std::vector<Vertex> vertices((copy.size() + 1) * 11 * 4);
  const int count = stb_easy_font_print(0, 0, copy.data(), nullptr, vertices.data(),
                                        static_cast<int>(vertices.size() * sizeof(Vertex)));
  std::vector<Rect> out;
  for (int q = 0; q < count; ++q) {
    const Vertex *v = &vertices[static_cast<std::size_t>(q) * 4];
    out.push_back({v[0].x, v[0].y, v[2].x, v[2].y});
  }
  return out;
}

// A line's width, unscaled
float textWidth(const std::string &text) {
  float right = 0;
  for (const Rect &r : letterRects(text))
    right = std::max(right, r.x1);
  return right;
}
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
  const float left = centred ? x + (w - textWidth(words) * scale) / 2 : x;
  const float top = y + (h - kLetterHeight * scale) / 2;
  for (const Rect &r : letterRects(words))
    fillRect(left + r.x0 * scale, top + r.y0 * scale, std::max(1.0f, (r.x1 - r.x0) * scale),
             std::max(1.0f, (r.y1 - r.y0) * scale), colour);
}

void MenuPicture::draw(const StartMenu &menu) {
  std::fill(pixels_.begin(), pixels_.end(), kBackground);
  const float scale = width_ >= 300 ? 1.5f : 1.0f; // (the 3DS's wider screen: bigger words)
  const auto buttons = menu.buttons(width_, height_);
  for (const StartMenu::Button &button : buttons) {
    const bool chosen =
        menu.focus() == button.row &&
        (button.step != 0 || (button.starts == StartMenu::Outcome::Watch) == menu.watchChosen());
    if (button.step != 0) {
      fillRect(button.x, button.y, button.width, button.height, kButton);
      outline(button.x, button.y, button.width, button.height, chosen ? kChosen : kEdge);
      text(button.step < 0 ? "<" : ">", button.x, button.y, button.width, button.height,
           scale * 1.4f, chosen ? kChosen : kWords);
      continue;
    }
    const bool play = button.starts == StartMenu::Outcome::Play;
    fillRect(button.x, button.y, button.width, button.height, play ? kPlay : kWatch);
    if (chosen) {
      outline(button.x, button.y, button.width, button.height, kChosen);
      outline(button.x + 1, button.y + 1, button.width - 2, button.height - 2, kChosen);
    }
    text(play ? "Play" : "Watch", button.x, button.y, button.width, button.height, scale * 1.6f,
         chosen ? kChosen : kWords);
  }
  // Each row's label (left) and value (between its arrows)
  for (int r = 0; r < 3; ++r) {
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

} // namespace CosmicHex::Handheld
