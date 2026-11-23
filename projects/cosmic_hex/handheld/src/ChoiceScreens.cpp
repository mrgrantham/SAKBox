#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/CardFace.hpp>
#include <CosmicHex/Handheld/ChoiceScreens.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>

#include <ScratchPadd/Buttons/Buttons.hpp>
#include <ScratchPadd/Scene/Overlay.hpp>
#include <ScratchPadd/Scene/TextWrap.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace CosmicHex::Handheld {

namespace {
using Colour = Canvas::Colour;
constexpr Colour kBackground{16, 24, 42, 255};
constexpr Colour kRow{29, 42, 69, 255};
constexpr Colour kRowChosen{52, 72, 112, 255};
constexpr Colour kEdge{90, 107, 140, 255};
constexpr Colour kChosen = Palette::kHighlight;
constexpr Colour kWords = Palette::kWords;
constexpr Colour kDim{138, 148, 168, 255};
constexpr Colour kCannot{255, 128, 120, 255};
constexpr Colour kConfirm{42, 90, 58, 255};
constexpr Colour kConfirmEdge{79, 191, 122, 255};
// The resources' colours, as their cards' bands
constexpr std::array<Colour, kResources> kResourceColours{
    Colour{170, 150, 130, 255}, Colour{110, 200, 110, 255}, Colour{235, 150, 80, 255},
    Colour{170, 215, 245, 255}, Colour{185, 130, 230, 255}};

// Sizes in a DS's pixels, scaled to the screen
float scaleOf(int width) { return static_cast<float>(width) / 256.0f; }
float scaleOf(const Canvas &canvas) { return scaleOf(canvas.width()); }

using ChoiceScreens::Box;
Box backBox(int width) {
  const float k = scaleOf(width);
  return {4 * k, 168 * k, 56 * k, 21 * k};
}
Box confirmBox(int width) {
  const float k = scaleOf(width);
  return {196 * k, 168 * k, 56 * k, 21 * k};
}
Box declineBox(int width) {
  const float k = scaleOf(width);
  return {100 * k, 168 * k, 56 * k, 21 * k};
}

// The action buttons, on ScratchPadd's Buttons: in the screens' own row
// colours (raised, as every panel), the D-pad's in amber
namespace Buttons = ScratchPadd::Buttons;
using ChoiceScreens::ActionButton;
using ChoiceScreens::Press;
const Buttons::Style kActionStyle{.face = kRow, .edge = kEdge, .words = kWords, .focus = kChosen};
constexpr Colour kDecline{74, 32, 32, 255};

Buttons::Area areaOf(const Box &box) { return {box.x, box.y, box.w, box.h}; }
int idOf(ActionButton button) { return static_cast<int>(button); }

Buttons::Button backButton(int width) {
  return {.id = idOf(ActionButton::Back), .area = areaOf(backBox(width)), .label = "Back"};
}
// The green one that goes ahead; not yet `ready`, dimmed in the row colours
Buttons::Button goButton(int width, std::string_view words, bool ready = true) {
  return {.id = idOf(ActionButton::Confirm),
          .area = areaOf(confirmBox(width)),
          .label = std::string(words),
          .usable = ready,
          .face = ready ? std::optional(kConfirm) : std::nullopt,
          .edge = ready ? std::optional(kConfirmEdge) : std::nullopt};
}
// (in Back's place: an offer to you has no Back, being yours to answer)
Buttons::Button counterButton(int width) {
  return {.id = idOf(ActionButton::Back), .area = areaOf(backBox(width)), .label = "Counter"};
}
Buttons::Button declineButton(int width) {
  return {.id = idOf(ActionButton::Decline),
          .area = areaOf(declineBox(width)),
          .label = "Decline",
          .face = kDecline,
          .edge = kCannot};
}

// A screen's action buttons drawn: the go button, when ready, in focus (A
// presses it), and the one just pressed glowing and sunk
void drawActions(Canvas &canvas, std::vector<Buttons::Button> buttons, const Press &press) {
  Buttons::Panel panel;
  panel.set(std::move(buttons));
  if (const Buttons::Button *go = panel.find(idOf(ActionButton::Confirm)); go && go->usable)
    panel.setFocus(go->id);
  if (press.button)
    panel.press(idOf(*press.button), press.at);
  panel.setTime(press.now);
  panel.draw(canvas, kActionStyle);
}

Art::Card cardOf(Resource resource) { return static_cast<Art::Card>(static_cast<int>(resource)); }

// The rows of the build screen and of the player picker
Box buildRowBox(int width, std::size_t row) {
  const float k = scaleOf(width);
  return {4 * k, (6 + 39 * static_cast<float>(row)) * k, 248 * k, 35 * k};
}
Box playerRowBox(int width, std::size_t row) {
  const float k = scaleOf(width);
  return {8 * k, (36 + 62 * static_cast<float>(row)) * k, 240 * k, 54 * k};
}
Box resourceBox(int width, int r, int cardWidth, int cardHeight) {
  const float k = scaleOf(width);
  return {(6 + 49 * static_cast<float>(r)) * k, 40 * k, static_cast<float>(cardWidth),
          static_cast<float>(cardHeight)};
}

constexpr int kMostCardsShown = 16;

// How far in from a card's edge what's drawn on it sits, in its units (as
// its bands are)
constexpr float kOnCardInset = Art::kCardTitle.x;

std::string_view nameOfPlayer(int player) {
  static constexpr std::array<std::string_view, kMaxPlayers> kNames{"P1", "P2", "P3", "P4"};
  return kNames[static_cast<std::size_t>(player)];
}
} // namespace

