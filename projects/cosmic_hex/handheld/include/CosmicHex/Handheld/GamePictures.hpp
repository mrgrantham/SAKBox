// GamePictures.hpp - the pictures the top screen draws with, drawn from
// their recipes and loaded as textures, once at the start (the DS hands out
// its texture memory once, and has 256 KB of it): the tiles' faces and
// number chips, the cards' layers, icons and backs, and the close-ups' slots.
// GamePictures_test loads them all as the DS would, its texture memory
// emulated, so running out (grey models, cards gone) fails on the Mac.
//
//   loadFacePictures(renderer, kThisPlatform, &map, pictures);
//   loadCardPictures(renderer, kThisPlatform, pictures, closeUps);
#pragma once

#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/PlatformProfile.hpp>
#include <CosmicHex/Handheld/TopViews.hpp>

#include <ScratchPadd/PictureSlot/PictureSlot.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

namespace CosmicHex::Handheld {

// The close-ups' slots (PlatformProfile::closeUpSize): pictures shown big
// for a moment, drawn sharp when they're shown into slots loaded once and
// reused (PictureSlot): a discovered tile's face and its back (flying up
// face down: the user's). Cards need none: their icons are loaded sharp
struct CloseUpSlots {
  ScratchPadd::PictureSlot face, back;
};

// Each tile face (Art.hpp) drawn into a texture (profile.faceSize a side),
// from `map`'s whole-board faces where they're no smaller (shrunk), else
// from its recipe; and the number chips, half that size
void loadFacePictures(ScratchPadd::Scene::Renderer &renderer, const PlatformProfile &profile,
                      const BoardPicture *map, TopPictures &pictures);
// The cards in layers (CardFace: each card's layers mesh, the shapes
// texture they're cut from, each card's icon; no words: they're the
// overlay's), their backs, the squares they're drawn on, and the tile
// close-ups' slots, blank until one's shown
void loadCardPictures(ScratchPadd::Scene::Renderer &renderer, const PlatformProfile &profile,
                      TopPictures &pictures, CloseUpSlots &closeUps);

} // namespace CosmicHex::Handheld
