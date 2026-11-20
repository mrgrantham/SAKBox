#include <ScratchPadd/PictureSlot/PictureSlot.hpp>

#include <algorithm>

namespace ScratchPadd {

void PictureSlot::load(Scene::Renderer &renderer, int width, int height) {
  width_ = width;
  height_ = height;
  shows_.reset();
  // (blank: no picture-sized buffer made for it)
  texture_ = renderer.load({width, height, {}});
}

bool PictureSlot::show(Scene::Renderer &renderer, int id,
                       const std::function<std::vector<Scene::Colour>()> &draw) {
  // (one band, the whole picture)
  return show(renderer, id, [&](int, int) { return draw(); }, height_);
}

bool PictureSlot::show(
    Scene::Renderer &renderer, int id,
    const std::function<std::vector<Scene::Colour>(int first, int rows)> &drawBand, int bandRows) {
  if (shows_ == id)
    return true;
  shows_.reset();
  if (!texture_ || bandRows <= 0)
    return false;
  for (int first = 0; first < height_; first += bandRows) {
    const int rows = std::min(bandRows, height_ - first);
    std::vector<Scene::Colour> band = drawBand(first, rows);
    if (band.size() != static_cast<std::size_t>(width_) * static_cast<std::size_t>(rows) ||
        !renderer.replace(texture_, {width_, rows, std::move(band)}, first))
      return false;
  }
  shows_ = id;
  return true;
}

} // namespace ScratchPadd
