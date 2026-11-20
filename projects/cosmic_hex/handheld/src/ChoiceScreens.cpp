#include <CosmicHex/Handheld/BoardPieces.hpp>
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

struct Box {
  float x, y, w, h;
  bool contains(int px, int py) const {
    return static_cast<float>(px) >= x && static_cast<float>(px) < x + w &&
           static_cast<float>(py) >= y && static_cast<float>(py) < y + h;
  }
};
Box backBox(int width) {
  const float k = scaleOf(width);
  return {4 * k, 168 * k, 56 * k, 21 * k};
}
Box confirmBox(int width) {
  const float k = scaleOf(width);
  return {196 * k, 168 * k, 56 * k, 21 * k};
}
void button(Canvas &canvas, const Box &box, std::string_view words, Colour fill, Colour edge) {
  canvas.fillRect(box.x, box.y, box.w, box.h, fill);
  canvas.outline(box.x, box.y, box.w, box.h, edge);
  canvas.textCentred(box.x + box.w / 2, box.y + box.h / 2, words, kWords);
}

Art::Card cardOf(Resource resource) { return static_cast<Art::Card>(static_cast<int>(resource)); }

float luminance(Colour c) {
  return (0.2126f * static_cast<float>(c.r) + 0.7152f * static_cast<float>(c.g) +
          0.0722f * static_cast<float>(c.b)) /
         255.0f;
}

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

std::string_view nameOfPlayer(int player) {
  static constexpr std::array<std::string_view, kMaxPlayers> kNames{"P1", "P2", "P3", "P4"};
  return kNames[static_cast<std::size_t>(player)];
}
} // namespace

CardSprites::CardSprites(int screenWidth, int widthOnADs)
    : width_(static_cast<int>(
          std::lround(static_cast<float>(widthOnADs) * static_cast<float>(screenWidth) / 256.0f))),
      height_(width_ * 52 / 63), backHeight_(width_ * 88 / 63) {}

const std::vector<Canvas::Colour> &CardSprites::small(Art::Card card) {
  auto &sprite = small_[static_cast<std::size_t>(card)];
  if (!sprite.empty())
    return sprite;
  // The whole card, then its top down to the picture's foot
  const int fullHeight = width_ * 88 / 63;
  const auto whole = recipePixels(Art::cardRecipeOf(card), width_, fullHeight);
  if (whole.empty())
    return sprite;
  sprite.assign(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(width_) * height_);
  // Its short title in the band, dark on a light band
  Canvas band(width_, height_);
  band.blend(sprite, width_, height_, 0, 0);
  const float u = static_cast<float>(width_) / Art::kCardWidth;
  const Colour bandColour =
      band.at(static_cast<int>(6 * u), static_cast<int>((Art::kCardTitle.y + 2) * u));
  const Colour ink = luminance(bandColour) > 0.55f ? Colour{16, 20, 30, 255} : kWords;
  band.textCentred(static_cast<float>(width_) / 2,
                   (Art::kCardTitle.y + Art::kCardTitle.height / 2) * u,
                   Art::wordsOf(card).shortTitle, ink);
  // (the corners stay see-through: only where the card is drawn changes)
  for (std::size_t p = 0; p < sprite.size(); ++p)
    if (sprite[p].a > 0)
      sprite[p] = {band.pixels()[p].r, band.pixels()[p].g, band.pixels()[p].b, sprite[p].a};
  return sprite;
}

const std::vector<Canvas::Colour> &CardSprites::back(Art::CardBack design) {
  auto &sprite = backs_[static_cast<std::size_t>(design)];
  if (sprite.empty())
    sprite = recipePixels(Art::cardBackRecipe(design), width_, backHeight_);
  return sprite;
}

