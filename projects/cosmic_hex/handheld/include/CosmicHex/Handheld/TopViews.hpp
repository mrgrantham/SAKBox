// TopViews.hpp - drawing what the top screen shows over the board for a
// while (TopShows.hpp's shows): a discovered tile, the two cards dealt,
// another player's panel, who goes first. Each draws into a TopFrame: the
// renderer, its overlay and camera, the screen's size, the pictures loaded
// once (TopPictures) and the players' names.
//
//   TopFrame frame{renderer, overlay, camera, 256, 192, pictures, names, now};
//   drawDiscovery(frame, discovery, closeUpFace);
//
// The board itself and the HUD are the app's. Pictures over the board are
// flat squares just in front of the camera (ScreenPictures.hpp), so the
// overlay's words (always on top) go over them.
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Handheld/CardChoices.hpp>
#include <CosmicHex/Handheld/OtherPlayers.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/PlayerNames.hpp>
#include <CosmicHex/Handheld/RollDice.hpp>
#include <CosmicHex/Handheld/TopShows.hpp>

#include <ScratchPadd/PictureSlot/PictureSlot.hpp>
#include <ScratchPadd/Scene/LineStack.hpp>
#include <ScratchPadd/Scene/Overlay.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <array>

namespace CosmicHex::Handheld {

// The pictures the views draw with, loaded once by the app
struct TopPictures {
  ScratchPadd::Scene::Mesh hexTile, numberChip, column; // (the board's own pieces)
  ScratchPadd::Scene::Mesh die, homeDie;                // (the dice's, DicePieces)
  // Flat squares, each with its pictures' texture corners (ScreenPictures)
  ScratchPadd::Scene::Mesh pictureSquare, backSquare, closeUpCardSquare;
  std::array<ScratchPadd::Scene::Texture, Art::kFaces> faces{};
  std::array<ScratchPadd::Scene::Texture, Art::kHighestChip + 1> chips{}; // (by number)
  std::array<ScratchPadd::Scene::Texture, Art::kCards> cards{};           // (action cards')
  std::array<ScratchPadd::Scene::Texture, kResources> resources{};        // (resource cards')
  ScratchPadd::Scene::Mesh resourceSquare;                                // (and their square)
  std::array<ScratchPadd::Scene::Texture, 2> backs{};                     // (by Art::CardBack)
  // The cards' pictures' widths (resource cards bigger: their titles), and
  // each card's label band colour (by Art::Card: resources first)
  int resourceWidth = 44, cardWidth = 32;
  std::array<ScratchPadd::Scene::Colour, Art::kCards> cardBands{};
  // A card's title letters on its band (Palette::inkOnBand), for every
  // view that writes a title over a card's picture
  ScratchPadd::Scene::Colour titleInkOf(Art::Card card) const {
    return Palette::inkOnBand(cardBands[static_cast<std::size_t>(card)]);
  }
};

// One frame's drawing
struct TopFrame {
  ScratchPadd::Scene::Renderer &renderer;
  ScratchPadd::Scene::Overlay &overlay;
  const ScratchPadd::Scene::Camera &camera;
  float width, height; // (the screen, in pixels)
  const TopPictures &pictures;
  const PlayerNames &names;
  double now;                             // (the shows' clock)
  float k() const { return width / 256; } // (sizes are for a DS's screen, scaled)
};

// Where a discovery's words and its tile go on a screen `width` x `height`
// (points), in the font `lineHeight` measures: who discovered it and the
// tile's name stacked down from the top, the tile facing you in the middle
// as tall as fits under them (55% of the screen at most), and what it
// gives just under the tile
struct DiscoveryLayout {
  float headingScale = 1, titleScale = 2, givesScale = 1.5f;
  ScratchPadd::Scene::LineBox heading, title, gives;
  float tileHeight = 0; // (from its top corner to its bottom one, facing you)
};
DiscoveryLayout discoveryLayout(
    float width, float height,
    ScratchPadd::Scene::LineHeight lineHeight = ScratchPadd::Scene::Overlay::lineHeight);

// A discovered tile flying up from the board, turned over and set back
// (its face from `closeUp` when that holds it, and its back from
// `closeUpBack`: PictureSlot ids, the faces), its words placed by
// discoveryLayout
void drawDiscovery(const TopFrame &frame, const DiscoveryShow &show,
                   const ScratchPadd::PictureSlot &closeUp,
                   const ScratchPadd::PictureSlot &closeUpBack);
// The two cards dealt, thrown in and turned, the one `chosen` below
// outlined (`closeUps`: the cards' pictures drawn sharp, in order)
void drawDealtCards(const TopFrame &frame, const DealtShow &show, int chosen,
                    const std::array<ScratchPadd::PictureSlot, 2> &closeUps);
// Show resources: `player`'s resources as piles along the bottom (a card
// for each, five at most, a count past five), their action cards across
// the top, each card coming in as `show` has it
void drawResources(const TopFrame &frame, const Player &player, const CardsShow &show);
// What everyone can see of a player, on a panel sliding down from the top
// (`across`: how far it's slid sideways, in screen widths: the carousel)
void drawOtherPlayer(const TopFrame &frame, const PlayerView &view, float down, float across = 0);
// An action card played or a Charter revealed (CardPlayShow): rising from
// its player, turning over, held big to be read, then away
void drawCardPlay(const TopFrame &f, const CardPlayShow &show, const State &state, int you);

// Choosing a resource (PickShow): the card in focus big, sliding in as the
// picker's focus moves, `title` over it ("Your home pays 1"); under it what
// you could build, each dimmed when even with this card you couldn't
// (`held`: your resources now; `settings`: the costs)
void drawPick(const TopFrame &f, const PickShow &show, std::string_view title,
              const Resources &held, const Settings &settings, ScratchPadd::Scene::Colour yours);

// Cards gained (HandShow): your piles, `held` (the new cards counted in),
// rising into view, the new ones dropping into the backs of their piles
void drawGained(const TopFrame &f, const Resources &held, const HandShow &show);

// A roll's dice on the glass and, once they settle, the roll popping in
// over them (RollDice)
void drawRoll(const TopFrame &f, const RollDice &dice);

// Who goes first: each player's die in the glass in their colour, their
// numbers by their names as they land, then the order by name
void drawFirstRoll(const TopFrame &frame, const FirstRoll &roll, const FirstRollShow &show);
// What a roll pays: each card out of its planet and off along an arc to
// its owner (`you`, playing: down to the bottom screen; a bot's, or
// everyone's when watching: up to their row of the points bars)
void drawRollPay(const TopFrame &frame, const RollPayShow &show, const State &state, int you);
// A home's pay: its die out of the home on `state`'s board, its face, the
// card big (`closeUp` when it holds the resource's card: PictureSlot id,
// the card), then off to the hand (yours: `yours`, down to the bottom
// screen; a bot's, up and away)
void drawHomePay(const TopFrame &frame, const HomePayShow &show, const State &state, bool yours,
                 const ScratchPadd::PictureSlot &closeUp);

} // namespace CosmicHex::Handheld
