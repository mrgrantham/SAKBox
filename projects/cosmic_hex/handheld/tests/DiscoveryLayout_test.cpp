// The top screen's words, laid out on each handheld's screen in its own
// font: the discovery's words and tile (no two may overlap, and all stay
// on the screen), and the resource cards' names
#include <CosmicHex/Handheld/BoardView.hpp>
#include <CosmicHex/Handheld/GameWords.hpp>
#include <CosmicHex/Handheld/TopViews.hpp>

#include <ScratchPadd/Scene/Font.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;

namespace {

// A screen and the font its overlay draws in
struct Screen {
  std::string_view name;
  float width, height;
  const Scene::Font &font;
  Scene::LineHeight lineHeight;
};

float silkscreenLine(float scale) { return Scene::Font::silkscreen().lineHeight(scale); }
float orbitronLine(float scale) { return Scene::Font::orbitron().lineHeight(scale); }

const Screen kDs{"DS", 256, 192, Scene::Font::silkscreen(), silkscreenLine};
const Screen k3ds{"3DS", 400, 240, Scene::Font::orbitron(), orbitronLine};

// Where text's letters' own pixels are when Overlay::textCentred puts their
// middle on `box`'s middle
Scene::LineBox inkIn(const Screen &screen, const Scene::LineBox &box, std::string_view text,
                     float scale) {
  const float height = screen.font.ink(text, scale).height;
  return {box.middle() - height / 2, box.middle() + height / 2};
}

// Words with letters above and below the line ('g', 'y', capitals), as
// long as the game's
constexpr std::string_view kHeading = "Rigel discovers", kTitle = "Ancient ruins",
                           kGives = "Empty: an explorer badge";

class DiscoveryLayoutTest : public testing::TestWithParam<Screen> {};

TEST_P(DiscoveryLayoutTest, ItsWordsAndTileOverlapNothing) {
  const Screen &screen = GetParam();
  const DiscoveryLayout layout = discoveryLayout(screen.width, screen.height, screen.lineHeight);
  const Scene::LineBox heading = inkIn(screen, layout.heading, kHeading, layout.headingScale);
  const Scene::LineBox title = inkIn(screen, layout.title, kTitle, layout.titleScale);
  const Scene::LineBox gives = inkIn(screen, layout.gives, kGives, layout.givesScale);
  const Scene::LineBox tile{screen.height / 2 - layout.tileHeight / 2,
                            screen.height / 2 + layout.tileHeight / 2};
  EXPECT_GE(heading.top, 0);
  EXPECT_LE(heading.bottom, title.top);
  EXPECT_LE(title.bottom, tile.top);
  EXPECT_LE(tile.bottom, gives.top);
  EXPECT_LE(gives.bottom, screen.height);
}

TEST_P(DiscoveryLayoutTest, ItsLinesFitAcross) {
  const Screen &screen = GetParam();
  const DiscoveryLayout layout = discoveryLayout(screen.width, screen.height, screen.lineHeight);
  EXPECT_LE(screen.font.width(kHeading, layout.headingScale), screen.width);
  EXPECT_LE(screen.font.width(kTitle, layout.titleScale), screen.width);
  EXPECT_LE(screen.font.width(kGives, layout.givesScale), screen.width);
}

// The tile still the picture's main part: at least two fifths of the
// screen's height, at most the 55% it was given
TEST_P(DiscoveryLayoutTest, TheTileTakesMostOfWhatsLeft) {
  const Screen &screen = GetParam();
  const DiscoveryLayout layout = discoveryLayout(screen.width, screen.height, screen.lineHeight);
  EXPECT_GE(layout.tileHeight, 0.4f * screen.height);
  EXPECT_LE(layout.tileHeight, 0.55f * screen.height + 0.01f);
}

// The dim backdrop behind the tile, on every screen, even as it turns over:
// it once sat at a fixed distance, cutting through the DS's tile as it
// turned and in front of the 3DS's (smaller, under its font's taller lines),
// dimming it (the user's: the tile darkened as its name came in); and in
// front of the board, which it dims, past the camera's near plane
TEST_P(DiscoveryLayoutTest, TheBackdropIsBehindTheTileAndInFrontOfTheBoard) {
  const Screen &screen = GetParam();
  const DiscoveryLayout layout = discoveryLayout(screen.width, screen.height, screen.lineHeight);
  const Scene::Camera camera = gameCamera(0);
  const DiscoveryDepths depths =
      discoveryDepths(layout.tileHeight / screen.height, camera.fieldOfView);
  EXPECT_GT(depths.tile, camera.nearPlane);
  // (the tile's corners swing a radius (1) back as it turns over)
  EXPECT_GT(depths.backdrop, depths.tile + 1);
  constexpr float kBoardNearest = 9; // (BoardView.hpp: the board is nearer nowhere)
  EXPECT_LT(depths.backdrop, kBoardNearest);
}

// A resource card's name on the top screen (asked for at 1.5, scaled) as
// large as its card was laid out for, and inside the card: on the DS half
// again Silkscreen's size, as before Silkscreen (drawn at 1, it was much
// smaller: the user's)
TEST_P(DiscoveryLayoutTest, ResourceCardNamesAreAsLargeAsTheirCards) {
  const Screen &screen = GetParam();
  const float k = screen.width / 256;
  constexpr float kHomePayCard = 70; // (the smaller of the two big cards; choosing one is 84)
  for (int r = 0; r < CosmicHex::kResources; ++r) {
    const std::string_view name = GameWords::of(static_cast<CosmicHex::Resource>(r));
    EXPECT_LE(screen.font.ink(name, 1.5f * k).width, kHomePayCard * k) << name;
    EXPECT_GE(screen.font.ink(name, 1.5f * k).height, 1.4f * screen.font.ink(name, k).height)
        << name;
  }
}

INSTANTIATE_TEST_SUITE_P(OnEachHandheld, DiscoveryLayoutTest, testing::Values(kDs, k3ds),
                         [](const testing::TestParamInfo<Screen> &info) {
                           return std::string(info.param.name == "DS" ? "Ds" : "N3ds");
                         });

} // namespace