int pickerCardWidth(int screenWidth) {
  return static_cast<int>(std::lround(48 * scaleOf(screenWidth)));
}

CardSprites::CardSprites(int width) { resize(width); }

void CardSprites::resize(int width) {
  if (width == width_)
    return;
  width_ = width;
  height_ = CardFace::heightOf(width);
  for (auto &sprite : cards_)
    sprite.clear();
  for (auto &sprite : backs_)
    sprite.clear();
  // Titles when every card's fits its band, its letters' own pixels (as
  // drawn: one size for them all, or none)
  const float band = Art::kCardTitle.width * static_cast<float>(width) / Art::kCardWidth;
  titled_ = true;
  for (int c = 0; c < Art::kCards; ++c)
    titled_ = titled_ &&
              ScratchPadd::Scene::Overlay::inkOf(Art::wordsOf(static_cast<Art::Card>(c)).shortTitle)
                      .width <= band;
}

const std::vector<Canvas::Colour> &CardSprites::card(Art::Card card) {
  auto &sprite = cards_[static_cast<std::size_t>(card)];
  if (!sprite.empty())
    return sprite;
  // Its layers and its icon, as the top screen draws them
  const CardFace::PixelArea icon = CardFace::iconPixelsOn(card, width_);
  sprite = CardFace::pixelsOf(card, width_, CardFace::iconPixels(card, icon.width, icon.height));
  if (!titled_)
    return sprite;
  // Its short title on its band, dark on a light band
  Canvas band(width_, height_);
  band.copy(sprite);
  const float u = static_cast<float>(width_) / Art::kCardWidth;
  band.textCentred(static_cast<float>(width_) / 2,
                   (Art::kCardTitle.y + Art::kCardTitle.height / 2) * u,
                   Art::wordsOf(card).shortTitle, CardFace::titleInkOf(card));
  // (the corners stay see-through: only where the card is drawn changes)
  for (std::size_t p = 0; p < sprite.size(); ++p)
    if (sprite[p].a > 0)
      sprite[p] = {band.pixels()[p].r, band.pixels()[p].g, band.pixels()[p].b, sprite[p].a};
  return sprite;
}

const std::vector<Canvas::Colour> &CardSprites::back(Art::CardBack design) {
  auto &sprite = backs_[static_cast<std::size_t>(design)];
  if (sprite.empty())
    sprite = recipePixels(Art::cardBackRecipe(design), width_, height_);
  return sprite;
}

bool CardSprites::prepare() {
  for (int r = 0; r < kResources; ++r)
    if (cards_[static_cast<std::size_t>(r)].empty()) {
      card(static_cast<Art::Card>(r));
      return false;
    }
  for (Art::CardBack design : {Art::CardBack::Rings, Art::CardBack::Planets})
    if (backs_[static_cast<std::size_t>(design)].empty()) {
      back(design);
      return false;
    }
  return true;
}

namespace ChoiceScreens {

bool Box::contains(int px, int py) const {
  return static_cast<float>(px) >= x && static_cast<float>(px) < x + w &&
         static_cast<float>(py) >= y && static_cast<float>(py) < y + h;
}

bool Press::showing() const {
  return button && now >= at && now - at < Buttons::Panel::kPressSeconds;
}

void drawBuild(Canvas &canvas, const std::vector<BuildRow> &rows, int chosen, const Press &press) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const BuildRow &row = rows[i];
    const Box box = buildRowBox(canvas.width(), i);
    canvas.fillRect(box.x, box.y, box.w, box.h, kRow);
    const bool isChosen = static_cast<int>(i) == chosen;
    canvas.outline(box.x, box.y, box.w, box.h, isChosen ? kChosen : kEdge, isChosen ? 2 : 1);
    canvas.text(box.x + 6 * k, box.y + 6 * k, nameOf(row.what), row.canBuild ? kWords : kDim,
                k >= 1.2f ? 1.5f : 1.0f);
    // Its cost: a small tile in each resource's colour for each one it takes
    float x = box.x + 74 * k;
    for (int r = 0; r < kResources; ++r)
      for (int n = 0; n < row.cost[static_cast<std::size_t>(r)]; ++n) {
        canvas.fillRect(x, box.y + 6 * k, 8 * k, 10 * k,
                        kResourceColours[static_cast<std::size_t>(r)]);
        x += 10 * k;
      }
    if (x == box.x + 74 * k)
      canvas.text(x, box.y + 7 * k, "free", kDim);
    canvas.text(box.x + 6 * k, box.y + 22 * k, row.note, row.canBuild ? kDim : kCannot);
  }
  std::vector<Buttons::Button> actions{backButton(canvas.width())};
  if (chosen >= 0 && chosen < static_cast<int>(rows.size()) &&
      rows[static_cast<std::size_t>(chosen)].canBuild)
    actions.push_back(goButton(canvas.width(), "Build"));
  drawActions(canvas, std::move(actions), press);
}