bool CardSprites::prepare() {
  for (int r = 0; r < kResources; ++r)
    if (small_[static_cast<std::size_t>(r)].empty()) {
      small(static_cast<Art::Card>(r));
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

void drawBuild(Canvas &canvas, const std::vector<BuildRow> &rows, int chosen) {
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
  button(canvas, backBox(canvas.width()), "Back", kRow, kEdge);
  if (chosen >= 0 && chosen < static_cast<int>(rows.size()) &&
      rows[static_cast<std::size_t>(chosen)].canBuild)
    button(canvas, confirmBox(canvas.width()), "Build", kConfirm, kConfirmEdge);
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
                   CardSprites &cards, int focus) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  canvas.text(8 * k, 8 * k, choice.title, kWords);
  canvas.text(8 * k, 21 * k, choice.note, kDim);
  for (int r = 0; r < kResources; ++r) {
    const auto resource = static_cast<Resource>(r);
    const Box box = resourceBox(canvas.width(), r, cards.width(), cards.height());
    const bool offered = choice.offers(resource);
    canvas.blend(cards.small(cardOf(resource)), cards.width(), cards.height(),
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
  if (canGoBack)
    button(canvas, backBox(canvas.width()), "Back", kRow, kEdge);
  // (Take always there, the user's: dim until enough are picked)
  if (static_cast<int>(picked.size()) == choice.howMany && choice.actionFor(picked))
    button(canvas, confirmBox(canvas.width()), "Take", kConfirm, kConfirmEdge);
  else
    button(canvas, confirmBox(canvas.width()), "Take", kRow, kEdge);
}

std::optional<Touch> resourcesTouch(int width, int x, int y) {
  if (backBox(width).contains(x, y))
    return Touch{Touch::What::Back};
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  const float k = scaleOf(width);
  const int w = static_cast<int>(std::lround(48 * k)), h = w * 52 / 63;
  for (int r = 0; r < kResources; ++r)
    if (resourceBox(width, r, w, h).contains(x, y))
      return Touch{Touch::What::Item, r};
  return std::nullopt;
}

void drawPlayers(Canvas &canvas, const Pickers::PlayerChoice &choice, int chosen, int chosenCard,
                 Art::CardBack backs, CardSprites &cards) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  canvas.text(8 * k, 8 * k, "Steal from whom?", kWords);
  canvas.text(8 * k, 21 * k, "only players beside the planet", kDim);
  // The cards' backs at half a small card's width, overlapping
  const int bw = cards.width() / 2, bh = cards.backHeight() / 2;
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
    button(canvas, confirmBox(canvas.width()), "Steal", kConfirm, kConfirmEdge);
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
// The cards screen's grid: up to four a row, centred, under the title
Box cardBox(int width, int cardWidth, int cardHeight, std::size_t i, std::size_t count) {
  const float k = scaleOf(width);
  const auto perRow = static_cast<std::size_t>(4);
  const std::size_t row = i / perRow, column = i % perRow;
  const std::size_t inRow = std::min(perRow, count - row * perRow);
  const float w = static_cast<float>(cardWidth), h = static_cast<float>(cardHeight);
  const float gap = 8 * k;
  const float left = (static_cast<float>(width) -
                      (static_cast<float>(inRow) * w + static_cast<float>(inRow - 1) * gap)) /
                     2;
  return {left + static_cast<float>(column) * (w + gap),
          20 * k + static_cast<float>(row) * (h + gap), w, h};
}
// A card's words, wrapped to the screen, from `y` down
void cardWords(Canvas &canvas, float y, std::string_view title, std::string_view words,
               Colour colour) {
  const float k = scaleOf(canvas);
  canvas.textCentred(static_cast<float>(canvas.width()) / 2, y, title, kWords,
                     k >= 1.2f ? 1.5f : 1);
  const auto lines = ScratchPadd::Scene::wrapText(
      words, static_cast<float>(canvas.width()) - 24 * k,
      [](std::string_view line) { return ScratchPadd::Scene::Overlay::textWidth(line); });
  for (std::size_t l = 0; l < lines.size() && l < 3; ++l)
    canvas.textCentred(static_cast<float>(canvas.width()) / 2,
                       y + (14 + 11 * static_cast<float>(l)) * k, lines[l], colour);
}
} // namespace

void drawCards(Canvas &canvas, const std::vector<HeldCard> &cards, int chosen,
               CardSprites &sprites) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  canvas.textCentred(static_cast<float>(canvas.width()) / 2, 9 * k,
                     cards.empty() ? "No cards yet: buy one from Build" : "Your cards", kDim);
  for (std::size_t i = 0; i < cards.size() && i < 8; ++i) {
    const Box box = cardBox(canvas.width(), sprites.width(), sprites.height(), i, cards.size());
    canvas.blend(sprites.small(cards[i].card), sprites.width(), sprites.height(),
                 static_cast<int>(box.x), static_cast<int>(box.y),
                 cards[i].playable ? 1.0f : 0.55f);
    if (static_cast<int>(i) == chosen)
      canvas.outline(box.x - 2, box.y - 2, box.w + 4, box.h + 4, kChosen, 2);
  }
  if (chosen >= 0 && chosen < static_cast<int>(cards.size())) {
    const HeldCard &held = cards[static_cast<std::size_t>(chosen)];
    // What it does always; when it can't be used now, why, in red above
    const Art::CardWords words = Art::wordsOf(held.card);
    if (!held.playable)
      canvas.textCentred(static_cast<float>(canvas.width()) / 2, 112 * k, held.why, kCannot);
    cardWords(canvas, (held.playable ? 116 : 128) * k, words.title, words.words, kDim);
    if (held.playable)
      button(canvas, confirmBox(canvas.width()), held.charter ? "Reveal" : "Play", kConfirm,
             kConfirmEdge);
  }
  button(canvas, backBox(canvas.width()), "Back", kRow, kEdge);
}

std::optional<Touch> cardsTouch(int width, std::size_t cards, int x, int y) {
  if (backBox(width).contains(x, y))
    return Touch{Touch::What::Back};
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  const CardSprites sizes(width, kCardsScreenWidth);
  for (std::size_t i = 0; i < cards && i < 8; ++i)
    if (cardBox(width, sizes.width(), sizes.height(), i, cards).contains(x, y))
      return Touch{Touch::What::Item, static_cast<int>(i)};
  return std::nullopt;
}

void drawKeep(Canvas &canvas, const KeepChoice &choice, int chosen, CardSprites &sprites) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  canvas.textCentred(static_cast<float>(canvas.width()) / 2, 9 * k, "Keep one of these two", kDim);
  for (std::size_t i = 0; i < choice.cards.size(); ++i) {
    const Box box =
        cardBox(canvas.width(), sprites.width(), sprites.height(), i, choice.cards.size());
    canvas.blend(sprites.small(choice.cards[i]), sprites.width(), sprites.height(),
                 static_cast<int>(box.x), static_cast<int>(box.y));
    if (static_cast<int>(i) == chosen)
      canvas.outline(box.x - 2, box.y - 2, box.w + 4, box.h + 4, kChosen, 2);
  }
  if (chosen >= 0 && chosen < static_cast<int>(choice.cards.size())) {
    const Art::CardWords words = Art::wordsOf(choice.cards[static_cast<std::size_t>(chosen)]);
    cardWords(canvas, 116 * k, words.title, words.words, kDim);
    button(canvas, confirmBox(canvas.width()), "Keep", kConfirm, kConfirmEdge);
  } else {
    canvas.textCentred(static_cast<float>(canvas.width()) / 2, 116 * k, "Touch one to read it",
                       kDim);
  }
}

