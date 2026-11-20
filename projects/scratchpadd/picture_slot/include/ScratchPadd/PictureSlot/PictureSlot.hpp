// PictureSlot.hpp - a texture kept for pictures that come and go: a
// close-up drawn sharp when it's shown (a card turned over big, a tile held
// up to the camera), into the same texture each time. Made for the DS,
// which hands out texture memory once and never takes it back; elsewhere
// it saves loading and unloading.
//
//   PictureSlot closeUp;
//   closeUp.load(renderer, 64, 64);                 // once, blank
//   closeUp.show(renderer, cardId, [&] { return pixelsOf(cardId); });
//   if (closeUp.showing(cardId)) draw with closeUp.texture()
//
// Or a band of rows at a time, for a device short of memory (a DS draws a
// 128-pixel picture 16 rows at a time; elsewhere the band is the whole
// picture, as above):
//   closeUp.show(renderer, cardId, [&](int first, int rows) { ... }, 16);
//
// show() draws only when the picture asked for isn't the one in the slot
// already (drawing a picture can take a tenth of a second on a DS: do it
// while nothing on screen moves).
#pragma once

#include <ScratchPadd/Scene/Scene.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace ScratchPadd {

class PictureSlot {
public:
  // A blank texture `width` x `height`, the slot's for good
  void load(Scene::Renderer &renderer, int width, int height);
  int width() const { return width_; }
  int height() const { return height_; }

  // Picture `id` in the slot: drawn by `draw` (width x height, rows from
  // the top) unless it's there already. False if it couldn't be (no slot,
  // or `draw` gave none): the caller shows its usual picture
  bool show(Scene::Renderer &renderer, int id,
            const std::function<std::vector<Scene::Colour>()> &draw);
  // The same, drawn and sent a band at a time: `drawBand(first, rows)`
  // gives rows `first` to `first + rows` (width x rows), `bandRows` at a
  // time (the last band shorter), so only a band is ever in memory
  bool show(Scene::Renderer &renderer, int id,
            const std::function<std::vector<Scene::Colour>(int first, int rows)> &drawBand,
            int bandRows);
  // Whether picture `id` is the one in it
  bool showing(int id) const { return shows_ == id; }
  Scene::Texture texture() const { return texture_; }

private:
  Scene::Texture texture_;
  int width_ = 0, height_ = 0;
  std::optional<int> shows_;
};

} // namespace ScratchPadd