std::optional<Touch> buildTouch(int width, std::size_t rows, int x, int y) {
  if (backBox(width).contains(x, y))
    return Touch{Touch::What::Back};
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  for (std::size_t i = 0; i < rows; ++i)
    if (buildRowBox(width, i).contains(x, y))
      return Touch{Touch::What::Item, static_cast<int>(i)};
  return std::nullopt;
}

void drawResources(Canvas &canvas, const Pickers::ResourceChoice &choice,
                   const std::vector<Resource> &picked, const Resources &held, bool canGoBack,
                   CardSprites &cards, int focus, const Press &press, const CardsIn &in) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  canvas.text(8 * k, 8 * k, choice.title, kWords);
  canvas.text(8 * k, 21 * k,
              choice.left > 1
                  ? std::string(choice.note) + " (" + std::to_string(choice.left) + " to choose)"
                  : std::string(choice.note),
              kDim);
  for (int r = 0; r < kResources; ++r) {
    const auto resource = static_cast<Resource>(r);
    Box box = resourceBox(canvas.width(), r, cards.width(), cards.height());
    // (on its way up: from below the screen, with what's under it)
    box.y += (1 - in[static_cast<std::size_t>(r)]) * (static_cast<float>(canvas.height()) - box.y);
    const bool offered = choice.offers(resource);
    canvas.blend(cards.card(cardOf(resource)), cards.width(), cards.height(),
                 static_cast<int>(box.x), static_cast<int>(box.y), offered ? 1.0f : 0.35f);
    const auto times = std::count(picked.begin(), picked.end(), resource);
    for (int t = 0; t < times; ++t)
      canvas.outline(box.x - 2 - 3 * static_cast<float>(t), box.y - 2 - 3 * static_cast<float>(t),
                     box.w + 4 + 6 * static_cast<float>(t), box.h + 4 + 6 * static_cast<float>(t),
                     kChosen, 2);
    canvas.textCentred(box.x + box.w / 2, box.y + box.h + 9 * k,
                       std::to_string(held[static_cast<std::size_t>(r)]) + " held", kDim);
    if (r == focus)
      canvas.fillRect(box.x + 4 * k, box.y + box.h + 16 * k, box.w - 8 * k, 2 * k, kWords);
  }
  std::vector<Buttons::Button> actions;
  if (canGoBack)
    actions.push_back(backButton(canvas.width()));
  // (Take always there, the user's: dim until enough are picked)
  const bool ready = !choice.movesFor(picked).empty();
  actions.push_back(goButton(canvas.width(), "Take", ready));
  drawActions(canvas, std::move(actions), press);
}

std::optional<Touch> resourcesTouch(int width, int x, int y) {
  if (backBox(width).contains(x, y))
    return Touch{Touch::What::Back};
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  const int w = pickerCardWidth(width), h = CardFace::heightOf(w);
  for (int r = 0; r < kResources; ++r)
    if (resourceBox(width, r, w, h).contains(x, y))
      return Touch{Touch::What::Item, r};
  return std::nullopt;
}

