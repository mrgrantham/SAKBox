#include <ScratchPadd/PictureSlot/PictureSlot.hpp>

namespace ScratchPadd {

void PictureSlot::load(Scene::Renderer &renderer, int width, int height) {
  width_ = width;
  height_ = height;
  shows_.reset();
  texture_ = renderer.load(
      {width, height, std::vector<Scene::Colour>(static_cast<std::size_t>(width * height))});
}

bool PictureSlot::show(Scene::Renderer &renderer, int id,
                       const std::function<std::vector<Scene::Colour>()> &draw) {
  if (shows_ == id)
    return true;
  shows_.reset();
  if (!texture_)
    return false;
  std::vector<Scene::Colour> pixels = draw();
  if (pixels.size() != static_cast<std::size_t>(width_ * height_) ||
      !renderer.replace(texture_, {width_, height_, std::move(pixels)}))
    return false;
  shows_ = id;
  return true;
}

} // namespace ScratchPadd
