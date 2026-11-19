// PlatformProfile.hpp - what the game does differently on each console, in
// one place (these were constants scattered through the app,
// each in its own #if). Each value is tuned to what that hardware can do,
// most of them measured: see the round each comes from. The app reads
// kThisPlatform; tests on the Mac check both profiles.
//
// The roadmap's "device profile" (Gregory, Game Engine Architecture 1.6:
// a platform independence layer): the rest of the code asks the profile
// rather than which console it's on.
#pragma once

#include <CosmicHex/Handheld/TableGame.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <cstddef>

namespace CosmicHex::Handheld {

struct PlatformProfile {
  const char *name;

  // The screens, in pixels, and the overlay's text on the top one
  int topWidth, topHeight, bottomWidth, bottomHeight;
  std::size_t charsPerLine;
  float lineHeight;

  // The bots: simulated games a move at most, how they score moves, and
  // the frame arena each simulated game's memory comes from
  int botIterations;
  TableSettings::Arithmetic botArithmetic;
  std::size_t frameArenaBytes;

  // Drawing: the tile faces' texture size and how smoothly recipes are
  // drawn (2 x 2 samples on the DS, a quarter of the work), the
  // title's stars, and whether an unchanged top screen is left as it is
  // (the DS's 3D hardware shows its last frame by itself: the
  // bots went from 5 games a move to 84 on a DS)
  int faceSize;
  ScratchPadd::TextureRecipe::Smoothing recipeSmoothing;
  int titleStars;
  bool holdStillFrames;
  // The most memory the bottom board's sharp tiles may take zoomed in
  // (BoardPicture::setZoomBudget): the DS's zoom 2 takes about 240 KB,
  // its zoom 3 about 530 KB, more than it has
  std::size_t boardZoomBytes;

  // The dice's physics: its fixed step (each proved fair by
  // dice_fairness over 20,000 throws at it), and the most steps a frame may
  // catch up
  double diceStep;
  int mostDiceSteps;
};

// The Nintendo DS: 67 MHz, no floating-point hardware, a 4 KB data cache,
// memory copies at about 13 MB/s
inline constexpr PlatformProfile kDsProfile{
    .name = "DS",
    .topWidth = 256,
    .topHeight = 192,
    .bottomWidth = 256,
    .bottomHeight = 192,
    .charsPerLine = 41, // (stb_easy_font, about 6 pixels a letter at most)
    .lineHeight = 10,
    .botIterations = 200,
    .botArithmetic = TableSettings::Arithmetic::FixedPoint,
    .frameArenaBytes = 64 * 1024,
    .faceSize = 32, // (VRAM bank B is small)
    .recipeSmoothing = ScratchPadd::TextureRecipe::Smoothing::Coarse,
    .titleStars = 110, // (two polygons each, of the DS's 2048)
    .holdStillFrames = true,
    .boardZoomBytes = 320 * 1024,
    .diceStep = 1.0 / 30,
    .mostDiceSteps = 2,
};

// The 3DS: 268 MHz with floating-point hardware and a GPU of its own (its
// frames must be sent, so it doesn't hold still ones)
inline constexpr PlatformProfile kThreeDsProfile{
    .name = "3DS",
    .topWidth = 400,
    .topHeight = 240,
    .bottomWidth = 320,
    .bottomHeight = 240,
    .charsPerLine = 64,
    .lineHeight = 11,
    .botIterations = 450, // (the user's: they reached 400 every move)
    .botArithmetic = TableSettings::Arithmetic::Double,
    .frameArenaBytes = 128 * 1024,
    .faceSize = 64,
    .recipeSmoothing = ScratchPadd::TextureRecipe::Smoothing::Fine,
    .titleStars = 260,
    .holdStillFrames = false,
    .boardZoomBytes = 8 * 1024 * 1024,
    .diceStep = 1.0 / 60,
    .mostDiceSteps = 4,
};

// The console this build is for (desktop builds of the handheld library,
// its tests, take the 3DS's)
#if defined(__NDS__)
inline constexpr const PlatformProfile &kThisPlatform = kDsProfile;
#else
inline constexpr const PlatformProfile &kThisPlatform = kThreeDsProfile;
#endif

} // namespace CosmicHex::Handheld
