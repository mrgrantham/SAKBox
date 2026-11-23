#include <CosmicHex/Handheld/GamePictures.hpp>

#include <CosmicHex/Handheld/CardFace.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>

#include <cmath>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;

void loadCardPictures(Scene::Renderer &renderer, const PlatformProfile &profile,
                      TopPictures &pictures, CloseUpSlots &closeUps) {
  // Resources bigger in Show resources' piles (the user's: room for their
  // titles, and the cards under the top one easy to see), action cards
  // smaller
  pictures.resourceWidth = profile.topWidth >= 400 ? 64 : 44;
  pictures.cardWidth = profile.topWidth >= 400 ? 44 : 32;
  // The cards in layers (CardFace): the one shapes texture every layer is
  // cut from, each card's layers as a mesh, and its icon, the only
  // picture (the part of it inside the art band: CardFace::iconOf), as
  // sharp as the biggest card shown (profile.cardIconWidth across)
  const std::vector<Scene::Colour> shapes = CardFace::shapesPixels();
  pictures.cardShapes =
      renderer.load(Scene::TextureImage{CardFace::kShapesSize, CardFace::kShapesSize, shapes});
  for (int c = 0; c < Art::kCards; ++c) {
    const auto card = static_cast<Art::Card>(c);
    const auto index = static_cast<std::size_t>(c);
    pictures.cardLayers[index] = renderer.load(CardFace::layersMesh(card).data());
    const Art::CardArea shown = CardFace::iconOf(card);
    const int iconWidth = profile.cardIconWidth;
    const int iconHeight =
        static_cast<int>(std::lround(static_cast<float>(iconWidth) * shown.height / shown.width));
    const std::vector<Scene::Colour> icon =
        CardFace::iconPixels(card, iconWidth, iconHeight, profile.recipeSmoothing);
    if (icon.empty())
      continue;
    const PaddedPicture picture = padded(icon, iconWidth, iconHeight);
    pictures.cardIcons[index] =
        renderer.load(Scene::TextureImage{picture.width, picture.height, picture.pixels});
    pictures.cardIconSquares[index] =
        renderer.load(pictureSquare(picture.uMax, picture.vMax).data());
  }
  pictures.pictureSquare = renderer.load(pictureSquare().data());
  // The tile close-ups' slots, blank until one's shown (closeUpTile draws
  // into them: PictureSlot)
  closeUps.face.load(renderer, profile.closeUpSize, profile.closeUpSize);
  closeUps.back.load(renderer, profile.closeUpBackSize, profile.closeUpBackSize);
  // The two backs, whole cards (the planets for resource cards, the rings
  // for action cards: the agreed designs), for cards held face down
  const int backHeight = CardFace::heightOf(pictures.cardWidth);
  for (const Art::CardBack design : {Art::CardBack::Rings, Art::CardBack::Planets}) {
    const auto whole = recipePixels(Art::cardBackRecipe(design), pictures.cardWidth, backHeight,
                                    profile.recipeSmoothing);
    if (whole.empty())
      continue;
    const PaddedPicture picture = padded(whole, pictures.cardWidth, backHeight);
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
