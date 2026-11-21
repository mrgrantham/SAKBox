#include <CosmicHex/Handheld/GamePictures.hpp>

#include <CosmicHex/Handheld/RecipePixels.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;

void loadCardPictures(Scene::Renderer &renderer, const PlatformProfile &profile,
                      TopPictures &pictures, CloseUpSlots &closeUps) {
  // Resources bigger (the user's: room for their titles, and
  // the cards under the top one easy to see), action cards smaller: on
  // the DS 44 and 32 pixels wide, 8 and 2 KB each, all of them fitting
  // beside the tile faces in the 128 KB the textures share
  pictures.resourceWidth = profile.topWidth >= 400 ? 64 : 44;
  pictures.cardWidth = profile.topWidth >= 400 ? 44 : 32;
  const auto load = [&](Art::Card card, int width, float &uMax, float &vMax) {
    const int height = width * 52 / 63;
    const auto whole =
        recipePixels(Art::cardRecipeOf(card), width, width * 88 / 63, profile.recipeSmoothing);
    if (whole.empty())
      return Scene::Texture{};
    const std::vector<Scene::Colour> top(whole.begin(), whole.begin() + width * height);
    // (the band's colour, for its title's letters: dark on a light band)
    const int bandPixel = (width * 11 / 63) * width + width * 6 / 63;
    if (static_cast<int>(card) < kResources)
      pictures.resourceBands[static_cast<std::size_t>(card)] =
          top[static_cast<std::size_t>(bandPixel)];
    const PaddedPicture picture = padded(top, width, height);
    uMax = picture.uMax;
    vMax = picture.vMax;
    return renderer.load(Scene::TextureImage{picture.width, picture.height, picture.pixels});
  };
  float uMax = 1, vMax = 1;
  for (int r = 0; r < kResources; ++r)
    pictures.resources[static_cast<std::size_t>(r)] =
        load(static_cast<Art::Card>(r), pictures.resourceWidth, uMax, vMax);
  pictures.resourceSquare = renderer.load(pictureSquare(uMax, vMax).data());
  for (int c = kResources; c < Art::kCards; ++c)
    pictures.cards[static_cast<std::size_t>(c)] =
        load(static_cast<Art::Card>(c), pictures.cardWidth, uMax, vMax);
  pictures.pictureSquare = renderer.load(pictureSquare(uMax, vMax).data());
  // The close-ups' slots, blank until one's shown (closeUpTile and
  // closeUpCards draw into them: PictureSlot)
  const int size = profile.closeUpSize;
  closeUps.face.load(renderer, size, size);
  closeUps.back.load(renderer, profile.closeUpBackSize, profile.closeUpBackSize);
  for (ScratchPadd::PictureSlot &card : closeUps.cards)
    card.load(renderer, size, size);
  const int smallCardHeight = size * 52 / 63; // (whole pixels, as the card is drawn)
  pictures.closeUpCardSquare = renderer.load(
      pictureSquare(1, static_cast<float>(smallCardHeight) / static_cast<float>(size)).data());
  // The two backs, whole cards (the planets for resource cards, the rings
  // for action cards: the agreed designs), for cards held face down
  for (const Art::CardBack design : {Art::CardBack::Rings, Art::CardBack::Planets}) {
    const auto whole = recipePixels(Art::cardBackRecipe(design), pictures.cardWidth,
                                    pictures.cardWidth * 88 / 63, profile.recipeSmoothing);
    if (whole.empty())
      continue;
    const PaddedPicture picture = padded(whole, pictures.cardWidth, pictures.cardWidth * 88 / 63);
    pictures.backs[static_cast<std::size_t>(design)] =
        renderer.load(Scene::TextureImage{picture.width, picture.height, picture.pixels});
    pictures.backSquare = renderer.load(pictureSquare(picture.uMax, picture.vMax).data());
  }
}

void loadFacePictures(Scene::Renderer &renderer, const PlatformProfile &profile,
                      const BoardPicture *map, TopPictures &pictures) {
  for (int i = 0; i < Art::kFaces; ++i) {
    const auto face = static_cast<Art::Face>(i);
    // The map's, shrunk, when it's no smaller (the DS: 35 pixels to 32,
    // Half the start's drawing); else from its recipe
    const std::vector<Scene::Colour> pixels =
        map && map->wholeFaceSize() >= profile.faceSize
            ? shrunk(map->wholeFace(face), map->wholeFaceSize(), profile.faceSize)
            : recipePixels(Art::recipeOf(face), profile.faceSize, profile.recipeSmoothing);
    if (pixels.empty())
      continue; // (the tile shows white: a broken recipe, tested against on the Mac)
    pictures.faces[static_cast<std::size_t>(i)] =
        renderer.load(Scene::TextureImage{profile.faceSize, profile.faceSize, pixels});
  }
  // and the number chips, half a face's size
  const int chipSize = profile.faceSize / 2;
  for (int n = Art::kLowestChip; n <= Art::kHighestChip; ++n) {
    const std::vector<Scene::Colour> pixels =
        recipePixels(Art::chipRecipeOf(n), chipSize, profile.recipeSmoothing);
    if (pixels.empty())
      continue;
    pictures.chips[static_cast<std::size_t>(n)] =
        renderer.load(Scene::TextureImage{chipSize, chipSize, pixels});
  }
}

} // namespace CosmicHex::Handheld