void drawPlayers(Canvas &canvas, const Pickers::PlayerChoice &choice, int chosen, int chosenCard,
                 Art::CardBack backs, CardSprites &cards, const Press &press) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  canvas.text(8 * k, 8 * k, "Steal from whom?", kWords);
  canvas.text(8 * k, 21 * k, "only players beside the planet", kDim);
  // The cards' backs at half a small card's width, overlapping
  const int bw = cards.width() / 2, bh = cards.height() / 2;
  std::vector<Canvas::Colour> half(static_cast<std::size_t>(bw) * static_cast<std::size_t>(bh));
  const auto &whole = cards.back(backs);
  if (!whole.empty())
    for (int y = 0; y < bh; ++y)
      for (int x = 0; x < bw; ++x)
        half[static_cast<std::size_t>(y) * static_cast<std::size_t>(bw) +
             static_cast<std::size_t>(x)] =
            whole[static_cast<std::size_t>(y * 2) * static_cast<std::size_t>(cards.width()) +
                  static_cast<std::size_t>(x * 2)];
  for (std::size_t i = 0; i < choice.options.size(); ++i) {
    const auto &option = choice.options[i];
    const Box box = playerRowBox(canvas.width(), i);
    const bool isChosen = static_cast<int>(i) == chosen;
    // (chosen: a lighter row and a white edge, apart from every player's
    // colour, P4's yellow included)
    canvas.fillRect(box.x, box.y, box.w, box.h, isChosen ? kRowChosen : kRow);
    const ScratchPadd::Scene::Colour playerColour = BoardColours::ofPlayer(option.player);
    if (isChosen)
      canvas.outline(box.x, box.y, box.w, box.h, kWords, 2);
    else
      canvas.outline(box.x, box.y, box.w, box.h,
                     {playerColour.r, playerColour.g, playerColour.b, 255});
    canvas.text(box.x + 8 * k, box.y + 8 * k, nameOfPlayer(option.player),
                {playerColour.r, playerColour.g, playerColour.b, 255}, 1.5f);
    canvas.text(box.x + 8 * k, box.y + 32 * k,
                std::to_string(option.cards) + (option.cards == 1 ? " card" : " cards"), kWords);
    const int shown = std::min(option.cards, kMostCardsShown);
    for (int c = 0; c < shown; ++c) {
      const bool up = isChosen && c == chosenCard;
      const float cx = box.x + 72 * k + static_cast<float>(c) * 9 * k;
      const float cy = box.y + (box.h - static_cast<float>(bh)) / 2 - (up ? 7 * k : 0);
      canvas.blend(half, bw, bh, static_cast<int>(cx), static_cast<int>(cy));
      if (up)
        canvas.outline(cx - 1, cy - 1, static_cast<float>(bw) + 2, static_cast<float>(bh) + 2,
                       kChosen);
    }
  }
  if (chosen >= 0 && chosenCard >= 0)
    drawActions(canvas, {goButton(canvas.width(), "Steal")}, press);
}

std::optional<Touch> playersTouch(int width, std::size_t players, int x, int y) {
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  const float k = scaleOf(width);
  for (std::size_t i = 0; i < players; ++i) {
    const Box box = playerRowBox(width, i);
    if (!box.contains(x, y))
      continue;
    // The card under the touch: the last whose left edge is left of it
    // (they overlap, each 9 pixels on from the last)
    const float along = (static_cast<float>(x) - (box.x + 72 * k)) / (9 * k);
    const int card = along < 0 ? 0 : static_cast<int>(along);
    return Touch{Touch::What::Item, static_cast<int>(i), card};
  }
  return std::nullopt;
}

namespace {
namespace Scene = ScratchPadd::Scene;

// The chosen card's words: why it can't be used now, its title (bigger on
// a big screen) and three lines of what it does
float titleScaleOf(int width) { return scaleOf(width) >= 1.2f ? 1.5f : 1; }
std::array<float, 5> wordsScales(int width) { return {1, titleScaleOf(width), 1, 1, 1}; }
float wordsGap(int width) { return 2 * scaleOf(width); }

// A card's words, wrapped to the screen, from `top` down (wordsScales):
// `why` in red on the first line (none: left blank, so the words stay
// put as the cards are gone through), the title, what it does
void cardWords(Canvas &canvas, float top, std::string_view why, std::string_view title,
               std::string_view words) {
  const auto lines = Scene::stackDown(top, wordsScales(canvas.width()), wordsGap(canvas.width()));
  const float middle = static_cast<float>(canvas.width()) / 2;
  if (!why.empty())
    canvas.textCentred(middle, lines[0].middle(), why, kCannot);
  canvas.textCentred(middle, lines[1].middle(), title, kWords, titleScaleOf(canvas.width()));
  const auto wrapped =
      Scene::wrapText(words, static_cast<float>(canvas.width()) - 24 * scaleOf(canvas),
                      [](std::string_view line) { return Scene::Overlay::textWidth(line); });
  for (std::size_t l = 0; l < wrapped.size() && l + 2 < lines.size(); ++l)
    canvas.textCentred(middle, lines[l + 2].middle(), wrapped[l], kDim);
}

// A card drawn whole at its place, dimmed by `opacity`, outlined when chosen
void drawCard(Canvas &canvas, CardSprites &sprites, Art::Card card, const Box &box, bool chosen,
              float opacity = 1) {
  canvas.blend(sprites.card(card), sprites.width(), sprites.height(), static_cast<int>(box.x),
               static_cast<int>(box.y), opacity);
  if (chosen)
    canvas.outline(box.x - 2, box.y - 2, box.w + 4, box.h + 4, kChosen, 2);
}
} // namespace

