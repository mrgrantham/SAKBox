// ChoiceScreens.hpp - the bottom screen's choosing screens drawn on a
// Canvas (round 101): the build screen (round 92's B: a row for each thing,
// its cost, and why not in red), the resource picker and the player picker
// (round 100's 1 and 2). Each says what a touch is on, so the app needn't
// know their layout; sizes follow the screen (256 wide on a DS, in
// proportion on a 3DS).
//
//   ChoiceScreens::drawBuild(canvas, buildRows(state), chosen);
//   const auto touched = ChoiceScreens::buildTouch(canvas.width(), rows.size(), x, y);
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Handheld/BuildMenu.hpp>
#include <CosmicHex/Handheld/Pickers.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>

#include <array>
#include <optional>
#include <vector>

namespace CosmicHex::Handheld {

using ScratchPadd::Canvas; // (ScratchPadd/Canvas)

// The cards' small pictures for a screen `screenWidth` wide, drawn from
// their recipes once each, when first needed (on a DS each takes a tenth of
// a second or so: prepare() draws one at a time, in spare frames)
class CardSprites {
public:
  explicit CardSprites(int screenWidth);
  // A small card: the drawing down to its picture's foot, its short title
  // in the band (dark letters on a light band), `width` x height()
  const std::vector<Canvas::Colour> &small(Art::Card card);
  // A card's back, the same width, the whole card
  const std::vector<Canvas::Colour> &back(Art::CardBack design);
  int width() const { return width_; }
  int height() const { return height_; }         // a small card's
  int backHeight() const { return backHeight_; } // a whole card's
  // Draws one not yet drawn; whether they're all ready
  bool prepare();

private:
  int width_, height_, backHeight_;
  std::array<std::vector<Canvas::Colour>, Art::kCards> small_;
  std::array<std::vector<Canvas::Colour>, 2> backs_;
};

namespace ChoiceScreens {

// A touch on one of these screens: a row or a card or a player (its
// index), or the bottom buttons (found from the screen's width alone: the
// layouts scale with it)
struct Touch {
  enum class What { Item, Back, Confirm } what;
  int item = -1;
  int card = -1; // (the player picker: which of their cards)
};

// The build screen: a row each (Ship, Station, Colony, Card) with its cost
// as a small tile in each resource's colour for each one it takes, its note
// in red when it can't be built, `chosen` outlined; Back, and Build when
// the chosen row can be built
void drawBuild(Canvas &canvas, const std::vector<BuildRow> &rows, int chosen);
std::optional<Touch> buildTouch(int width, std::size_t rows, int x, int y);

// The resource picker: the five resource cards with how many you hold
// under each, those picked outlined (a resource picked twice, twice);
// Take once enough are picked, Back when it can be left
// (`focus`: the D-pad's card, underlined; -1 for none)
void drawResources(Canvas &canvas, const Pickers::ResourceChoice &choice,
                   const std::vector<Resource> &picked, const Resources &held, bool canGoBack,
                   CardSprites &cards, int focus = -1);
std::optional<Touch> resourcesTouch(int width, int x, int y);

// The player picker: a row for each player the rules allow, their name in
// their colour and their cards face down; the card tapped slides up (round
// 101, the user's: for the feel of picking one; which resource it is stays
// the rules' draw) and Steal shows
void drawPlayers(Canvas &canvas, const Pickers::PlayerChoice &choice, int chosen, int chosenCard,
                 Art::CardBack backs, CardSprites &cards);
std::optional<Touch> playersTouch(int width, std::size_t players, int x, int y);

} // namespace ChoiceScreens

} // namespace CosmicHex::Handheld
