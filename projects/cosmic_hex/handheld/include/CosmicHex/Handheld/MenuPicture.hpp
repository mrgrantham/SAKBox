// MenuPicture.hpp - the start menu (StartMenu) drawn into pixels, for a
// console's touch screen, as BoardPicture draws the board: each row's label,
// its value between two arrow buttons, Play and Watch, the chosen row
// outlined. Text is stb_easy_font's (OverlayText), filled in as pixels.
//
//   MenuPicture picture(320, 240);
//   picture.draw(menu);
//   picture.pixels()  // RGBA, rows from the top
#pragma once

#include <CosmicHex/Handheld/ButtonMenu.hpp>
#include <CosmicHex/Handheld/StartMenu.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>

#include <ScratchPadd/Scene/Types.hpp>

#include <span>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

class MenuPicture {
public:
  MenuPicture(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }

  void draw(const StartMenu &menu);
  void draw(const ButtonMenu &menu); // (the title's and the pause menu's buttons)
  std::span<const ScratchPadd::Scene::Colour> pixels() const { return canvas_.pixels(); }

private:
  // `text` centred in a box, or from its left when `centred` is false
  void text(const std::string &text, float x, float y, float w, float h, float scale,
            ScratchPadd::Scene::Colour colour, bool centred = true);

  int width_, height_;
  ScratchPadd::Canvas canvas_;
};

} // namespace CosmicHex::Handheld