CardsLayout cardsLayout(int width, int height, std::size_t count, TextMeasure measure) {
  CardsLayout l;
  const float k = scaleOf(width), gap = 6 * k, margin = 4 * k;
  constexpr float kWidest = 56; // (a DS's: four across)
  l.heading = Scene::stackDown(3 * k, std::array{1.0f}, 0, measure.lineHeight)[0];
  // The chosen card's words over the buttons (on a screen as short as
  // they are, at its foot)
  const float buttonsTop = std::min(backBox(width).y, static_cast<float>(height));
  l.wordsTop = buttonsTop - 2 * k -
               Scene::stackHeight(wordsScales(width), wordsGap(width), measure.lineHeight);
  const std::size_t shown = std::min(count, CardsLayout::kMostCards);
  if (shown == 0)
    return l;
  // The cards between: as many rows as make them biggest
  const float top = l.heading.bottom + gap, room = l.wordsTop - gap - top;
  const float across = static_cast<float>(width) - 2 * margin;
  std::size_t rows = 1;
  float widest = 0;
  for (std::size_t r = 1; r <= shown; ++r) {
    const std::size_t inRow = (shown + r - 1) / r; // (a full row's cards, rounded up)
    const auto perRow = static_cast<float>(inRow);
    const float w =
        std::min(kWidest * k, CardFace::widthToFit((across - (perRow - 1) * gap) / perRow,
                                                   (room - static_cast<float>(r - 1) * gap) /
                                                       static_cast<float>(r)));
    if (w > widest) {
      widest = w;
      rows = r;
    }
  }
  l.cardWidth = std::max(1, static_cast<int>(widest));
  l.cardHeight = CardFace::heightOf(l.cardWidth);
  // Each row centred across, the rows together centred in their room
  const std::size_t perRow = (shown + rows - 1) / rows;
  const auto w = static_cast<float>(l.cardWidth), h = static_cast<float>(l.cardHeight);
  const float rowsTop =
      top + (room - (static_cast<float>(rows) * h + static_cast<float>(rows - 1) * gap)) / 2;
  for (std::size_t i = 0; i < shown; ++i) {
    const std::size_t row = i / perRow, column = i % perRow;
    const auto inRow = static_cast<float>(std::min(perRow, shown - row * perRow));
    const float left = (static_cast<float>(width) - (inRow * w + (inRow - 1) * gap)) / 2;
    l.cards.push_back({left + static_cast<float>(column) * (w + gap),
                       rowsTop + static_cast<float>(row) * (h + gap), w, h});
  }
  return l;
}

void drawCards(Canvas &canvas, const std::vector<HeldCard> &cards, int chosen, CardSprites &sprites,
               const Press &press) {
  canvas.clear(kBackground);
  const CardsLayout layout = cardsLayout(canvas.width(), canvas.height(), cards.size());
  canvas.textCentred(static_cast<float>(canvas.width()) / 2, layout.heading.middle(),
                     cards.empty() ? "No cards yet: buy one from Build" : "Your cards", kDim);
  sprites.resize(layout.cardWidth);
  for (std::size_t i = 0; i < layout.cards.size(); ++i)
    drawCard(canvas, sprites, cards[i].card, layout.cards[i], static_cast<int>(i) == chosen,
             cards[i].playable ? 1.0f : 0.55f);
  std::vector<Buttons::Button> actions{backButton(canvas.width())};
  if (chosen >= 0 && chosen < static_cast<int>(cards.size())) {
    const HeldCard &held = cards[static_cast<std::size_t>(chosen)];
    // What it does always; when it can't be used now, why, in red above
    const Art::CardWords words = Art::wordsOf(held.card);
    cardWords(canvas, layout.wordsTop, held.playable ? "" : held.why, words.title, words.words);
    if (held.playable)
      actions.push_back(goButton(canvas.width(), held.charter ? "Reveal" : "Play"));
  }
  drawActions(canvas, std::move(actions), press);
}

std::optional<Touch> cardsTouch(int width, int height, std::size_t cards, int x, int y) {
  if (backBox(width).contains(x, y))
    return Touch{Touch::What::Back};
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  const CardsLayout layout = cardsLayout(width, height, cards);
  for (std::size_t i = 0; i < layout.cards.size(); ++i)
    if (layout.cards[i].contains(x, y))
      return Touch{Touch::What::Item, static_cast<int>(i)};
  return std::nullopt;
}

void drawKeep(Canvas &canvas, const KeepChoice &choice, int chosen, CardSprites &sprites,
              const Press &press) {
  canvas.clear(kBackground);
  const CardsLayout layout = cardsLayout(canvas.width(), canvas.height(), choice.cards.size());
  const float middle = static_cast<float>(canvas.width()) / 2;
  canvas.textCentred(middle, layout.heading.middle(), "Keep one of these two", kDim);
  sprites.resize(layout.cardWidth);
  for (std::size_t i = 0; i < layout.cards.size(); ++i)
    drawCard(canvas, sprites, choice.cards[i], layout.cards[i], static_cast<int>(i) == chosen);
  if (chosen >= 0 && chosen < static_cast<int>(choice.cards.size())) {
    const Art::CardWords words = Art::wordsOf(choice.cards[static_cast<std::size_t>(chosen)]);
    cardWords(canvas, layout.wordsTop, "", words.title, words.words);
    drawActions(canvas, {goButton(canvas.width(), "Keep")}, press);
  } else {
    const auto lines =
        Scene::stackDown(layout.wordsTop, wordsScales(canvas.width()), wordsGap(canvas.width()));
    canvas.textCentred(middle, lines[1].middle(), "Touch one to read it", kDim);
  }
}

