// ChoiceScreens.hpp - the bottom screen's choosing screens drawn on a
// Canvas: the build screen, the resource picker and the player picker
// Each says what a touch is on, so the app needn't
// know their layout; sizes follow the screen (256 wide on a DS, in
// proportion on a 3DS).
//
//   ChoiceScreens::drawBuild(canvas, buildRows(state), chosen);
//   const auto touched = ChoiceScreens::buildTouch(canvas.width(), rows.size(), x, y);
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Handheld/BuildMenu.hpp>
#include <CosmicHex/Handheld/CardChoices.hpp>
#include <CosmicHex/Handheld/OtherPlayers.hpp>
#include <CosmicHex/Handheld/Pickers.hpp>
#include <CosmicHex/Handheld/PlayerNames.hpp>
#include <CosmicHex/Handheld/TradeRoom.hpp>
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
  // `widthOnADs`: a card's width on a DS's 256-wide screen (in proportion
  // on wider ones): 48 for the pickers' five across, 56 for the cards
  // screens' four, their titles roomier
  explicit CardSprites(int screenWidth, int widthOnADs = 48);
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

// The cards screens' cards' width on a DS (CardSprites)
inline constexpr int kCardsScreenWidth = 56;

namespace ChoiceScreens {

// A touch on one of these screens: a row or a card or a player (its
// index), or the bottom buttons (found from the screen's width alone: the
// layouts scale with it)
struct Touch {
  enum class What { Item, Back, Confirm } what;
  int item = -1;
  int card = -1; // (the player picker: which of their cards)
};

// The screens' own buttons at their foot, on ScratchPadd's Buttons (their
// ids there): Back, the green one that goes ahead (Build, Take, Steal,
// Play, Keep, Trade, Offer, Accept) and the trade room's Decline. The green
// one, when it's ready, has the D-pad's focus: A presses it
enum class ActionButton { Back, Confirm, Decline };

// The action button last pressed and when, and the time now (seconds, the
// app's clock): it glows and sinks a moment (Buttons::Panel::press). The
// app keeps it; without one, nothing shows pressed
struct Press {
  std::optional<ActionButton> button;
  double at = 0, now = 0;
  // Whether it still shows: draw again meanwhile
  bool showing() const;
};

// The build screen: a row each (Ship, Station, Colony, Card) with its cost
// as a small tile in each resource's colour for each one it takes, its note
// in red when it can't be built, `chosen` outlined; Back, and Build when
// the chosen row can be built
void drawBuild(Canvas &canvas, const std::vector<BuildRow> &rows, int chosen,
               const Press &press = {});
std::optional<Touch> buildTouch(int width, std::size_t rows, int x, int y);

// The resource picker: the five resource cards with how many you hold
// under each, those picked outlined (a resource picked twice, twice);
// Take once enough are picked, Back when it can be left
// (`focus`: the D-pad's card, underlined; -1 for none)
void drawResources(Canvas &canvas, const Pickers::ResourceChoice &choice,
                   const std::vector<Resource> &picked, const Resources &held, bool canGoBack,
                   CardSprites &cards, int focus = -1, const Press &press = {});
std::optional<Touch> resourcesTouch(int width, int x, int y);

// The player picker: a row for each player the rules allow, their name in
// their colour and their cards face down; the card tapped slides up (round
// 101, the user's: for the feel of picking one; which resource it is stays
// the rules' draw) and Steal shows
void drawPlayers(Canvas &canvas, const Pickers::PlayerChoice &choice, int chosen, int chosenCard,
                 Art::CardBack backs, CardSprites &cards, const Press &press = {});
std::optional<Touch> playersTouch(int width, std::size_t players, int x, int y);

// The cards screen: your action cards and Charters as their pictures, up
// to four a row, `chosen` outlined; under them its title and what it does
// (or why it can't be used now, in red); Back, and Play (Reveal for a
// Charter) when the chosen one can be used
void drawCards(Canvas &canvas, const std::vector<HeldCard> &cards, int chosen, CardSprites &sprites,
               const Press &press = {});
std::optional<Touch> cardsTouch(int width, std::size_t cards, int x, int y);

// After buying: the two cards dealt, side by side, `chosen` outlined, its
// words under them; Keep once one is chosen (no Back: one must be kept)
void drawKeep(Canvas &canvas, const KeepChoice &choice, int chosen, CardSprites &sprites,
              const Press &press = {});
std::optional<Touch> keepTouch(int width, std::size_t cards, int x, int y);

// The other players' tabs: one for each, their name in their colour and
// their points, `chosen` (a player) outlined, the top screen showing them;
// Back. A touch on a tab is that player (Touch::item is the player)
void drawOthers(Canvas &canvas, const std::vector<PlayerView> &others, int chosen,
                const PlayerNames &names, const Press &press = {});
std::optional<Touch> othersTouch(int width, const std::vector<PlayerView> &others, int x, int y);

// The trade room: a tab for each partner (the bank, then the players; a
// closed one greyed), your cards to give on top (a touch adds one, the
// minus under it takes one back; "of 4" is what you hold), the cards to get
// below, Back, the deal in words, and Trade (the bank) or Offer (a
// player) when it can be made. Answering an offer: what they give and
// want, a counter's changes outlined in amber, Decline and Accept
void drawTrade(Canvas &canvas, const TradeRoom &room, CardSprites &cards, const Press &press = {});
struct TradeTouch {
  enum class What { Partner, Add, Remove, Back, Confirm, Decline } what;
  int partner = 0;                              // (Partner)
  TradeRoom::Side side = TradeRoom::Side::Give; // (Add, Remove)
  Resource resource = Resource::Minerals;       // (Add, Remove)
};
std::optional<TradeTouch> tradeTouch(int width, const TradeRoom &room, int x, int y);

} // namespace ChoiceScreens

} // namespace CosmicHex::Handheld
