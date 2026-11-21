// The game's pictures loaded as each console would, its texture memory
// emulated on a desktop renderer (Renderer::setTextureBudget): every one
// loads, with room left for the toon textures meshes make as they're first
// drawn (the DS ran out: the title's rocket grey, the cards gone)
#include <CosmicHex/Handheld/GamePictures.hpp>

#include <ExpectedCapabilities/ExpectedCapabilities.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;

namespace {
// Room the toon textures need once the game runs: 128 bytes a mesh and
// colour (the board's pieces in four players' colours, the dice, the title)
constexpr std::uint32_t kToonRoom = 8 * 1024;
} // namespace

TEST(GamePicturesTest, TheDsPicturesFitItsTextureMemory) {
  Scene::Display display(256, 192, "GamePictures_test", false);
  if (!display.isOpen())
    SKIP_OR_FAIL_WITHOUT("display", "no window to draw in: " + display.error());
  Scene::Renderer renderer(display);
  renderer.setTextureBudget(Scene::TextureMemory::ds());
  TopPictures pictures;
  CloseUpSlots closeUps;
  loadFacePictures(renderer, kDsProfile, nullptr, pictures);
  loadCardPictures(renderer, kDsProfile, pictures, closeUps);
  for (const Scene::Texture face : pictures.faces)
    EXPECT_TRUE(face);
  for (const Scene::Texture card : pictures.resources)
    EXPECT_TRUE(card);
  // (every resource card's title dark on its pale band: the user's, white
  // was hard to read)
  for (int r = 0; r < kResources; ++r)
    EXPECT_EQ(pictures.titleInkOf(static_cast<Art::Card>(r)), Palette::kInkOnLightBand)
        << "resource " << r;
  for (int c = kResources; c < Art::kCards; ++c) {
    EXPECT_TRUE(pictures.cards[static_cast<std::size_t>(c)]) << "card " << c;
  }
  for (const Scene::Texture back : pictures.backs)
    EXPECT_TRUE(back);
  EXPECT_TRUE(closeUps.face.texture());
  EXPECT_TRUE(closeUps.back.texture());
  for (const auto &card : closeUps.cards)
    EXPECT_TRUE(card.texture());
  const auto memory = renderer.textureMemory();
  ASSERT_TRUE(memory.has_value());
  if (memory) {
    std::printf("DS texture memory: %u KB used, %u KB left\n", memory->used() / 1024,
                memory->left() / 1024);
    EXPECT_GE(memory->left(), kToonRoom);
  }
}