std::optional<Touch> keepTouch(int width, int height, std::size_t cards, int x, int y) {
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  const CardsLayout layout = cardsLayout(width, height, cards);
  for (std::size_t i = 0; i < layout.cards.size(); ++i)
    if (layout.cards[i].contains(x, y))
      return Touch{Touch::What::Item, static_cast<int>(i)};
  return std::nullopt;
}

namespace {
// The tabs as a panel of buttons (ScratchPadd's Buttons), each's id its
// player, in a row across the middle
ScratchPadd::Buttons::Panel othersPanel(int width, const std::vector<PlayerView> &others,
                                        int chosen) {
  const float k = scaleOf(width);
  const float w = 72 * k, gap = 8 * k;
  const auto count = static_cast<float>(others.size());
  const float left = (static_cast<float>(width) - (count * w + (count - 1) * gap)) / 2;
  std::vector<ScratchPadd::Buttons::Button> tabs;
  for (std::size_t i = 0; i < others.size(); ++i) {
    const PlayerView &view = others[i];
    tabs.push_back({.id = view.player,
                    .area = {left + static_cast<float>(i) * (w + gap), 70 * k, w, 44 * k},
                    .face = view.player == chosen ? std::optional(kRowChosen) : std::nullopt,
                    .edge = BoardColours::ofPlayer(view.player)});
  }
  ScratchPadd::Buttons::Panel panel;
  panel.set(std::move(tabs));
  panel.setFocus(chosen);
  return panel;
}
} // namespace

void drawOthers(Canvas &canvas, const std::vector<PlayerView> &others, int chosen,
                const PlayerNames &names, const Press &press) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  canvas.textCentred(static_cast<float>(canvas.width()) / 2, 40 * k,
                     "Other players: touch one, or left and right", kDim);
  othersPanel(canvas.width(), others, chosen)
      .draw(canvas, {.face = kRow, .edge = kEdge, .words = kWords, .focus = kChosen});
  // Each tab's words: the name in the player's colour, the points under it
  for (const auto &tab : othersPanel(canvas.width(), others, chosen).buttons()) {
    const auto &a = tab.area;
    const auto view = std::find_if(others.begin(), others.end(),
                                   [&](const PlayerView &v) { return v.player == tab.id; });
    canvas.textCentred(a.x + a.width / 2, a.y + 14 * k, names.shortOf(tab.id),
                       BoardColours::ofPlayer(tab.id), k >= 1.2f ? 1.5f : 1.0f);
    canvas.textCentred(a.x + a.width / 2, a.y + 32 * k,
                       std::to_string(view->points) + (view->points == 1 ? " point" : " points"),
                       kWords);
  }
  drawActions(canvas, {backButton(canvas.width())}, press);
}

std::optional<Touch> othersTouch(int width, const std::vector<PlayerView> &others, int x, int y) {
  if (backBox(width).contains(x, y))
    return Touch{Touch::What::Back};
  if (const std::optional<int> player = othersPanel(width, others, -1).at(x, y))
    return Touch{Touch::What::Item, *player};
  return std::nullopt;
}

namespace {
// The trade room's partner tabs, a panel (ScratchPadd's Buttons) across
// the top, each's id its partner, down to tradeTabsBottom
float tradeTabsBottom(int width) { return 20 * scaleOf(width); }
ScratchPadd::Buttons::Panel tradeTabs(int width, const TradeRoom &room) {
  const float k = scaleOf(width);
  const std::vector<int> partners = room.partners();
  const float w =
      (static_cast<float>(width) - 8 * k - 3 * k * static_cast<float>(partners.size() - 1)) /
      static_cast<float>(partners.size());
  std::vector<ScratchPadd::Buttons::Button> tabs;
  for (std::size_t i = 0; i < partners.size(); ++i) {
    const int p = partners[i];
    tabs.push_back({.id = p,
                    .area = {4 * k + static_cast<float>(i) * (w + 3 * k), 2 * k, w, 18 * k},
                    .label = p == TradeRoom::kBank ? "Bank" : room.names().shortOf(p),
                    .usable = room.open(p),
                    // (the chosen tab in the palette's colour for what you've chosen)
                    .edge = p == room.partner() ? std::optional(kChosen) : std::nullopt});
  }
  ScratchPadd::Buttons::Panel panel;
  panel.set(std::move(tabs));
  return panel;
}
// A length in a card's units, on the trade room's cards, in pixels
float onCard(const TradeLayout &l, float units) {
  return units * static_cast<float>(l.cardWidth) / Art::kCardWidth;
}
} // namespace

