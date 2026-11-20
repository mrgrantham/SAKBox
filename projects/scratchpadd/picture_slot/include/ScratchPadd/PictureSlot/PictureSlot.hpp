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
  // Whether picture `id` is the one in it
  bool showing(int id) const { return shows_ == id; }
  Scene::Texture texture() const { return texture_; }

private:
  Scene::Texture texture_;
  int width_ = 0, height_ = 0;
  std::optional<int> shows_;
};

} // namespace ScratchPadd
