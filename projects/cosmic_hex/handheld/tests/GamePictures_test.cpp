// The game's pictures loaded as each console would, its texture memory
// emulated on a desktop renderer (Renderer::setTextureBudget): every one
// loads, with room left for the toon textures meshes make as they're first
// drawn (the DS ran out: the title's rocket grey, the cards gone)
#include <CosmicHex/Handheld/CardFace.hpp>
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
  // (every card in its layers: its mesh, the one shapes texture, its icon)
  EXPECT_TRUE(pictures.cardShapes);
  for (int c = 0; c < Art::kCards; ++c) {
    EXPECT_TRUE(pictures.cardLayers[static_cast<std::size_t>(c)]) << "card " << c;
    EXPECT_TRUE(pictures.cardIcons[static_cast<std::size_t>(c)]) << "card " << c;
  }
  // (every resource card's title dark on its pale band: the user's, white
  // was hard to read)
  for (int r = 0; r < kResources; ++r)
    EXPECT_EQ(CardFace::titleInkOf(static_cast<Art::Card>(r)), Palette::kInkOnLightBand)
        << "resource " << r;
  for (const Scene::Texture back : pictures.backs)
    EXPECT_TRUE(back);
  EXPECT_TRUE(closeUps.face.texture());
  EXPECT_TRUE(closeUps.back.texture());
  const auto memory = renderer.textureMemory();
  ASSERT_TRUE(memory.has_value());
  if (memory) {
    std::printf("DS texture memory: %u KB used, %u KB left\n", memory->used() / 1024,
                memory->left() / 1024);
    EXPECT_GE(memory->left(), kToonRoom);
  }
}

// The 3DS's pictures, counted as its renderer keeps them (RGBA8, 4 bytes
// a texel, before their mipmaps): it has room to spare, so this only
// reports what they take
TEST(GamePicturesTest, TheThreeDsPicturesMemory) {
  Scene::Display display(400, 240, "GamePictures_test", false);
  if (!display.isOpen())
    SKIP_OR_FAIL_WITHOUT("display", "no window to draw in: " + display.error());
  Scene::Renderer renderer(display);
  constexpr std::uint32_t kPlenty = 64 * 1024 * 1024;
  renderer.setTextureBudget({.slotBytes = kPlenty, .slots = 1, .bytesPerTexel = 4});
  TopPictures pictures;
  CloseUpSlots closeUps;
  loadFacePictures(renderer, kThreeDsProfile, nullptr, pictures);
  loadCardPictures(renderer, kThreeDsProfile, pictures, closeUps);
  const auto memory = renderer.textureMemory();
  ASSERT_TRUE(memory.has_value());
  if (memory)
    std::printf("3DS texture memory: %u KB used\n", memory->used() / 1024);
}
