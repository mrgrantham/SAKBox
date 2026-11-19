#include <CosmicHex/Handheld/RecipePixels.hpp>

#include <cstddef>

namespace CosmicHex::Handheld {

std::vector<ScratchPadd::Scene::Colour>
recipePixels(std::string_view recipe, int size, ScratchPadd::TextureRecipe::Smoothing smoothing) {
  return recipePixels(recipe, size, size, smoothing);
}

std::vector<ScratchPadd::Scene::Colour>
recipePixels(std::string_view recipe, int width, int height,
             ScratchPadd::TextureRecipe::Smoothing smoothing) {
  namespace TR = ScratchPadd::TextureRecipe;
  const auto read = TR::read(recipe);
  if (recipe.empty() || !read)
    return {};
  const TR::Image image = TR::draw(*read, width, height, TR::Format::Rgba8, {}, smoothing);
  std::vector<ScratchPadd::Scene::Colour> pixels(static_cast<std::size_t>(width) *
                                                 static_cast<std::size_t>(height));
  for (std::size_t p = 0; p < pixels.size(); ++p)
    pixels[p] = {image.pixels[p * 4], image.pixels[p * 4 + 1], image.pixels[p * 4 + 2],
                 image.pixels[p * 4 + 3]};
  return pixels;
}

} // namespace CosmicHex::Handheld