std::optional<Touch> keepTouch(int width, std::size_t cards, int x, int y) {
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  const CardSprites sizes(width, kCardsScreenWidth);
  for (std::size_t i = 0; i < cards; ++i)
    if (cardBox(width, sizes.width(), sizes.height(), i, cards).contains(x, y))
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
                const PlayerNames &names) {
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
  button(canvas, backBox(canvas.width()), "Back", kRow, kEdge);
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
// the top, each's id its partner
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
// A row of the five resources' cards: where card r is, and its minus
Box tradeCard(int width, float y, int r) {
  const float k = scaleOf(width);
  const CardSprites sizes(width);
  return {(6 + 49 * static_cast<float>(r)) * k, y, static_cast<float>(sizes.width()),
          static_cast<float>(sizes.height())};
}
Box tradeMinus(int width, float y, int r) {
  const float k = scaleOf(width);
  const Box card = tradeCard(width, y, r);
  // (at the card's right, clear of how many you hold at its left)
  return {card.x + card.w - 15 * k, card.y + card.h + 2 * k, 13 * k, 12 * k};
}
// (yours, what you give, on the lower row, nearer you, as a hand of cards
// is held: the user's)
float giveRow(int width) { return 102 * scaleOf(width); }
float getRow(int width) { return 34 * scaleOf(width); }
Box declineBox(int width) {
  const float k = scaleOf(width);
  return {100 * k, 168 * k, 56 * k, 21 * k};
}
} // namespace