Box TradeLayout::card(float rowTop, int resource) const {
  return {left + static_cast<float>(resource) * step, rowTop, static_cast<float>(cardWidth),
          static_cast<float>(cardHeight)};
}

Box TradeLayout::count(float rowTop, int resource) const {
  // (on its title band, at its right)
  const Box c = card(rowTop, resource);
  const float band = onCard(*this, Art::kCardTitle.height), w = 1.4f * band;
  return {c.x + c.w - onCard(*this, kOnCardInset) - w, c.y + onCard(*this, Art::kCardTitle.y), w,
          band};
}

Box TradeLayout::minus(float rowTop, int resource) const {
  // (at its foot's right: its art band under its icon)
  const Box c = card(rowTop, resource);
  const Art::CardArea icon = CardFace::iconOf(static_cast<Art::Card>(resource));
  const Art::CardArea art = CardFace::artOf(static_cast<Art::Card>(resource));
  const float footTop = c.y + onCard(*this, icon.y + icon.height),
              footBottom = c.y + onCard(*this, art.y + art.height);
  const float h = std::min(footBottom - footTop, 12 * textScale), w = 11 * textScale;
  return {c.x + c.w - onCard(*this, kOnCardInset) - w, (footTop + footBottom - h) / 2, w, h};
}

Box TradeLayout::held(float rowTop, int resource) const {
  // (at its foot's left, up to its minus)
  const Box c = card(rowTop, resource), m = minus(rowTop, resource);
  const float x = c.x + onCard(*this, kOnCardInset);
  return {x, m.y, m.x - x, m.h};
}

TradeLayout tradeLayout(int width, int height, TextMeasure measure) {
  TradeLayout l;
  const float k = scaleOf(width), gap = 2 * k, margin = 4 * k;
  l.textScale = k;
  constexpr float kWidest = 48; // (a DS's: the picker's)
  // The deal on a line over the buttons (on a screen as short as they
  // are, at its foot), the labels over their rows
  const float buttonsTop = std::min(backBox(width).y, static_cast<float>(height));
  l.deal = Scene::stackUp(buttonsTop - gap, std::array{1.0f}, 0, measure.lineHeight)[0];
  const float line = measure.lineHeight(1);
  const float top = tradeTabsBottom(width) + gap, bottom = l.deal.top - gap;
  // The cards as big as fit: five across, two rows down under their labels
  const float rowsHigh = bottom - top - 2 * (line + gap) - gap;
  const float across = (static_cast<float>(width) - 2 * margin - 4 * gap) / 5;
  l.cardWidth = std::max(
      1, static_cast<int>(std::min(kWidest * k, CardFace::widthToFit(across, rowsHigh / 2))));
  l.cardHeight = CardFace::heightOf(l.cardWidth);
  const auto w = static_cast<float>(l.cardWidth), h = static_cast<float>(l.cardHeight);
  // Across: centred; down: what's spare between the rows, yours (what you
  // give) on the lower, nearer you, as a hand of cards is held (the user's)
  l.step = w + std::min(gap * 3, (static_cast<float>(width) - 2 * margin - 5 * w) / 4);
  l.left = (static_cast<float>(width) - (4 * l.step + w)) / 2;
  const float spare = rowsHigh - 2 * h;
  l.getLabel = Scene::stackDown(top, std::array{1.0f}, 0, measure.lineHeight)[0];
  l.getTop = l.getLabel.bottom + gap;
  l.giveLabel =
      Scene::stackDown(l.getTop + h + gap + spare, std::array{1.0f}, 0, measure.lineHeight)[0];
  l.giveTop = l.giveLabel.bottom + gap;
  return l;
}

