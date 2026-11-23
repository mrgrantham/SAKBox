// ChoiceScreens.hpp - the bottom screen's choosing screens drawn on a
// Canvas: the build screen, the resource picker, the player picker, the
// cards screen, the keep screen, the other players' tabs and the trade
// room. Each says what a touch is on, so the app needn't know their
// layout; sizes follow the screen (256 wide on a DS, in proportion on a
// 3DS), and the screens with rows of whole cards (the trade room, the
// cards screen) are laid out from the screen's size and the font's lines
// (tradeLayout, cardsLayout), the same code on every screen.
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
#include <CosmicHex/Handheld/TextMeasure.hpp>
#include <CosmicHex/Handheld/TradeRoom.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>

#include <array>
#include <optional>
#include <vector>

namespace CosmicHex::Handheld {

using ScratchPadd::Canvas; // (ScratchPadd/Canvas)

// The cards' pictures, `width` pixels wide, each a whole card in its
// layers with its icon (CardFace, as the top screen's), drawn once, when
// first needed (prepare() draws one at a time, in spare frames)
class CardSprites {
public:
  explicit CardSprites(int width);
  // Cards another width from now on (a layout's: the cards screen's for
  // how many it shows), each drawn afresh when next needed
  void resize(int width);
  // A whole card, width() x height() (CardFace::heightOf), its short title
  // in its band (dark letters on a light band) when every card's fits it
  // (titled(): small cards show their icon and band alone)
  const std::vector<Canvas::Colour> &card(Art::Card card);
  // A card's back, the same size
  const std::vector<Canvas::Colour> &back(Art::CardBack design);
  int width() const { return width_; }
  int height() const { return height_; }
  bool titled() const { return titled_; }
  // Draws one not yet drawn (the resources and the backs); whether they're
  // all ready
  bool prepare();

private:
  int width_ = 0, height_ = 0;
  bool titled_ = false;
  std::array<std::vector<Canvas::Colour>, Art::kCards> cards_;
  std::array<std::vector<Canvas::Colour>, 2> backs_;
};

// The resource picker's cards' width on a screen `screenWidth` wide (a
// DS's 48, five across; in proportion on a wider one)
int pickerCardWidth(int screenWidth);

namespace ChoiceScreens {

// A place on one of these screens, in pixels from its top left
struct Box {
  float x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const;
};

// Where the cards screen (and the keep screen) puts `count` whole cards
// on a screen `width` x `height`, worked out from its size and the font's
// lines: the heading's line at the top; the chosen card's words at the
// foot over the buttons (why it can't be used, its title, three lines of
// what it does); between them, the cards in as many rows as make them
// biggest (a DS's 56 wide at most), each row centred
struct CardsLayout {
  static constexpr std::size_t kMostCards = 8;
  ScratchPadd::Scene::LineBox heading;
  float wordsTop = 0; // (the chosen card's words, from here down)
  int cardWidth = 0, cardHeight = 0;
  std::vector<Box> cards; // (each card's place, up to kMostCards)
};
CardsLayout cardsLayout(int width, int height, std::size_t count, TextMeasure measure = {});

// Where the trade room puts its two rows of five whole cards on a screen
// `width` x `height`, worked out from its size and the font's lines the
// same way on every screen: the partner tabs across the top, the buttons
// at the foot and the deal on a line over them; between, what you get on
// the upper row and what you give on the lower (nearer you, as a hand is
// held), each under its label, the cards as big as fit across and down (a
// DS's 48 wide at most, the picker's). On each card: how many are in the
// deal at its top right (on its title band), and at its foot (its art band
// under its icon) how many you hold at the left and its minus at the right
struct TradeLayout {
  ScratchPadd::Scene::LineBox getLabel, giveLabel, deal;
  float getTop = 0, giveTop = 0; // (each row's cards' top)
  float left = 0, step = 0;      // (the first card's left; from one card's left to the next's)
  int cardWidth = 0, cardHeight = 0;
  float textScale = 1;
  Box card(float rowTop, int resource) const;
  Box count(float rowTop, int resource) const; // (its badge)
  Box minus(float rowTop, int resource) const;
  Box held(float rowTop, int resource) const; // (where "of 4" goes)
};
TradeLayout tradeLayout(int width, int height, TextMeasure measure = {});

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

// How far each of the resource picker's cards has risen into its place,
// by resource (0 below the screen, 1 in place): as it opens they come up
// one after another (BottomScreen times them)
using CardsIn = std::array<float, kResources>;
inline constexpr CardsIn kCardsInPlace{1, 1, 1, 1, 1};

// The resource picker: the five resource cards with how many you hold
// under each, those picked outlined (a resource picked twice, twice);
// Take once enough are picked, Back when it can be left
// (`focus`: the D-pad's card, underlined; -1 for none; `in`: each card's
// way up into its place)
void drawResources(Canvas &canvas, const Pickers::ResourceChoice &choice,
                   const std::vector<Resource> &picked, const Resources &held, bool canGoBack,
                   CardSprites &cards, int focus = -1, const Press &press = {},
                   const CardsIn &in = kCardsInPlace);
std::optional<Touch> resourcesTouch(int width, int x, int y);

// The player picker: a row for each player the rules allow, their name in
// their colour and their cards face down; the card tapped slides up (round
// 101, the user's: for the feel of picking one; which resource it is stays
// the rules' draw) and Steal shows
void drawPlayers(Canvas &canvas, const Pickers::PlayerChoice &choice, int chosen, int chosenCard,
                 Art::CardBack backs, CardSprites &cards, const Press &press = {});
std::optional<Touch> playersTouch(int width, std::size_t players, int x, int y);

// The cards screen: your action cards and Charters as whole cards
// (cardsLayout; `sprites` resized to its cards), `chosen` outlined; under
// them its title and what it does (or why it can't be used now, in red);
// Back, and Play (Reveal for a Charter) when the chosen one can be used
void drawCards(Canvas &canvas, const std::vector<HeldCard> &cards, int chosen, CardSprites &sprites,
               const Press &press = {});
std::optional<Touch> cardsTouch(int width, int height, std::size_t cards, int x, int y);

// After buying: the two cards dealt, side by side (cardsLayout), `chosen`
// outlined, its words under them; Keep once one is chosen (no Back: one
// must be kept)
void drawKeep(Canvas &canvas, const KeepChoice &choice, int chosen, CardSprites &sprites,
              const Press &press = {});
std::optional<Touch> keepTouch(int width, int height, std::size_t cards, int x, int y);

// The other players' tabs: one for each, their name in their colour and
// their points, `chosen` (a player) outlined, the top screen showing them;
// Back. A touch on a tab is that player (Touch::item is the player)
void drawOthers(Canvas &canvas, const std::vector<PlayerView> &others, int chosen,
                const PlayerNames &names, const Press &press = {});
std::optional<Touch> othersTouch(int width, const std::vector<PlayerView> &others, int x, int y);

// The trade room (tradeLayout, `cards` resized to its cards): a tab for
// each partner (the bank, then the players; a closed one greyed), the
// cards to get on the upper row, your cards to give on the lower (a touch
// adds one, the minus on it takes one back; "of 4" on it is what you
// hold), Back, the deal in words, and Trade (the bank) or Offer (a player)
// when it can be made. Answering an offer: what they give and
// want, a counter's changes outlined in amber, Decline and Accept, and
// Counter (in Back's place) for an offer to you. Countering: the same
// screen as an offer, headed with whose offer, no tabs, its go button
// Counter
void drawTrade(Canvas &canvas, const TradeRoom &room, CardSprites &cards, const Press &press = {});
struct TradeTouch {
  enum class What { Partner, Add, Remove, Back, Confirm, Decline, Counter } what;
  int partner = 0;                              // (Partner)
  TradeRoom::Side side = TradeRoom::Side::Give; // (Add, Remove)
  Resource resource = Resource::Minerals;       // (Add, Remove)
};
std::optional<TradeTouch> tradeTouch(int width, int height, const TradeRoom &room, int x, int y);

} // namespace ChoiceScreens

} // namespace CosmicHex::Handheld
