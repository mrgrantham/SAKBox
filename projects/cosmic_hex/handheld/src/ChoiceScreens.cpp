#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/ChoiceScreens.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>

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
constexpr Colour kChosen{255, 214, 90, 255};
constexpr Colour kWords{226, 232, 244, 255};
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

std::string_view nameOfPlayer(int player) {
  static constexpr std::array<std::string_view, kMaxPlayers> kNames{"P1", "P2", "P3", "P4"};
  return kNames[static_cast<std::size_t>(player)];
}
} // namespace

CardSprites::CardSprites(int screenWidth)
    : width_(static_cast<int>(std::lround(48.0f * static_cast<float>(screenWidth) / 256.0f))),
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
  sprite.assign(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(width_ * height_));
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
                   CardSprites &cards) {
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
  }
  if (canGoBack)
    button(canvas, backBox(canvas.width()), "Back", kRow, kEdge);
  if (static_cast<int>(picked.size()) == choice.howMany && choice.actionFor(picked))
    button(canvas, confirmBox(canvas.width()), "Take", kConfirm, kConfirmEdge);
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

void drawPlayers(Canvas &canvas, const Pickers::PlayerChoice &choice, int chosen,
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
        half[static_cast<std::size_t>(y * bw + x)] =
            whole[static_cast<std::size_t>((y * 2) * cards.width() + x * 2)];
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
    for (int c = 0; c < std::min(option.cards, 16); ++c)
      canvas.blend(half, bw, bh, static_cast<int>(box.x + 72 * k + static_cast<float>(c) * 9 * k),
                   static_cast<int>(box.y + (box.h - static_cast<float>(bh)) / 2));
  }
  if (chosen >= 0)
    button(canvas, confirmBox(canvas.width()), "Steal", kConfirm, kConfirmEdge);
}

std::optional<Touch> playersTouch(int width, std::size_t players, int x, int y) {
  if (confirmBox(width).contains(x, y))
    return Touch{Touch::What::Confirm};
  for (std::size_t i = 0; i < players; ++i)
    if (playerRowBox(width, i).contains(x, y))
      return Touch{Touch::What::Item, static_cast<int>(i)};
  return std::nullopt;
}

} // namespace ChoiceScreens

} // namespace CosmicHex::Handheld