void drawTrade(Canvas &canvas, const TradeRoom &room, CardSprites &cards, const Press &press) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  const int width = canvas.width();
  const TradeLayout layout = tradeLayout(width, canvas.height());
  cards.resize(layout.cardWidth);
  const bool answering = room.showingOffer();
  if (room.countering())
    canvas.textCentred(static_cast<float>(width) / 2, 11 * k,
                       "Your counter to " + room.names()[room.from()], kChosen);
  else if (!answering)
    tradeTabs(width, room)
        .draw(canvas,
              {.face = kRow, .edge = kEdge, .words = kWords, .labelScale = 1, .dimmed = 0.45f});
  else
    canvas.textCentred(static_cast<float>(width) / 2, 11 * k,
                       room.names()[room.from()] + (room.counterToYours()
                                                        ? " answers with a counter"
                                                        : " offers you a trade"),
                       kChosen);
  // (each card's minus, a small button on it, gathered as the rows are
  // drawn and drawn together after them)
  std::vector<Buttons::Button> minuses;
  const auto row = [&](float top, const Scene::LineBox &labelLine, const Resources &counts,
                       std::string_view label, bool minus, bool showHeld) {
    canvas.text(layout.left, labelLine.top, label, kDim);
    for (int r = 0; r < kResources; ++r) {
      const Box box = layout.card(top, r);
      const int n = counts[static_cast<std::size_t>(r)];
      canvas.blend(cards.card(cardOf(static_cast<Resource>(r))), cards.width(), cards.height(),
                   static_cast<int>(box.x), static_cast<int>(box.y), n > 0 ? 1.0f : 0.45f);
      if (n > 0) {
        // (how many in a badge on its title band)
        canvas.outline(box.x - 1, box.y - 1, box.w + 2, box.h + 2, kChosen, 2);
        const Box badge = layout.count(top, r);
        canvas.fillRect(badge.x, badge.y, badge.w, badge.h, Colour{12, 16, 28, 255});
        canvas.textCentred(badge.x + badge.w / 2, badge.y + badge.h / 2, std::to_string(n), kWords);
      }
      if (answering && std::find(room.changed().begin(), room.changed().end(),
                                 static_cast<Resource>(r)) != room.changed().end())
        canvas.outline(box.x - 3, box.y - 3, box.w + 6, box.h + 6, kChosen);
      if (minus)
        minuses.push_back({.id = static_cast<int>(minuses.size()),
                           .area = areaOf(layout.minus(top, r)),
                           .label = "-"});
      if (showHeld) {
        // (at its foot's left, clear of the minus at its right: "of 4", or
        // just the number where that won't fit)
        const Box at = layout.held(top, r);
        const std::string count = std::to_string(room.held()[static_cast<std::size_t>(r)]);
        const std::string words = "of " + count;
        canvas.text(at.x, at.y + (at.h - Scene::Overlay::lineHeight(1)) / 2,
                    Scene::Overlay::textWidth(words) <= at.w ? words : count, kDim);
      }
    }
  };
  if (answering) {
    row(layout.giveTop, layout.giveLabel, room.theyGive(), "THEY GIVE YOU", false, false);
    row(layout.getTop, layout.getLabel, room.theyWant(), "THEY WANT FROM YOU", false, false);
    std::vector<Buttons::Button> actions{declineButton(width)};
    if (room.canCounter())
      actions.push_back(counterButton(width));
    if (room.accept())
      actions.push_back(goButton(width, "Accept"));
    else
      canvas.textCentred(224 * k, 178 * k, "can't afford", kCannot);
    drawActions(canvas, std::move(actions), press);
    return;
  }
  const bool bank = room.partner() == TradeRoom::kBank;
  row(layout.giveTop, layout.giveLabel, room.give(),
      bank ? "YOU GIVE (touch a kind)" : "YOU GIVE (touch to add)", !bank, true);
  row(layout.getTop, layout.getLabel, room.get(), bank ? "YOU GET" : "YOU ASK FOR", !bank, false);
  Buttons::Panel minusButtons;
  minusButtons.set(std::move(minuses));
  minusButtons.draw(canvas, kActionStyle);
  std::vector<Buttons::Button> actions{backButton(width)};
  // The deal over the buttons, or why not between them
  const std::string why = room.whyNot();
  canvas.textCentred(static_cast<float>(width) / 2, layout.deal.middle(), room.deal(), kWords);
  if (!why.empty())
    canvas.textCentred(static_cast<float>(width) / 2, 180 * k, why, kCannot);
  else
    actions.push_back(goButton(width, bank ? "Trade" : room.countering() ? "Counter" : "Offer"));
  drawActions(canvas, std::move(actions), press);
}

std::optional<TradeTouch> tradeTouch(int width, int height, const TradeRoom &room, int x, int y) {
  using What = TradeTouch::What;
  if (room.showingOffer()) {
    if (declineBox(width).contains(x, y))
      return TradeTouch{What::Decline};
    if (confirmBox(width).contains(x, y))
      return TradeTouch{What::Confirm};
    if (room.canCounter() && backBox(width).contains(x, y))
      return TradeTouch{What::Counter};
    return std::nullopt;
  }
  if (!room.countering())
    if (const std::optional<int> partner = tradeTabs(width, room).at(x, y))
      return TradeTouch{What::Partner, *partner};
  if (backBox(width).contains(x, y))
    return TradeTouch{What::Back};
  if (confirmBox(width).contains(x, y))
    return TradeTouch{What::Confirm};
  const TradeLayout layout = tradeLayout(width, height);
  const bool minuses = room.partner() != TradeRoom::kBank;
  for (const auto &[side, top] : {std::pair{TradeRoom::Side::Give, layout.giveTop},
                                  std::pair{TradeRoom::Side::Get, layout.getTop}})
    for (int r = 0; r < kResources; ++r) {
      if (minuses && layout.minus(top, r).contains(x, y))
        return TradeTouch{What::Remove, 0, side, static_cast<Resource>(r)};
      if (layout.card(top, r).contains(x, y))
        return TradeTouch{What::Add, 0, side, static_cast<Resource>(r)};
    }
  return std::nullopt;
}

} // namespace ChoiceScreens

} // namespace CosmicHex::Handheld