void drawTrade(Canvas &canvas, const TradeRoom &room, CardSprites &cards) {
  const float k = scaleOf(canvas);
  canvas.clear(kBackground);
  const int width = canvas.width();
  const bool answering = room.answering();
  if (!answering)
    tradeTabs(width, room)
        .draw(canvas,
              {.face = kRow, .edge = kEdge, .words = kWords, .labelScale = 1, .dimmed = 0.45f});
  else
    canvas.textCentred(static_cast<float>(width) / 2, 11 * k,
                       room.names()[room.from()] + (room.changed().empty()
                                                        ? " offers you a trade"
                                                        : " answers with a counter"),
                       kChosen);
  const auto row = [&](float y, const Resources &counts, std::string_view label, bool minus,
                       bool showHeld) {
    canvas.text(6 * k, y - 10 * k, label, kDim);
    for (int r = 0; r < kResources; ++r) {
      const Box box = tradeCard(width, y, r);
      const int n = counts[static_cast<std::size_t>(r)];
      canvas.blend(cards.small(cardOf(static_cast<Resource>(r))), cards.width(), cards.height(),
                   static_cast<int>(box.x), static_cast<int>(box.y), n > 0 ? 1.0f : 0.45f);
      if (n > 0) {
        // (how many in a badge at the card's foot, clear of its picture)
        canvas.outline(box.x - 1, box.y - 1, box.w + 2, box.h + 2, kChosen, 2);
        canvas.fillRect(box.x + box.w - 13 * k, box.y + box.h - 11 * k, 13 * k, 11 * k,
                        Colour{12, 16, 28, 255});
        canvas.textCentred(box.x + box.w - 6.5f * k, box.y + box.h - 5.5f * k, std::to_string(n),
                           kWords);
      }
      if (answering && std::find(room.changed().begin(), room.changed().end(),
                                 static_cast<Resource>(r)) != room.changed().end())
        canvas.outline(box.x - 3, box.y - 3, box.w + 6, box.h + 6, kChosen);
      if (minus) {
        const Box m = tradeMinus(width, y, r);
        canvas.fillRect(m.x, m.y, m.w, m.h, kRow);
        canvas.outline(m.x, m.y, m.w, m.h, kEdge);
        canvas.textCentred(m.x + m.w / 2, m.y + m.h / 2, "-", kWords);
      }
      if (showHeld)
        // (at the card's left, clear of the minus at its right)
        canvas.text(box.x + 3 * k, box.y + box.h + 4 * k,
                    "of " + std::to_string(room.held()[static_cast<std::size_t>(r)]), kDim);
    }
  };
  if (answering) {
    row(giveRow(width), room.theyGive(), "THEY GIVE YOU", false, false);
    row(getRow(width), room.theyWant(), "THEY WANT FROM YOU", false, false);
    button(canvas, declineBox(width), "Decline", Colour{74, 32, 32, 255}, kCannot);
    if (room.accept())
      button(canvas, confirmBox(width), "Accept", kConfirm, kConfirmEdge);
    else
      canvas.textCentred(224 * k, 178 * k, "can't afford", kCannot);
    return;
  }
  const bool bank = room.partner() == TradeRoom::kBank;
  row(giveRow(width), room.give(), bank ? "YOU GIVE (touch a kind)" : "YOU GIVE (touch to add)",
      !bank, true);
  row(getRow(width), room.get(), bank ? "YOU GET" : "YOU ASK FOR", !bank, false);
  button(canvas, backBox(width), "Back", kRow, kEdge);
  // The deal, or why not, between the buttons
  const std::string why = room.whyNot();
  canvas.textCentred(static_cast<float>(width) / 2, 160 * k, room.deal(), kWords);
  if (!why.empty())
    canvas.textCentred(static_cast<float>(width) / 2, 180 * k, why, kCannot);
  else
    button(canvas, confirmBox(width), bank ? "Trade" : "Offer", kConfirm, kConfirmEdge);
}

std::optional<TradeTouch> tradeTouch(int width, const TradeRoom &room, int x, int y) {
  using What = TradeTouch::What;
  if (room.answering()) {
    if (declineBox(width).contains(x, y))
      return TradeTouch{What::Decline};
    if (confirmBox(width).contains(x, y))
      return TradeTouch{What::Confirm};
    return std::nullopt;
  }
  if (const std::optional<int> partner = tradeTabs(width, room).at(x, y))
    return TradeTouch{What::Partner, *partner};
  if (backBox(width).contains(x, y))
    return TradeTouch{What::Back};
  if (confirmBox(width).contains(x, y))
    return TradeTouch{What::Confirm};
  for (const auto &[side, rowY] : {std::pair{TradeRoom::Side::Give, giveRow(width)},
                                   std::pair{TradeRoom::Side::Get, getRow(width)}})
    for (int r = 0; r < kResources; ++r) {
      if (tradeMinus(width, rowY, r).contains(x, y))
        return TradeTouch{What::Remove, 0, side, static_cast<Resource>(r)};
      if (tradeCard(width, rowY, r).contains(x, y))
        return TradeTouch{What::Add, 0, side, static_cast<Resource>(r)};
    }
  return std::nullopt;
}

} // namespace ChoiceScreens

} // namespace CosmicHex::Handheld
