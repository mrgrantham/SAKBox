// TopViews.hpp - drawing what the top screen shows over the board for a
// while (TopShows.hpp's shows): a discovered tile, the two cards dealt,
// another player's panel, who goes first. Each draws into a TopFrame: the
// renderer, its overlay and camera, the screen's size, the pictures loaded
// once (TopPictures) and the players' names. Every card is drawn one way
// (TopViews.cpp's cardFace: CardFace's layers, its icon, its words).
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
#include <string>
#include <string_view>

namespace CosmicHex::Handheld {

// The pictures the views draw with, loaded once by the app
struct TopPictures {
  ScratchPadd::Scene::Mesh hexTile, numberChip, column, hexRing; // (the board's own pieces)
  ScratchPadd::Scene::Mesh die, homeDie;                         // (the dice's, DicePieces)
  // Flat squares, each with its pictures' texture corners (ScreenPictures)
  ScratchPadd::Scene::Mesh pictureSquare, backSquare;
  std::array<ScratchPadd::Scene::Texture, Art::kFaces> faces{};
  std::array<ScratchPadd::Scene::Texture, Art::kHighestChip + 1> chips{}; // (by number)
  // The cards, in layers (CardFace): each card's layers, one mesh; the
  // rounded rectangles they're cut from, one small texture; and each card's
  // icon, its only picture (all by Art::Card, resources first)
  std::array<ScratchPadd::Scene::Mesh, Art::kCards> cardLayers{};
  ScratchPadd::Scene::Texture cardShapes;
  std::array<ScratchPadd::Scene::Texture, Art::kCards> cardIcons{};
  std::array<ScratchPadd::Scene::Texture, 2> backs{}; // (by Art::CardBack)
  // The piles' cards' widths (Show resources: resource cards bigger, their
  // titles on them; action cards smaller, their titles under them)
  int resourceWidth = 44, cardWidth = 32;
};

// A point on the top screen, in points from its top left
struct ScreenPoint {
  float x = 0, y = 0;
};

// Where the views' big cards rest on a screen `width` x `height` (points,
// sizes for a DS's scaled by width / 256): each a true card's shape
// (CardFace, 2:3), as big as fits beside the words round it, whole on the
// screen (the 3DS's is shorter for its width)
struct CardBox {
  float x = 0, y = 0, width = 0, height = 0;
};
// The cards dealt, card `i` of `count` side by side, over the hint
CardBox dealtCardBox(float width, float height, std::size_t i, std::size_t count);
// An action card played, held in the middle under its words
CardBox playedCardBox(float width, float height);
// The resource picker's card in focus, between its title and what you
// could build
CardBox pickCardBox(float width, float height);
// A home's card, held under its die, over what it gives
CardBox homePayCardBox(float width, float height);

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

// How far in front of the camera a discovered tile faces you (its corners
// `tileRadius` from its middle, `tileShare` of the screen's height, seen
// through a vertical `fieldOfView` in degrees), and its dim backdrop: always
// behind the tile, so it dims the board and never the tile
struct DiscoveryDepths {
  float tile = 0, backdrop = 0;
};
DiscoveryDepths discoveryDepths(float tileShare, float fieldOfView, float tileRadius = 1);

// A discovered tile flying up from the board, turned over and set back
// (its face from `closeUp` when that holds it, and its back from
// `closeUpBack`: PictureSlot ids, the faces), its words placed by
// discoveryLayout
void drawDiscovery(const TopFrame &frame, const DiscoveryShow &show,
                   const ScratchPadd::PictureSlot &closeUp,
                   const ScratchPadd::PictureSlot &closeUpBack);
// The two cards dealt, thrown in and turned, the one `chosen` below
// outlined
void drawDealtCards(const TopFrame &frame, const DealtShow &show, int chosen);
// Show resources: `player`'s resources as piles along the bottom (a card
// for each, five at most, a count past five), their action cards across
// the top, each card coming in as `show` has it
void drawResources(const TopFrame &frame, const Player &player, const CardsShow &show);
// How wide a line of text is at a scale, in points
using TextWidth = float (*)(std::string_view text, float scale);
// How a layout worked out before drawing measures text: the platform's
// font (the overlay's) unless another is asked for (a test's)
struct TextMeasure {
  ScratchPadd::Scene::LineHeight lineHeight = ScratchPadd::Scene::Overlay::lineHeight;
  TextWidth width = ScratchPadd::Scene::Overlay::textWidth;
};

// Where another player's panel puts what it shows on a screen `width` x
// `height` (points from the panel's top), worked out from the screen's
// size and the font's lines the same way on every screen: `name` and
// their points down the left; their resource and action cards face down
// in two columns beside them, fanned, six a row, a second row under the
// first past six; their badges and pieces under the cards; their planets,
// the label beside the first row; their Charters along the bottom. The
// words, the pieces and the planets keep their size; the cards take the
// room left between the header and the pieces (as big as a DS's 36 at
// most), and a row fans out when it's short, closing up as it fills.
struct OtherPlayerLayout {
  static constexpr int kCardsARow = 6, kCardsShown = 12, kPlanetRows = 2;
  float textScale = 1, nameScale = 1.5f, labelScale = 1, chartersScale = 1;
  float unit = 1;   // (a DS's point for shapes and room: the screen's scale both ways)
  float margin = 0; // (the panel's lines in from the screen's edges)
  float frameTop = 0, frameBottom = 0; // (the panel's two lines)
  float left = 0, right = 0;           // (what's on it, across)
  ScratchPadd::Scene::LineBox name, points;
  // The cards: each column's left, the room each has across, the labels'
  // line over them, the first row's top, a card's size and the rows' step
  float resourcesX = 0, actionsX = 0, columnWidth = 0;
  std::string resourcesLabel, actionsLabel; // ("RESOURCES 9"; a shorter word where that won't fit)
  ScratchPadd::Scene::LineBox cardLabels;
  float cardsTop = 0, cardWidth = 0, cardHeight = 0, rowStep = 0;
  int cardRows = 1;
  float cardsBottom() const;
  // How far apart a row of `inRow` cards fans: wide when there are few,
  // closer as it fills, never past the column
  float fanStep(int inRow) const;
  // The pieces: each group's left (badges, ships, stations, colonies), the
  // icons' middle and half their height, a count's line on them, the
  // groups' names under them
  std::array<float, 4> piecesX{};
  float piecesMiddle = 0, pieceHalf = 0;
  ScratchPadd::Scene::LineBox pieceCounts, pieceNames;
  // The planets: their label (on the first row), the first hexagon's
  // middle, the hexagons' corner radius and steps, how many a row and
  // how many show (past what fits, the last place says how many more)
  ScratchPadd::Scene::LineBox planetsLabel;
  float planetsX = 0, planetsY = 0, planetRadius = 0, planetStep = 0, planetRowStep = 0;
  int planetsARow = 1, planetsShown = 0;
  float planetsBottom() const;
  // The Charters' line (none: an empty box at the bottom)
  ScratchPadd::Scene::LineBox charters;
};
OtherPlayerLayout otherPlayerLayout(const PlayerView &view, std::string_view name, float width,
                                    float height, TextMeasure measure = {});
// "Charters: ..." for the Charters a player has revealed (empty for none)
std::string chartersLine(const PlayerView &view);

// What everyone can see of a player, on a panel sliding down from the top
// (`across`: how far it's slid sideways, in screen widths: the carousel),
// laid out by otherPlayerLayout
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
// A home's pay: its die, the home's tile on `state`'s board, pulled out
// of its place (left empty space, rippling as it goes) to face you, spun
// to its side for each pay; each card big, then off to its player
// (CardPlaces: yours, `yours`, down to the bottom screen; a bot's, to
// their row of the points bars); then the die set back in its place, space
// rippling where it seats
void drawHomePay(const TopFrame &frame, const HomePayShow &show, const State &state, bool yours);

} // namespace CosmicHex::Handheld
