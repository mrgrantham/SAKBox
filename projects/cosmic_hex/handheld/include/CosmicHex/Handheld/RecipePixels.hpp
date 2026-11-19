// RecipePixels.hpp - a texture recipe drawn as a square of colours: the
// tile faces and number chips, for the 3D board's textures and the flat
// map alike (round 92: the app and BoardPicture each had their own copy)
#pragma once

#include <ScratchPadd/Scene/Types.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <string_view>
#include <vector>

namespace CosmicHex::Handheld {

// `recipe` (TextureRecipe's text) drawn `size` pixels a side, as RGBA
// colours in rows; none if it's empty or broken (recipes are tested on the
// Mac, so a broken one is a bug there)
std::vector<ScratchPadd::Scene::Colour> recipePixels(
    std::string_view recipe, int size,
    ScratchPadd::TextureRecipe::Smoothing smoothing = ScratchPadd::TextureRecipe::Smoothing::Fine);
// The same `width` x `height` (a card's 63 x 88 shape, say)
std::vector<ScratchPadd::Scene::Colour> recipePixels(
    std::string_view recipe, int width, int height,
    ScratchPadd::TextureRecipe::Smoothing smoothing = ScratchPadd::TextureRecipe::Smoothing::Fine);

// A square picture `from` pixels a side made `to` a side (no bigger), each
// pixel the average of those it covers, weighted by how opaque they are (so
// see-through pixels don't darken an edge): for a picture already drawn at
// one size wanted at a smaller one, quicker than its recipe (round 105)
std::vector<ScratchPadd::Scene::Colour>
shrunk(const std::vector<ScratchPadd::Scene::Colour> &pixels, int from, int to);

} // namespace CosmicHex::Handheld
