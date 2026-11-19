#include <CosmicHex/Handheld/MenuPicture.hpp>
#include <CosmicHex/Handheld/Palette.hpp>

#include <ScratchPadd/Scene/OverlayText.hpp>

#include <algorithm>
#include <cmath>

namespace CosmicHex::Handheld {

namespace {
using Colour = ScratchPadd::Scene::Colour;
constexpr Colour kBackground{16, 24, 42, 255};
constexpr Colour kButton{29, 42, 69, 255};
constexpr Colour kEdge{90, 107, 140, 255};
constexpr Colour kChosen = Palette::kHighlight;
constexpr Colour kWords = Palette::kSoftWords;
constexpr Colour kPlay{42, 90, 58, 255};
constexpr Colour kWatch{42, 58, 90, 255};
namespace Scene = ScratchPadd::Scene;

// A line's letters' own width, unscaled
float textWidth(const std::string &text) { return Scene::Overlay::inkOf(text).width; }
} // namespace

MenuPicture::MenuPicture(int width, int height)
    : width_(width), height_(height), canvas_(width, height) {}

void MenuPicture::text(const std::string &words, float x, float y, float w, float h, float scale,
                       Colour colour, bool centred) {
  // Centred by the letters' own pixels (Overlay::inkOf, as the overlay's
  // textCentred), down always and across when asked
  if (centred) {
    canvas_.textCentred(x + w / 2, y + h / 2, words, colour, scale);
    return;
  }
  const Scene::Overlay::Ink ink = Scene::Overlay::inkOf(words, scale);
  canvas_.text(x, y + (h - ink.height) / 2 - ink.top, words, colour, scale);
}

void MenuPicture::draw(const StartMenu &menu) {
  canvas_.clear(kBackground);
  const float scale = width_ >= 300 ? 1.5f : 1.0f; // (the 3DS's wider screen: bigger words)
  // The arrows and Start and Back, as a panel of buttons (ScratchPadd's
  // Buttons): the focused one outlined
  const auto buttons = menu.buttons(width_, height_);
  std::vector<ScratchPadd::Buttons::Button> panelButtons;
  std::optional<int> focused;
  std::vector<std::size_t> chosenArrows; // (both of the chosen row's: drawn after)
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    const StartMenu::Button &button = buttons[i];
    const bool chosen =
        menu.focus() == button.row &&
        (button.step != 0 || (button.starts == StartMenu::Outcome::Back) == menu.backChosen());
    // (the panel's focus is one button: Start or Back; the chosen row's two
    // arrows are both marked, as either changes its value)
    if (chosen && button.step == 0)
      focused = static_cast<int>(i);
    else if (chosen)
      chosenArrows.push_back(i);
    const bool start = button.starts == StartMenu::Outcome::Start;
    panelButtons.push_back(
        {.id = static_cast<int>(i),
         .area = {button.x, button.y, button.width, button.height},
         .label = button.step != 0 ? (button.step < 0 ? "<" : ">") : (start ? "Start" : "Back"),
         .face = button.step != 0 ? std::nullopt : std::optional(start ? kPlay : kWatch),
         .edge = button.step != 0 ? std::nullopt : std::optional(start ? kPlay : kWatch)});
  }
  ScratchPadd::Buttons::Panel panel;
  panel.set(std::move(panelButtons));
  panel.setFocus(focused);
  panel.draw(canvas_, {.face = kButton,
                       .edge = kEdge,
                       .words = kWords,
                       .focus = kChosen,
                       .labelScale = scale * 1.4f});
  for (const std::size_t i : chosenArrows) {
    const StartMenu::Button &b = buttons[i];
    canvas_.outline(b.x, b.y, b.width, b.height, kChosen);
    canvas_.textCentred(b.x + b.width / 2, b.y + b.height / 2, b.step < 0 ? "<" : ">", kChosen,
                        scale * 1.4f);
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
  canvas_.clear(kBackground);
  const float scale = width_ >= 300 ? 1.6f : 1.2f;
  menu.panel(width_, height_, kPlay)
      .draw(
          canvas_,
          {.face = kButton, .edge = kEdge, .words = kWords, .focus = kChosen, .labelScale = scale});
}

} // namespace CosmicHex::Handheld
