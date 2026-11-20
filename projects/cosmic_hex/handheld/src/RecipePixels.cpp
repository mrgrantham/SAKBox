#include <CosmicHex/Handheld/RecipePixels.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

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

std::vector<ScratchPadd::Scene::Colour>
recipePixels(const ScratchPadd::TextureRecipe::Recipe &recipe, int width, int height,
             ScratchPadd::TextureRecipe::Smoothing smoothing,
             ScratchPadd::TextureRecipe::Rows rows) {
  namespace TR = ScratchPadd::TextureRecipe;
  const TR::Image image = TR::draw(recipe, width, height, TR::Format::Rgba8, {}, smoothing, rows);
  std::vector<ScratchPadd::Scene::Colour> pixels(static_cast<std::size_t>(image.width) *
                                                 static_cast<std::size_t>(image.height));
  for (std::size_t p = 0; p < pixels.size(); ++p)
    pixels[p] = {image.pixels[p * 4], image.pixels[p * 4 + 1], image.pixels[p * 4 + 2],
                 image.pixels[p * 4 + 3]};
  return pixels;
}

std::vector<ScratchPadd::Scene::Colour>
shrunk(const std::vector<ScratchPadd::Scene::Colour> &pixels, int from, int to) {
  if (to <= 0 || from < to || pixels.size() < static_cast<std::size_t>(from) * from)
    return {};
  std::vector<ScratchPadd::Scene::Colour> out(static_cast<std::size_t>(to) * to);
  for (int y = 0; y < to; ++y)
    for (int x = 0; x < to; ++x) {
      // The source pixels this one covers (whole numbers: each at least one)
      const int x0 = x * from / to, x1 = std::max(x0 + 1, (x + 1) * from / to);
      const int y0 = y * from / to, y1 = std::max(y0 + 1, (y + 1) * from / to);
      std::uint32_t r = 0, g = 0, b = 0, a = 0, count = 0;
      for (int sy = y0; sy < y1; ++sy)
        for (int sx = x0; sx < x1; ++sx) {
          const auto &c = pixels[static_cast<std::size_t>(sy) * from + sx];
          r += c.r * c.a;
          g += c.g * c.a;
          b += c.b * c.a;
          a += c.a;
          ++count;
        }
      auto &o = out[static_cast<std::size_t>(y) * to + x];
      if (a == 0) {
        o = {0, 0, 0, 0};
        continue;
      }
      o = {static_cast<std::uint8_t>(r / a), static_cast<std::uint8_t>(g / a),
           static_cast<std::uint8_t>(b / a), static_cast<std::uint8_t>(a / count)};
    }
  return out;
}

} // namespace CosmicHex::Handheld
