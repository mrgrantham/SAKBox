#include <CosmicHex/Handheld/TopViews.hpp>

#include <CosmicHex/Dice/DiceTray.hpp>
#include <CosmicHex/Handheld/BoardMotion.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/CardFace.hpp>
#include <CosmicHex/Handheld/CardPlaces.hpp>
#include <CosmicHex/Handheld/CardsHeld.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/GameWords.hpp>
#include <CosmicHex/Handheld/Hud.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>

#include <ScratchPadd/Motion/Motion.hpp>
#include <ScratchPadd/Scene/TextWrap.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;
namespace Motion = ScratchPadd::Motion;

namespace {
// Words fading in (the user's: never just appearing): `colour` blended up
// from `under` (what they're over, as near as can be: the DS's text layer
// can't be see-through) by `amount`, 0 to 1
Scene::Colour fadedIn(Scene::Colour colour, float amount, Scene::Colour under = {8, 12, 22}) {
  const float a = std::clamp(amount, 0.0f, 1.0f);
  const auto mix = [a](std::uint8_t from, std::uint8_t to) {
    return static_cast<std::uint8_t>(std::lround(from + (to - from) * a));
  };
  return {mix(under.r, colour.r), mix(under.g, colour.g), mix(under.b, colour.b), colour.a};
}

constexpr float kPi = 3.14159265f;

// The largest scale from `largest` down to `smallest`, in four steps, that
// `text` fits `width` at as drawn (its letters' spacing included), as
// `measure` measures it, its line no taller than `height`; `smallest` if
// none does
float scaleToFit(std::string_view text, float width, float largest, float smallest,
                 TextMeasure measure = {}, float height = std::numeric_limits<float>::max()) {
  const float step = (largest - smallest) / 4;
  float scale = largest;
  while (scale > smallest &&
         (measure.width(text, scale) > width || measure.lineHeight(scale) > height))
    scale = std::max(smallest, scale - step);
  return scale;
}

// A flat picture covering screen pixels (x, y) to (x + w, y + h)
void picture(const TopFrame &f, Scene::Mesh square, Scene::Texture texture, float x, float y,
             float w, float h, float distance = kPictureDistance) {
  f.renderer.draw(square, onScreenAt(f.camera, f.width, f.height, x, y, w, h, distance),
                  {.shading = Scene::Material::Shading::Unlit, .texture = texture});
}
// A dark, see-through backdrop over the whole screen (in 3D, behind the
// pictures: the overlay's shapes would cover them), `top` down
void backdrop(const TopFrame &f, float top, Scene::Colour colour, float alpha,
              float distance = kPictureDistance * 1.2f) {
  f.renderer.draw(f.pictures.pictureSquare,
                  onScreenAt(f.camera, f.width, f.height, 0, top, f.width, f.height, distance),
                  {.shading = Scene::Material::Shading::Unlit,
                   .colour = colour,
                   .alpha = static_cast<std::uint8_t>(alpha)});
}

// How a card's face is drawn (cardFace)
struct CardLook {
  float turned = 1;                      // (over from its back: its width through |cos|; 1, flat)
  float title = 1;                       // (its title on its band fading in, 0 to 1; 0, none)
  float words = 0;                       // (what it does, under its art, fading in; 0, none)
  std::optional<Scene::Colour> titleInk; // (else its band's: CardFace::titleInkOf)
  std::uint8_t alpha = 31;
  float distance = kPictureDistance;
};

// A card's face, `w` wide (CardFace::heightOf tall) with its top left at
// (x, y), the one way every view draws a card (CardFace): its layers in one
// draw, its icon over them, and once it's flat the overlay's words: its
// short title on its band, as large as fits the band; what it does under
// its art, as many lines as fit
void cardFace(const TopFrame &f, Art::Card card, float x, float y, float w,
              const CardLook &look = {}) {
  const auto index = static_cast<std::size_t>(card);
  const float h = CardFace::heightOf(w), u = w / Art::kCardWidth;
  const float across = std::abs(std::cos(look.turned * kPi));
  const float cw = w * across, cx = x + (w - cw) / 2, cu = cw / Art::kCardWidth;
  if (cw < 0.5f)
    return;
  f.renderer.draw(f.pictures.cardLayers[index],
                  onScreenAt(f.camera, f.width, f.height, cx, y, cw, h, look.distance),
                  {.shading = Scene::Material::Shading::Unlit,
                   .texture = f.pictures.cardShapes,
                   .alpha = look.alpha});
  const Art::CardArea &icon = Art::kCardIcon;
  f.renderer.draw(f.pictures.pictureSquare,
                  onScreenAt(f.camera, f.width, f.height, cx + icon.x * cu, y + icon.y * u,
                             icon.width * cu, icon.height * u, look.distance - CardFace::kIconLift),
                  {.shading = Scene::Material::Shading::Unlit,
                   .texture = f.pictures.cardIcons[index],
                   .alpha = look.alpha});
  if (look.turned < 1)
    return;
  const float k = f.k();
  const Art::CardWords words = Art::wordsOf(card);
  if (look.title > 0) {
    const Scene::LineBox band{y + Art::kCardTitle.y * u,
                              y + (Art::kCardTitle.y + Art::kCardTitle.height) * u};
    const float scale = scaleToFit(words.shortTitle, Art::kCardTitle.width * u,
                                   std::max(1.0f, 2 * k), 1, TextMeasure{}, band.height());
    f.overlay.textCentred(x + w / 2, band.middle(), words.shortTitle,
                          fadedIn(look.titleInk.value_or(CardFace::titleInkOf(card)), look.title,
                                  CardFace::bandOf(card)),
                          scale);
  }
  if (look.words > 0) {
    // (wrapped by the width the letters take as drawn, spacing included;
    // stacked by the font's own line height, as many as the area holds)
    constexpr Scene::Colour kInk{40, 40, 48}, kCream{244, 241, 232};
    const Art::CardArea &area = Art::kCardWords;
    const float lineHeight = Scene::Overlay::lineHeight(k), bottom = y + (area.y + area.height) * u;
    float top = y + area.y * u;
    for (const auto &line :
         Scene::wrapText(words.words, area.width * u, [k](std::string_view line) {
           return Scene::Overlay::textWidth(line, k);
         })) {
      if (top + lineHeight > bottom)
        break;
      f.overlay.text(x + area.x * u, top, line, fadedIn(kInk, look.words, kCream), k);
      top += lineHeight + k;
    }
  }
}
} // namespace

// ---------------------------------------------------------------- Discovery

namespace {
// A number token's dots (one for each way two dice make it) over its chip,
// sharp: the chip's own are too small to read close up (the user's).
// make_number_chips.py's row: dots at 72 of the chip's 100, 14 apart
void tokenDots(const TopFrame &f, const Scene::Transform &pose, int number, float shown,
               float across) {
  const float dotsDown = kChipDown + 0.44f * kChipRadius; // (the row's middle)
  float x = 0, y = 0;
  if (!f.renderer.toScreen(pose.apply({0, kChipLift, dotsDown}), x, y))
    return;
  const int ways = 6 - std::abs(7 - number);
  const float spacing = 0.14f * across, size = std::max(2.0f, std::round(0.08f * across));
  constexpr Scene::Colour kChip{242, 234, 216}, kInk{30, 30, 36}; // (make_number_chips.py's)
  // (a band of the chip's colour over the blurred ones, then the dots)
  const float bandWide = (static_cast<float>(ways) - 1) * spacing + 2 * size;
  f.overlay.rectangle(x - bandWide / 2, y - size, bandWide, 2 * size, kChip);
  const float first = x - (static_cast<float>(ways) - 1) * spacing / 2;
  for (int i = 0; i < ways; ++i)
    f.overlay.rectangle(std::round(first + static_cast<float>(i) * spacing - size / 2),
                        std::round(y - size / 2), size, size, fadedIn(kInk, shown, kChip));
}

// A number token's number over its chip, on a tile at `pose` facing you
void tokenNumber(const TopFrame &f, const Scene::Transform &pose, int number, float shown) {
  const float digitsDown = kChipDown - 0.33f * kChipRadius; // (the digits' middle)
  float x = 0, y = 0, edgeX = 0, edgeY = 0;
  if (!f.renderer.toScreen(pose.apply({0, kChipLift, digitsDown}), x, y) ||
      !f.renderer.toScreen(pose.apply({kChipRadius, kChipLift, digitsDown}), edgeX, edgeY))
    return;
  const float across = 2 * std::hypot(edgeX - x, edgeY - y); // (the chip, on the screen)
  const std::string words = std::to_string(number);
  // (a whole scale, as the overlay draws sharpest, the digits' height)
  const float scale =
      std::max(1.0f, std::round(0.36f * across / Scene::Overlay::inkOf("8", 1).height));
  const auto ink = Scene::Overlay::inkOf(words, scale);
  constexpr Scene::Colour kChip{242, 234, 216}, kInk{30, 30, 36}; // (make_number_chips.py's)
  const float w = std::max(ink.width + 4, 0.62f * across), h = ink.height + 4;
  f.overlay.rectangle(x - w / 2, y - h / 2, w, h, kChip);
  f.overlay.textCentred(x, y, words, fadedIn(kInk, shown, kChip), scale);
  tokenDots(f, pose, number, shown, across);
}
} // namespace

DiscoveryLayout discoveryLayout(float width, float height, Scene::LineHeight lineHeight) {
  const float k = width / 256;
  // (room at the screen's edges, and between the words and the tile)
  const float margin = 3 * k, gap = 2 * k;
  constexpr float kMostOfTheHeight = 0.55f;
  DiscoveryLayout layout{.headingScale = k, .titleScale = 2 * k, .givesScale = 1.5f * k};
  const auto top =
      Scene::stackDown(margin, std::array{layout.headingScale, layout.titleScale}, gap, lineHeight);
  layout.heading = top[0];
  layout.title = top[1];
  // The tile is centred on the screen (it faces you along the camera's
  // line of sight), so the room it has is twice the nearer of the words
  // above and below the middle
  const float middle = height / 2;
  const float roomAbove = middle - layout.title.bottom - gap;
  const float roomBelow = height - margin - lineHeight(layout.givesScale) - gap - middle;
  const float half = std::min({kMostOfTheHeight * height / 2, roomAbove, roomBelow});
  layout.tileHeight = 2 * half;
  layout.gives =
      Scene::stackDown(middle + half + gap, std::array{layout.givesScale}, 0, lineHeight)[0];
  return layout;
}

DiscoveryDepths discoveryDepths(float tileShare, float fieldOfView, float tileRadius) {
  // (its corners 2 * tileRadius apart, `tileShare` of the view's height)
  const float tile = 2 * tileRadius / (tileShare * 2 * std::tan(fieldOfView * kPi / 360));
  // The backdrop behind it, clear of the tile as it turns over (its top and
  // bottom corners swing a radius towards and away from you) whatever size
  // the words leave the tile, and well in front of the board. At a fixed
  // distance it cut through the DS's tile as it turned, and was in front of
  // the 3DS's (smaller: its font's taller lines), dimming them
  constexpr float kClearOfItsTurn = 1.5f; // (radii)
  return {.tile = tile, .backdrop = tile + kClearOfItsTurn * tileRadius};
}

void drawDiscovery(const TopFrame &f, const DiscoveryShow &show,
                   const ScratchPadd::PictureSlot &closeUp,
                   const ScratchPadd::PictureSlot &closeUpBack, float wordsDim) {
  const auto moment = show.moment(f.now);
  const Discovery::Found *found = show.current();
  if (!moment || !found)
    return;
  const DiscoveryLayout layout = discoveryLayout(f.width, f.height);
  const float lifted = moment->lifted();
  // On the board: where it lies, flat (its axes the world's)
  const Scene::Vector3 onBoard = BoardPieces::centre(found->hex);
  const float radius = (BoardPieces::at({found->hex, 0}) - onBoard).length();
  // Facing you: in front of the camera, as tall as its words leave room
  // for (its corners `radius` from its middle), its face towards you, its
  // top up the screen
  const Scene::Camera &camera = f.camera;
  const DiscoveryDepths depths =
      discoveryDepths(layout.tileHeight / f.height, camera.fieldOfView, radius);
  // A dim backdrop while it faces you, behind it: in as it turns over, gone
  // early on its way back (the tile passes through its plane on the way up
  // and down)
  const float dim = moment->turn * std::max(0.0f, 1 - 4 * moment->down);
  if (dim > 0)
    backdrop(f, 0, {8, 12, 22}, 24 * dim, depths.backdrop);
  const auto [forward, right, screenUp] = viewAxes(camera);
  const Scene::Vector3 facing = camera.eye + forward * depths.tile;
  const Scene::Transform facingYou =
      Scene::Transform::fromAxes(forward.cross(screenUp), forward * -1, screenUp * -1, facing);
  // Between: its axes carried from one to the other (rigidBetween: flat
  // while it dips or settles, it stays rigid), its place along an arc
  // bowing up the screen (not a straight line)
  const Scene::Vector3 place = Motion::arc(onBoard, facing, screenUp * (0.8f * radius), lifted);
  const Scene::Transform pose =
      rigidBetween(Scene::Transform::translation(onBoard), facingYou, lifted, place);
  // Turning over about its x axis, as on the board: face down till halfway
  const float turn = moment->turn * kPi;
  const bool faceUp = turn > kPi / 2;
  const auto spin = Scene::Transform::rotation({1, 0, 0}, faceUp ? turn - kPi : turn);
  const Art::Face face = faceUp ? found->face : found->faceDown;
  // (each side sharp when its close-up holds it: the face, or the back
  // flying up face down; else the board's own picture)
  const ScratchPadd::PictureSlot &sharp = faceUp ? closeUp : closeUpBack;
  const Scene::Texture texture = sharp.showing(static_cast<int>(face))
                                     ? sharp.texture()
                                     : f.pictures.faces[static_cast<std::size_t>(face)];
  f.renderer.draw(
      f.pictures.hexTile, pose * spin,
      texture ? Scene::Material{.shading = Scene::Material::Shading::Toon, .texture = texture}
              : Scene::Material{.shading = Scene::Material::Shading::Toon,
                                .colour = BoardColours::ofFace(face)});
  // Its number token dropped on from above once it's face up (from up the
  // screen while it faces you), riding it back down
  const Scene::Texture chip = f.pictures.chips[static_cast<std::size_t>(found->number)];
  if (found->number > 0 && moment->turn >= 1 && chip) {
    const float drop = (1 - moment->chip) * 2.5f * radius;
    f.renderer.draw(f.pictures.numberChip, pose * Scene::Transform::translation({0, 0, -drop}),
                    Scene::Material{.shading = Scene::Material::Shading::Toon, .texture = chip});
  }
  // Its words, while it faces you (dimmed under salvage's cards)
  if (lifted > 0.98f) {
    const float shown = 1 - std::clamp(wordsDim, 0.0f, 1.0f);
    f.overlay.textCentred(f.width / 2, layout.heading.middle(), f.names[found->by] + " discovers",
                          fadedIn(BoardColours::ofPlayer(found->by), shown), layout.headingScale);
    // (once it's turned over, fading in)
    if (moment->named > 0) {
      f.overlay.textCentred(f.width / 2, layout.title.middle(), found->name,
                            fadedIn(Palette::kWords, moment->named * shown), layout.titleScale);
      f.overlay.textCentred(f.width / 2, layout.gives.middle(), found->gives,
                            fadedIn(Palette::kSoftWords, moment->named * shown), layout.givesScale);
    }
    // The token's number written sharp over its picture once it's landed
    // (the user's A: a small texture's digits blur; the words are crisp
    // on every screen), on the chip's own cream, where its digits are (the
    // upper part: 20 to 56 of its picture's 100)
    // (once the token's down: its drop overshoots, so not as it first
    // reaches its place)
    if (found->number > 0 && moment->numbered > 0)
      tokenNumber(f, pose, found->number, moment->numbered);
  }
}

// ------------------------------------------------------------------- Dealt

namespace {
constexpr float kDealtTop = 8, kDealtGap = 16; // (a DS's pixels)
// The hint under the cards dealt: its line's middle
float dealtHintY(float height, float k) { return height - 6 * k; }

// While a player chooses resources: what the screen says
std::string_view choiceWords(bool yours) { return yours ? "Your choice: below" : "Choosing..."; }

// A resource card shown big, then flown to its player (a home's pay,
// salvage): it grows out of `from` up to `held` (its middle; a pop:
// Settle), `width` wide when full size, its name on its band once it is;
// then winds up and goes off along an arc bowing by `bow` to `to`
// (CardPlaces), shrinking
struct FlyingCard {
  Resource resource{};
  float width = 0;
  ScreenPoint from, held, to, bow;
  float grown = 0, away = 0, words = 0; // (each 0 to 1, through time)
};
void flyingCard(const TopFrame &f, const FlyingCard &card) {
  const float grown = Motion::along(Motion::Curve::Settle, card.grown);
  const float leave = Motion::along(Motion::Curve::Anticipate, card.away);
  const float w = card.width * grown * (1 - 0.8f * std::max(0.0f, leave));
  const float h = CardFace::heightOf(w);
  const float rose = std::min(1.0f, grown);
  const float riseX = card.from.x + (card.held.x - card.from.x) * rose;
  const float riseY = card.from.y + (card.held.y - card.from.y) * rose;
  const float cx = Motion::arc(riseX, card.to.x, card.bow.x, leave);
  const float cy = Motion::arc(riseY, card.to.y, card.bow.y, leave);
  // (its name on its band once it's full size, fading in)
  if (w > 1)
    cardFace(f, static_cast<Art::Card>(card.resource), cx - w / 2, cy - h / 2, w,
             {.title = card.away <= 0 ? card.words : 0});
}
// How a card's flight off to its player bows: down to your hand below, a
// swing out and up; up to a bot's row of the points bars, out sideways
ScreenPoint flightBow(bool toYourHand, float k) {
  return toYourHand ? ScreenPoint{18 * k, -30 * k} : ScreenPoint{-24 * k, 0};
}
} // namespace

CardBox dealtCardBox(float width, float height, std::size_t i, std::size_t count) {
  const float k = width / 256, n = static_cast<float>(std::max<std::size_t>(count, 1));
  const float gap = kDealtGap * k, top = kDealtTop * k;
  const float bottom = dealtHintY(height, k) - Scene::Overlay::lineHeight(k) / 2 - 4 * k;
  const float w =
      std::min(100 * k, CardFace::widthToFit((width - 8 * k - (n - 1) * gap) / n, bottom - top));
  const float left = (width - (n * w + (n - 1) * gap)) / 2;
  return {left + static_cast<float>(i) * (w + gap), top, w, CardFace::heightOf(w)};
}

void drawDealtCards(const TopFrame &f, const DealtShow &show, int chosen) {
  const std::vector<Art::Card> &cards = show.cards();
  if (!show.on() || cards.empty())
    return;
  const float k = f.k();
  f.overlay.textCentred(f.width / 2, dealtHintY(f.height, k), "Keep one: touch it below",
                        Palette::kDimWords, k);
  for (std::size_t i = 0; i < cards.size(); ++i) {
    const CardBox home = dealtCardBox(f.width, f.height, i, cards.size());
    const float w = home.width, cardH = home.height, homeX = home.x, top = home.y;
    // Thrown in from the bottom left along an arc (bowing up), landing
    // with a settle (DealtShow::thrown)
    const float flown = show.thrown(i, f.now);
    const float x = Motion::arc(-w, homeX, -6 * k, flown);
    const float y = Motion::arc(f.height, top, -30 * k, flown);
    // Then turned over: its width through |cos|, its back until halfway
    const float turned = show.turned(i, f.now);
    if (turned < 0.5f) {
      const float across = std::abs(std::cos(turned * kPi));
      const float cw = w * across, cx = x + w * (1 - across) / 2;
      picture(f, f.pictures.backSquare,
              f.pictures.backs[static_cast<std::size_t>(Art::kActionBack)], cx, y, cw, cardH);
      continue;
    }
    const float words = show.words(i, f.now);
    cardFace(f, cards[i], x, y, w, {.turned = turned, .title = words, .words = words});
    if (turned < 1)
      continue;
    if (static_cast<int>(i) == chosen) {
      f.overlay.rectangle(x - 3 * k, y - 3 * k, w + 6 * k, 2 * k, Palette::kHighlight);
      f.overlay.rectangle(x - 3 * k, y + cardH + k, w + 6 * k, 2 * k, Palette::kHighlight);
      f.overlay.rectangle(x - 3 * k, y - 3 * k, 2 * k, cardH + 6 * k, Palette::kHighlight);
      f.overlay.rectangle(x + w + k, y - 3 * k, 2 * k, cardH + 6 * k, Palette::kHighlight);
    }
  }
}

// -------------------------------------------------------------- Card play

CardBox playedCardBox(float width, float height) {
  // (as big as fits under its words at the top)
  const float k = width / 256;
  const float w = std::min(90 * k, CardFace::widthToFit(width, height - 24 * k));
  const float h = CardFace::heightOf(w), midY = height / 2 + 6 * k;
  return {width / 2 - w / 2, midY - h / 2, w, h};
}

void drawCardPlay(const TopFrame &f, const CardPlayShow &show, const State &state, int you) {
  const auto m = show.at(f.now);
  const CardPlayShow::Play *play = show.current();
  if (!m || !play)
    return;
  const float k = f.k();
  backdrop(f, 0, {8, 12, 22}, 12 * std::min(1.0f, m->up * 2) * (1 - m->away));
  // From its player (your hand below, or their row of the points bars) up
  // to the middle, face down until it turns; then off up and away
  const CardBox held = playedCardBox(f.width, f.height);
  const float w = held.width, cardH = held.height;
  const auto [fromX, fromY] = CardPlaces::at(f, state, play->player, you);
  const float midX = held.x + w / 2, midY = held.y + cardH / 2;
  const float cx =
      Motion::arc(Motion::arc(fromX, midX, -10 * k, m->up), f.width + w, 0.0f, m->away);
  const float cy = Motion::arc(Motion::arc(fromY, midY, -20 * k, m->up), -cardH, -20 * k, m->away);
  const float size = std::max(0.2f, std::min(1.0f, m->up)) * (1 - 0.5f * m->away);
  const float cw = w * size;
  const float x = cx - cw / 2, y = cy - CardFace::heightOf(cw) / 2;
  if (m->turned < 0.5f) {
    const float across = std::abs(std::cos(m->turned * kPi));
    picture(f, f.pictures.backSquare, f.pictures.backs[static_cast<std::size_t>(Art::kActionBack)],
            x + cw * (1 - across) / 2, y, cw * across, CardFace::heightOf(cw));
  } else {
    // (its words only while it's held flat, not as it flies away)
    const float turned = m->away > 0 ? 0.999f : m->turned;
    cardFace(f, play->card, x, y, cw, {.turned = turned, .title = m->words, .words = m->words});
  }
  if (m->away <= 0) {
    const bool charter = play->card >= Art::Card::CharterThreeOfAKind;
    const std::string who = play->player == you ? "You" : f.names[play->player];
    f.overlay.textCentred(f.width / 2, 8 * k,
                          who + (charter ? " reveal" : " play") + (play->player == you ? "" : "s") +
                              (charter ? " a Charter" : ""),
                          BoardColours::ofPlayer(play->player), k);
  }
  (void)state;
}

// -------------------------------------------------------------------- Roll

// The dice on the glass, in front of the board; once they've settled, the
// roll in big words in the middle of the screen on a hexagon (the user's),
// going with the glass as it's pulled away
void drawRoll(const TopFrame &f, const RollDice &dice) {
  if (dice.idle() || !f.pictures.die)
    return;
  const Dice::DiceTray &tray = dice.tray();
  const auto toFloat = [](cyclone::real r) { return static_cast<float>(static_cast<double>(r)); };
  const Scene::Transform glass =
      glassFrame(f.camera, f.width / f.height, static_cast<float>(Dice::DiceTray::kWidth),
                 static_cast<float>(Dice::DiceTray::kDepth));
  for (int d = 0; d < 2; ++d) {
    const cyclone::Vector3 at = tray.position(d);
    const cyclone::Quaternion turn = tray.orientation(d);
    f.renderer.draw(f.pictures.die,
                    dieOnGlass(glass, {toFloat(at.x), toFloat(at.y), toFloat(at.z)},
                               {toFloat(turn.r), toFloat(turn.i), toFloat(turn.j), toFloat(turn.k)},
                               static_cast<float>(tray.offset())),
                    {.shading = Scene::Material::Shading::Lit});
  }
  // Once they've settled, the roll in big words in the middle of the
  // screen on a hexagon (the user's), going with the glass as
  // it's pulled away (its middle, wherever the glass has got to)
  if (dice.stage() == RollDice::Stage::Rolling)
    return;
  const auto middle = onScreen(f.camera, f.width, f.height,
                               glass.apply({-static_cast<float>(tray.offset()), 0, 0}));
  if (!middle)
    return;
  const auto [first, second] = dice.rolled();
  char total[8], pair[16];
  std::snprintf(total, sizeof total, "%d", first + second);
  std::snprintf(pair, sizeof pair, "%d + %d", first, second);
  Scene::Overlay &overlay = f.overlay;
  // (the user's: a little smaller, so the words sit well inside)
  const float big = f.width >= 400 ? 4 : 3; // (the 3DS's wider screen: bigger)
  const float small = f.width >= 400 ? 1.5f : 1;
  const float radius = f.width >= 400 ? 50.0f : 42.0f;
  const float x = (*middle)[0], y = (*middle)[1];
  // Popping in as the dice settle (the user's: as the home's card does):
  // the hexagon growing a little past its size and settling; then, once
  // it has, its words fading in (never on something still moving)
  const float popped = Motion::along(Motion::Curve::Settle,
                                     std::min(1.0f, dice.shownSeconds() / RollDice::kSecondsPop));
  // Both solid: a see-through hexagon's six triangles were blended twice
  // along the edges they share, which showed as dark lines
  constexpr Scene::Colour kInside{12, 18, 30, 255};
  overlay.hexagon(x, y, (radius + 3) * popped, Palette::kHighlight); // (an amber edge)
  overlay.hexagon(x, y, radius * popped, kInside);
  const float words =
      dice.stage() == RollDice::Stage::Showing
          ? std::clamp((dice.shownSeconds() - RollDice::kSecondsPop) / kSecondsWordsIn, 0.0f, 1.0f)
          : 1.0f;
  if (words <= 0)
    return;
  const auto faded = [&](Scene::Colour c) {
    const auto mix = [words](std::uint8_t from, std::uint8_t to) {
      return static_cast<std::uint8_t>(std::lround(from + (to - from) * words));
    };
    return Scene::Colour{mix(kInside.r, c.r), mix(kInside.g, c.g), mix(kInside.b, c.b), 255};
  };
  // The total over the two dice, the two lines together centred on the
  // hexagon by their letters' own pixels (Overlay::textCentred)
  const auto totalInk = Scene::Overlay::inkOf(total, big),
             pairInk = Scene::Overlay::inkOf(pair, small);
  constexpr float kGap = 6;
  const float height = totalInk.height + kGap + pairInk.height;
  overlay.textCentred(x, y - height / 2 + totalInk.height / 2, total, faded(Palette::kHighlight),
                      big);
  overlay.textCentred(x, y + height / 2 - pairInk.height / 2, pair, faded(Palette::kWords), small);
}

// --------------------------------------------------------- Show resources

namespace {
// The five piles of resource cards along the bottom (Show resources' and
// cards gained's): `heldOf` each; each card's height by `showSlid` (0 below
// the screen, 1 in place: by its order across the piles), and a pile's
// newest `fresh` cards, at its back, by `dropped` (0 above the screen, 1
// in place). The order counted on past the piles, for what's drawn next
// A DS's pixels between a pile's cards (their bands show)
constexpr float kPileStep = 5;

int drawPiles(const TopFrame &f, const Resources &heldOf, const std::function<float(int)> &showSlid,
              const Resources &fresh, const std::function<float(Resource, int)> &dropped) {
  const float k = f.k();
  const TopPictures &p = f.pictures;
  const auto rw = static_cast<float>(p.resourceWidth), rh = CardFace::heightOf(rw);
  // Five piles across with a little room between, their bottoms just above
  // the screen's edge
  const float pileGap = 6 * k;
  const float pilesLeft = (f.width - (5 * rw + 4 * pileGap)) / 2;
  const float pileY = f.height - rh - 6 * k;
  int order = 0;
  for (int r = 0; r < kResources; ++r) {
    const int held = heldOf[static_cast<std::size_t>(r)];
    const int shown = std::max(1, std::min(held, 5));
    const float x = pilesLeft + static_cast<float>(r) * (rw + pileGap);
    const int newest = std::min(fresh[static_cast<std::size_t>(r)], shown);
    for (int j = shown - 1; j >= 0; --j) {
      // (each card under the front one 5 pixels higher: its band shows)
      const float y = pileY - static_cast<float>(j) * kPileStep * k;
      float slid = y + (1 - showSlid(order++)) * (f.height - y + 4);
      // (a newly gained card, at the back: dropping down into its place)
      if (j >= shown - newest)
        slid -= (1 - dropped(static_cast<Resource>(r), shown - 1 - j)) * (slid + rh + 4);
      // (the title on the front card's band, in its band's ink, faint
      // when there are none)
      cardFace(
          f, static_cast<Art::Card>(r), x, slid, rw,
          {.title = j == 0 ? 1.0f : 0.0f,
           .titleInk = held == 0 ? std::optional(Scene::Colour{120, 128, 145}) : std::nullopt,
           .alpha = static_cast<std::uint8_t>(held == 0 ? 10 : 31),
           .distance = kPictureDistance - CardFace::kCardStep * static_cast<float>(shown - j)});
    }
    // Past five, how many in a badge on the front card, under its art
    if (held > 5) {
      const float front = pileY + (1 - showSlid(order - 1)) * (f.height - pileY + 4);
      const float bw = 14 * k, bh = 10 * k;
      const float by = front + (Art::kCardWords.y + 4) * rw / Art::kCardWidth;
      f.overlay.rectangle(x + rw / 2 - bw / 2, by, bw, bh, {58, 74, 106});
      f.overlay.textCentred(x + rw / 2, by + bh / 2, std::to_string(held), Palette::kWords);
    }
  }
  return order;
}
} // namespace

void drawResources(const TopFrame &f, const Player &player, const CardsShow &show) {
  if (!show.showing(f.now))
    return;
  const float k = f.k();
  const TopPictures &p = f.pictures;
  int order = drawPiles(
      f, player.resources, [&](int o) { return show.arrived(o, f.now); }, Resources{},
      [](Resource, int) { return 1.0f; });
  // The action cards across the top, smaller, each easing down, their
  // short titles under them
  std::vector<Art::Card> actions;
  for (const int id : player.contracts)
    if (const auto card = cardOf(id))
      actions.push_back(*card);
  // (as big as fits between the HUD's top rows and the tallest piles, with
  // their titles under them: the 3DS's screen is shorter for its width)
  const auto rw = static_cast<float>(p.resourceWidth);
  const float pilesTop = f.height - 6 * k - CardFace::heightOf(rw) - 4 * kPileStep * k;
  const float y = 28 * k, titleRoom = Scene::Overlay::lineHeight(1) + 6 * k;
  const float w =
      CardFace::widthToFit(static_cast<float>(p.cardWidth), pilesTop - 2 * k - titleRoom - y);
  const float h = CardFace::heightOf(w);
  const float gap = 8 * k, total = static_cast<float>(actions.size()) * (w + gap) - gap;
  for (std::size_t i = 0; i < actions.size(); ++i) {
    const float x = (f.width - total) / 2 + static_cast<float>(i) * (w + gap);
    const float in = show.arrived(order++, f.now);
    cardFace(f, actions[i], x, y - (1 - in) * (y + h + 4), w, {.title = 0});
    f.overlay.textCentred(x + w / 2, y + h + titleRoom / 2, Art::wordsOf(actions[i]).shortTitle,
                          Palette::kWords);
  }
}

void drawGained(const TopFrame &f, const Resources &held, const HandShow &show) {
  if (!show.showing(f.now))
    return;
  const float up = show.piles(f.now);
  drawPiles(
      f, held, [up](int) { return up; }, show.added(),
      [&](Resource r, int index) { return show.card(r, index, f.now); });
}

// ------------------------------------------------------------ Other player

float OtherPlayerLayout::cardsBottom() const {
  return cardsTop + rowStep * static_cast<float>(cardRows - 1) + cardHeight;
}

float OtherPlayerLayout::fanStep(int inRow) const {
  if (inRow <= 1)
    return 0;
  // (at most two fifths of a card apart, so the backs read as a hand)
  constexpr float kWidest = 0.4f;
  return std::min(kWidest * cardWidth, (columnWidth - cardWidth) / static_cast<float>(inRow - 1));
}

float OtherPlayerLayout::planetsBottom() const {
  if (planetsShown == 0)
    return planetsLabel.bottom;
  const int rows = (planetsShown + planetsARow - 1) / planetsARow;
  return planetsY + planetRowStep * static_cast<float>(rows - 1) + planetRadius;
}

std::string chartersLine(const PlayerView &view) {
  if (view.charters.empty())
    return {};
  std::string line = "Charters:";
  for (const Art::Card card : view.charters)
    line += " " + std::string(Art::wordsOf(card).shortTitle);
  return line;
}

OtherPlayerLayout otherPlayerLayout(const PlayerView &view, std::string_view name, float width,
                                    float height, TextMeasure measure) {
  OtherPlayerLayout l;
  // Words the size every view's are (the screen's width over a DS's);
  // shapes and room in a DS's points scaled as far as the screen goes both
  // ways (the 3DS's is wider for its height)
  const float k = width / 256, u = std::min(k, height / 192);
  l.textScale = k;
  l.unit = u;
  const auto line = [&](float scale) { return measure.lineHeight(scale); };
  const float margin = 3 * u, inside = 7 * u, gap = 2 * u, between = 5 * u;
  l.margin = margin;
  l.frameTop = margin;
  l.frameBottom = height - margin - 1;
  l.left = margin + inside;
  l.right = width - margin - inside;
  const float top = l.frameTop + inside, bottom = l.frameBottom - margin;

  // The cards' columns, where a DS's were (84 and 170 of its 256), as
  // wide as the narrower leaves; the name fitted to the room left of them
  l.resourcesX = width * 84 / 256;
  l.actionsX = width * 170 / 256;
  l.columnWidth = std::min(l.actionsX - l.resourcesX - gap, l.right - l.actionsX);
  l.nameScale = scaleToFit(name, l.resourcesX - gap - l.left, 1.5f * k, k, measure);
  const auto header = Scene::stackDown(top, std::array{l.nameScale, k}, gap, measure.lineHeight);
  l.name = header[0];
  l.points = header[1];
  // (both labels at one scale, the longer fitting its column; where it
  // won't at the smallest, a shorter word: a pixel font shrinks only so far)
  const auto labelScale = [&](const std::string &a, const std::string &b) {
    const std::string &longer = measure.width(a, k) < measure.width(b, k) ? b : a;
    return scaleToFit(longer, l.columnWidth, k, 0.6f * k, measure);
  };
  const auto fits = [&](const std::string &text, float scale) {
    return measure.width(text, scale) <= l.columnWidth;
  };
  const std::string cards = std::to_string(view.actionCards);
  for (const std::string_view word : {"RESOURCES", "RES"}) {
    l.resourcesLabel = std::string(word) + " " + std::to_string(view.resourceCards);
    l.actionsLabel = "CARDS " + cards;
    l.labelScale = labelScale(l.resourcesLabel, l.actionsLabel);
    if (fits(l.resourcesLabel, l.labelScale) && fits(l.actionsLabel, l.labelScale))
      break;
  }
  l.cardLabels = Scene::stackDown(top, std::array{l.labelScale}, 0, measure.lineHeight)[0];
  l.cardsTop = l.cardLabels.bottom + gap;

  // The Charters along the bottom, fitted across
  const std::string charters = chartersLine(view);
  l.chartersScale =
      charters.empty() ? k : scaleToFit(charters, l.right - l.left, k, 0.6f * k, measure);
  l.charters = charters.empty()
                   ? Scene::LineBox{bottom, bottom}
                   : Scene::stackUp(bottom, std::array{l.chartersScale}, 0, measure.lineHeight)[0];

  // The planets: hexagons a DS's size, or big enough for two digits in
  // this font; as many a row as fit right of their label, two rows
  constexpr float kSqrt3 = 1.7320508f; // (a pointy hexagon's width over its radius)
  l.planetRadius = std::max(12 * u, (measure.width("88", k) + 2 * gap) / kSqrt3);
  l.planetStep = kSqrt3 * l.planetRadius + 4 * u;
  l.planetRowStep = 2 * l.planetRadius + u;
  const float labelWide = measure.width("PLANETS", k);
  l.planetsX = l.left + labelWide + 3 * gap + kSqrt3 * l.planetRadius / 2;
  l.planetsARow = std::max(
      1, 1 + static_cast<int>((l.right - kSqrt3 * l.planetRadius / 2 - l.planetsX) / l.planetStep));
  const int planets = static_cast<int>(view.planets.size());
  l.planetsShown = std::min(planets, OtherPlayerLayout::kPlanetRows * l.planetsARow);
  const int planetRows = std::max(1, (l.planetsShown + l.planetsARow - 1) / l.planetsARow);
  const float planetsHigh =
      planets == 0 ? line(k)
                   : 2 * l.planetRadius + l.planetRowStep * static_cast<float>(planetRows - 1);
  // The pieces: icons a DS's size, a count's line on them, names under
  l.pieceHalf = 6 * u;
  const float piecesHigh = std::max(2 * l.pieceHalf, l.pieceHalf + line(k) / 2) + gap + line(k);

  // The cards take what's left between the header and the pieces: as big
  // as a DS's (36 wide) at most, half their column's room at most (six
  // still fan), the second row showing the first's top band
  constexpr float kRowShows = 0.36f; // (of a card's height: its band and a little)
  const auto rowsOf = [](int count) {
    const int shown = std::min(count, OtherPlayerLayout::kCardsShown);
    return std::max(1, (shown + OtherPlayerLayout::kCardsARow - 1) / OtherPlayerLayout::kCardsARow);
  };
  l.cardRows = std::max(rowsOf(view.resourceCards), rowsOf(view.actionCards));
  const float sectionsBelow = between + piecesHigh + between + planetsHigh +
                              (charters.empty() ? 0 : between + l.charters.height());
  const float room = bottom - sectionsBelow - l.cardsTop;
  const float tallest = room / (1 + kRowShows * static_cast<float>(l.cardRows - 1));
  l.cardWidth = std::max(0.0f, CardFace::widthToFit(std::min(36 * u, l.columnWidth / 2), tallest));
  l.cardHeight = CardFace::heightOf(l.cardWidth);
  l.rowStep = kRowShows * l.cardHeight;

  // Each under the one before; what room's spare left at the bottom
  const float piecesTop = std::max(l.cardsBottom(), l.points.bottom) + between;
  l.piecesMiddle = piecesTop + l.pieceHalf;
  for (std::size_t i = 0; i < l.piecesX.size(); ++i)
    l.piecesX[i] = l.left + static_cast<float>(i) * (l.right - l.left) / 4;
  l.pieceCounts = Scene::stackAround(l.piecesMiddle, std::array{k}, 0, measure.lineHeight)[0];
  l.pieceNames =
      Scene::stackDown(std::max(l.piecesMiddle + l.pieceHalf, l.pieceCounts.bottom) + gap,
                       std::array{k}, 0, measure.lineHeight)[0];
  const float planetsTop = l.pieceNames.bottom + between;
  l.planetsY = planetsTop + (planets == 0 ? line(k) / 2 : l.planetRadius);
  l.planetsLabel = Scene::stackAround(l.planetsY, std::array{k}, 0, measure.lineHeight)[0];
  return l;
}

void drawOtherPlayer(const TopFrame &f, const PlayerView &view, float down, float across) {
  Scene::Overlay &overlay = f.overlay;
  const std::string name = f.names.shortOf(view.player);
  const OtherPlayerLayout l = otherPlayerLayout(view, name, f.width, f.height);
  const float k = l.textScale, u = l.unit;
  const float top = -f.height * (1 - down);
  const Scene::Colour colour = BoardColours::ofPlayer(view.player);
  // (the backdrop once, behind both panels while they slide past)
  if (across <= 0)
    backdrop(f, top, {12, 18, 32}, 29);
  // (everything on it slid sideways together, for the carousel)
  const Scene::Overlay::Mark start = overlay.mark();
  overlay.rectangle(l.margin, top + l.frameTop, f.width - 2 * l.margin, 1, colour);
  overlay.rectangle(l.margin, top + l.frameBottom, f.width - 2 * l.margin, 1, colour);
  // Name and points
  overlay.text(l.left, top + l.name.top, name, colour, l.nameScale);
  overlay.text(l.left, top + l.points.top,
               std::to_string(view.points) + (view.points == 1 ? " point" : " points"),
               Palette::kWords, k);
  // Their cards, face down, in the agreed backs: fanned, six a row, a
  // second row under the first when there are more (up to 12 shown), the
  // count by the label
  const auto backs = [&](float x, int count, const std::string &label, Art::CardBack design) {
    overlay.text(x, top + l.cardLabels.top, label, Palette::kDimWords, l.labelScale);
    const Scene::Texture back = f.pictures.backs[static_cast<std::size_t>(design)];
    if (!f.pictures.backSquare || !back)
      return;
    const int shown = std::min(count, OtherPlayerLayout::kCardsShown);
    for (int i = 0; i < shown; ++i) {
      const int row = i / OtherPlayerLayout::kCardsARow;
      const int inRow =
          std::min(shown - row * OtherPlayerLayout::kCardsARow, OtherPlayerLayout::kCardsARow);
      // (the cards slide with the panel: they're pictures, not the overlay's)
      picture(f, f.pictures.backSquare, back,
              x + (across * f.width) +
                  static_cast<float>(i % OtherPlayerLayout::kCardsARow) * l.fanStep(inRow),
              top + l.cardsTop + static_cast<float>(row) * l.rowStep, l.cardWidth, l.cardHeight,
              kPictureDistance * (1 - 0.002f * static_cast<float>(i)));
    }
  };
  backs(l.resourcesX, view.resourceCards, l.resourcesLabel, Art::kResourceBack);
  backs(l.actionsX, view.actionCards, l.actionsLabel, Art::kActionBack);
  // Badges, ships, stations and colonies as the board draws them (the
  // user's): each a group of its pieces, up to four, or one and how many;
  // its name small under it
  const float y = top + l.piecesMiddle, h = l.pieceHalf;
  const auto pieces = [&](std::size_t group, int count, std::string_view name, auto &&drawOne) {
    const float x = l.piecesX[group];
    constexpr int kMostDrawn = 4;
    for (int i = 0; i < (count > kMostDrawn ? 1 : count); ++i)
      drawOne(x + h + static_cast<float>(i) * 2 * (h - 0.5f * u), y);
    if (count > kMostDrawn)
      overlay.text(x + 2 * h + 2 * u, top + l.pieceCounts.top, "x" + std::to_string(count),
                   Palette::kWords, k);
    if (count == 0)
      overlay.text(x, top + l.pieceCounts.top, "none", Palette::kDimWords, k);
    overlay.text(x, top + l.pieceNames.top, name, Palette::kDimWords, k);
  };
  pieces(0, view.badges, "badges",
         [&](float x, float yy) { drawBadge(overlay, x, yy, 0.9f * h, true); });
  pieces(1, view.ships, "ships", [&](float x, float yy) {
    overlay.triangle(x, yy - 0.8f * h, x - 0.75f * h, yy + 0.7f * h, x + 0.75f * h, yy + 0.7f * h,
                     colour);
  });
  pieces(2, view.stations, "stations", [&](float x, float yy) {
    overlay.rectangle(x - 0.6f * h, yy - 0.6f * h, 1.2f * h, 1.2f * h, colour);
  });
  pieces(3, view.colonies, "colonies", [&](float x, float yy) {
    overlay.rectangle(x - 0.75f * h, yy - 0.75f * h, 1.5f * h, 1.5f * h, colour);
    overlay.rectangle(x - 0.25f * h, yy - 0.25f * h, 0.5f * h, 0.5f * h, {20, 22, 30});
  });
  // Their planets: a hexagon each in its kind's colour, its number on it;
  // past what fits, the last place says how many more
  overlay.text(l.left, top + l.planetsLabel.top, "PLANETS", Palette::kDimWords, k);
  if (view.planets.empty())
    overlay.text(l.planetsX - l.planetRadius, top + l.planetsLabel.top, "none yet",
                 Palette::kDimWords, k);
  const int planets = static_cast<int>(view.planets.size());
  for (int i = 0; i < l.planetsShown; ++i) {
    const int row = i / l.planetsARow, column = i % l.planetsARow;
    const float x = l.planetsX + static_cast<float>(column) * l.planetStep;
    const float py = top + l.planetsY + static_cast<float>(row) * l.planetRowStep;
    if (i == l.planetsShown - 1 && planets > l.planetsShown) {
      overlay.textCentred(x, py, "+" + std::to_string(planets - i), Palette::kWords, k);
      continue;
    }
    const PlayerView::Planet &planet = view.planets[static_cast<std::size_t>(i)];
    overlay.hexagon(x, py, l.planetRadius, BoardColours::ofKind(planet.kind), true);
    if (planet.colony)
      overlay.hexagon(x, py, 2 * l.planetRadius / 3, colour, true);
    overlay.textCentred(x, py, std::to_string(planet.number), {20, 22, 30}, k);
  }
  // Their revealed Charters
  if (!view.charters.empty())
    overlay.text(l.left, top + l.charters.top, chartersLine(view), Palette::kWords,
                 l.chartersScale);
  overlay.moveSince(start, across * f.width, 0);
}

// -------------------------------------------------------------- First roll

namespace {
// A number rolling up into place (the user's): the number rolled sliding
// up into a window just its own height, past where it settles and back
// (Settle's overshoot), over 0.4 s from `seconds` 0, cut to the window
// (Overlay::clipText)
void odometer(const TopFrame &f, float x, float y, int value, float seconds, Scene::Colour colour) {
  const float k = f.k(), scale = std::round(1.5f * k);
  const float high = Scene::Overlay::inkOf("8", scale).height;
  constexpr float kSeconds = 0.4f;
  const float t = std::clamp(seconds / kSeconds, 0.0f, 1.0f);
  // (from a line below to its place: past 1, a little above it)
  const float below = (1 - Motion::along(Motion::Curve::Settle, t)) * (high + 3 * k);
  f.overlay.clipText({.top = y - high / 2 - 1 * k, .bottom = y + high / 2 + 1 * k});
  f.overlay.textCentred(x, y + below, std::to_string(value), colour, scale);
  f.overlay.clipText({});
}
} // namespace

void drawFirstRoll(const TopFrame &f, const FirstRoll &roll, const FirstRollShow &show) {
  if (roll.rounds.empty())
    return;
  const float k = f.k();
  Scene::Overlay &overlay = f.overlay;
  // Fading in over the board: the backdrop darkening, the words coming in
  // and settling down into place (the user's: it popped up)
  const float in = show.appeared(f.now);
  const float drop = -12 * k * (1 - Motion::along(Motion::Curve::Settle, in));
  // A dark backdrop behind the glass (in 3D: the overlay's shapes would
  // cover the dice), in front of the board
  f.renderer.draw(f.pictures.pictureSquare,
                  onScreenAt(f.camera, f.width, f.height, 0, 0, f.width, f.height, 7.5f),
                  {.shading = Scene::Material::Shading::Unlit,
                   .colour = {10, 15, 26},
                   .alpha = static_cast<std::uint8_t>(std::lround(31 * in))});
  // The glass, and where a point on its floor shows on the screen
  const Scene::Transform glass =
      glassFrame(f.camera, f.width / f.height, static_cast<float>(Dice::DiceTray::kWidth),
                 static_cast<float>(Dice::DiceTray::kDepth));
  const auto onFloor = [&](double x, double z) {
    return onScreen(f.camera, f.width, f.height,
                    glass.apply({static_cast<float>(x), 0, static_cast<float>(z)}));
  };
  const int players = static_cast<int>(roll.rounds.front().size());
  const Dice::DiceInTurn *dice = show.dice();
  // Once all have landed: the players moving into their order (the
  // user's), the dice and the lanes fading as they go
  // (the dice fade as the first to move goes; the lanes go with them)
  const float sorted = show.sorted(f.now);
  const auto rankOf = [&](int p) { return (p - roll.first + players) % players; };
  // The dice of this round, each in its player's colour, in its own lane
  if (dice && f.pictures.die) {
    const auto toFloat = [](cyclone::real r) { return static_cast<float>(static_cast<double>(r)); };
    const auto smaller = static_cast<float>(dice->dieSize() / Dice::DiceTray::kDieSize);
    for (int p = 0; p < players; ++p) {
      if (!dice->thrown(p))
        continue;
      const cyclone::Vector3 at = dice->position(p);
      const cyclone::Quaternion turn = dice->orientation(p);
      f.renderer.draw(
          f.pictures.die,
          dieOnGlass(glass, {toFloat(at.x), toFloat(at.y), toFloat(at.z)},
                     {toFloat(turn.r), toFloat(turn.i), toFloat(turn.j), toFloat(turn.k)}, 0) *
              Scene::Transform::scaling({smaller, smaller, smaller}),
          {.shading = Scene::Material::Shading::Lit,
           .colour = Palette::player(p),
           .alpha = static_cast<std::uint8_t>(31 * (1 - sorted))});
    }
    // The walls between the lanes: faint lines down the glass
    for (int p = 1; p < players && sorted < 0.5f; ++p) {
      const double x = dice->laneMiddle(p) - dice->laneWidth() / 2;
      const auto top = onFloor(x, -Dice::DiceTray::kDepth / 2),
                 bottom = onFloor(x, Dice::DiceTray::kDepth / 2);
      if (top && bottom)
        overlay.line((*top)[0], std::max((*top)[1], 50 * k), (*bottom)[0], (*bottom)[1],
                     {60, 72, 98}, 1);
    }
  }
  // Its lines down from the top: the title, the names, their numbers, the
  // order (or the rounds before), and a tie's call
  const float numberScale = std::round(1.5f * k);
  const auto [titleLine, namesLine, numbersLine, orderLine, tiedLine] =
      Scene::stackDown(3 * k, std::array{2 * k, k, numberScale, k, k}, k);
  overlay.textCentred(f.width / 2, titleLine.middle() + drop, "WHO GOES FIRST",
                      fadedIn(Palette::kWords, in, {10, 15, 26}), 2 * k);
  // Each player's name over their lane, and under it their number rolling
  // into place as their die lands (an odometer, the user's); the numbers of
  // rounds before (the ties') small under it
  // (a lane's place across the screen)
  const auto laneX = [&](int lane) {
    if (dice)
      if (const auto top = onFloor(dice->laneMiddle(lane), -Dice::DiceTray::kDepth / 2))
        return (*top)[0];
    return f.width / 2 + (static_cast<float>(lane) - static_cast<float>(players - 1) / 2) * 58 * k;
  };
  constexpr std::array<std::string_view, 4> kOrdinals{"1st", "2nd", "3rd", "4th"};
  for (int p = 0; p < players; ++p) {
    // From its lane to its place in the order, whimsically (the user's): a
    // little wind-up back the other way, a hop up over the others, past its
    // place and back (Anticipate, then Settle); each after the one before
    const float t = show.sorted(f.now, rankOf(p));
    const float swung = t < 0.5f ? 0.5f * Motion::along(Motion::Curve::Anticipate, 2 * t)
                                 : 0.5f + 0.5f * Motion::along(Motion::Curve::Settle, 2 * t - 1);
    const float x = laneX(p) + (laneX(rankOf(p)) - laneX(p)) * swung;
    // (staying put, a smaller hop in place: the order is always shown, even
    // when the first lane's player goes first and nobody moves)
    const float hop = (rankOf(p) == p ? -8 : -14) * k * std::sin(kPi * t);
    if (const float in = show.sortedWords(f.now, rankOf(p));
        in > 0 && rankOf(p) < static_cast<int>(kOrdinals.size()))
      overlay.textCentred(
          x, orderLine.middle(), kOrdinals[static_cast<std::size_t>(rankOf(p))],
          fadedIn(rankOf(p) == 0 ? Palette::kHighlight : Palette::kDimWords, in, {10, 15, 26}), k);
    const auto round = static_cast<std::size_t>(show.round());
    const bool winner = show.allLanded() && p == roll.first;
    // (sorted: those who sat out the last round's ties show their last throw)
    if (sorted > 0 && show.shown(round, p) == 0)
      for (std::size_t r = round; r-- > 0;)
        if (const int n = show.shown(r, p)) {
          overlay.textCentred(x, numbersLine.middle() + hop, std::to_string(n), Palette::kDimWords,
                              numberScale);
          break;
        }
    // (once the first is known, the others' numbers dimmed: they decide
    // nothing, so a tie among them is left as it is)
    if (const int n = show.shown(round, p))
      odometer(f, x, numbersLine.middle() + hop, n,
               static_cast<float>(f.now - show.shownAt(round, p)),
               winner             ? Palette::kHighlight
               : show.allLanded() ? Palette::kDimWords
                                  : Palette::kWords);
    std::string before;
    for (std::size_t r = 0; r < round && r < roll.rounds.size(); ++r)
      if (const int n = show.shown(r, p))
        before += (before.empty() ? "" : " ") + std::to_string(n);
    if (!before.empty() && sorted <= 0)
      overlay.textCentred(x, orderLine.middle(), before, Palette::kDimWords, k);
    overlay.textCentred(x, namesLine.middle() + hop + drop, f.names.shortOf(p),
                        fadedIn(Palette::player(p), in, {10, 15, 26}), k);
  }
  if (show.round() > 0 && !show.allLanded())
    overlay.textCentred(f.width / 2, tiedLine.middle(), "tied: they throw again",
                        Palette::kHighlight, k);
  // Once all have landed: only the highest roll counts, the rest taking
  // their turns round the table from the first (the user saw two tie on a 3
  // below the first, nobody throwing again, and took it for a bug), then
  // the order, by name
  if (!show.allLanded())
    return;
  overlay.textCentred(f.width / 2, tiedLine.middle(), "the rest follow round the table",
                      fadedIn(Palette::kSoftWords, sorted, {10, 15, 26}), k);
  std::string homes = "Homes: ", ships = "Ships: ";
  for (int i = 0; i < players; ++i) {
    homes += (i ? ", " : "") + f.names[(roll.first + i) % players];
    ships += (i ? ", " : "") + f.names[(roll.first + players - 1 - i) % players];
  }
  // (up from the screen's bottom edge)
  const auto [firstLine, homesLine, shipsLine] =
      Scene::stackUp(f.height - 3 * k, std::array{1.5f * k, k, k}, k);
  overlay.textCentred(f.width / 2, firstLine.middle(), f.names[roll.first] + " goes first",
                      Palette::player(roll.first), 1.5f * k);
  overlay.textCentred(f.width / 2, homesLine.middle(), homes, Palette::kWords, k);
  // (the longest line: a long name of yours could take it off the screen)
  const std::string shipsWords = ships + " (back again)";
  overlay.textCentred(f.width / 2, shipsLine.middle(), shipsWords, Palette::kSoftWords,
                      scaleToFit(shipsWords, f.width - 4 * k, k, 0.75f * k));
}

// ---------------------------------------------------------------- Roll pay

namespace {
// How far a card's path bows (Motion::arc's bulge, in pixels: negative up
// the screen): up, as a card is thrown; but down over the board when both
// its ends are along the top (the bank behind the ROUND hexagon, the points
// bars: the starting hand dealt, a bot's trade), as up there it would be
// lost under the HUD and off the screen's edge
float cardPathBow(float fromY, float toY, float k) {
  constexpr float kAlongTheTop = 48; // (a DS's pixels: the HUD's top rows)
  const bool alongTheTop = std::max(fromY, toY) < kAlongTheTop * k;
  return alongTheTop ? 56 * k : -28 * k;
}
} // namespace

void drawRollPay(const TopFrame &f, const RollPayShow &show, const State &state, int you) {
  const float k = f.k();
  const TopPictures &p = f.pictures;
  // Each paying planet lit: a tall see-through column round its tile in its
  // resource's colour, in as its first card pops out, out as its last lands
  std::vector<std::pair<int, float>> lit; // (planet, how strongly)
  for (std::size_t i = 0; i < show.cards().size(); ++i)
    if (const auto m = show.at(i, f.now); m && show.cards()[i].planet >= 0) {
      const float strength = m->pop * (1 - m->fly * m->fly);
      const int planet = show.cards()[i].planet;
      const auto it = std::ranges::find(lit, planet, &std::pair<int, float>::first);
      if (it == lit.end())
        lit.emplace_back(planet, strength);
      else
        it->second = std::max(it->second, strength);
    }
  if (p.column)
    for (const auto &[planet, strength] : lit) {
      const Planet &tile = state.planets[static_cast<std::size_t>(planet)];
      f.renderer.draw(p.column, BoardPieces::onHex(tile.hex),
                      {.shading = Scene::Material::Shading::Unlit,
                       .colour = BoardColours::ofKind(tile.kind),
                       .alpha = static_cast<std::uint8_t>(14 * strength)});
    }
  for (std::size_t i = 0; i < show.cards().size(); ++i) {
    const auto m = show.at(i, f.now);
    if (!m)
      continue;
    const RollPayShow::Card &card = show.cards()[i];
    const CardPlaces::Point from = CardPlaces::flightStart(f, state, card, you);
    const CardPlaces::Point to = CardPlaces::at(f, state, card.player, you);
    const float x = Motion::arc(from.x, to.x, 0.0f, m->fly);
    const float y = Motion::arc(from.y, to.y, cardPathBow(from.y, to.y, k), m->fly);
    // Popping out (of a planet, a home), then shrinking as it goes into a
    // bot's row of the points bars, small enough to read as landing in it
    // (yours stays its size, off to your hand; to the bank: shrinking away
    // into the ROUND hexagon)
    const bool yours = card.player == you;
    const float landing = card.player == RollPayShow::kBank ? 1 - m->fly
                          : yours                           ? 1
                                                            : 1 - 0.75f * m->fly;
    const float size = 20 * k * m->pop * landing;
    if (size < 1)
      continue;
    // (a card stolen between two others, or dealt to another: face down, as
    // at a table; its back the whole card, on the backs' own square)
    const bool hidden = card.secret && card.player != you && card.from != you;
    // (each card a step nearer than the last: crossing paths, one stays
    // whole in front of the other, its layers never mixed with another's)
    const float tall = CardFace::heightOf(size);
    const float distance = kPictureDistance - CardFace::kCardStep * static_cast<float>(i % 8);
    if (hidden)
      picture(f, p.backSquare, p.backs[static_cast<std::size_t>(Art::kResourceBack)], x - size / 2,
              y - tall / 2, size, tall, distance);
    else
      cardFace(f, static_cast<Art::Card>(card.resource), x - size / 2, y - tall / 2, size,
               {.title = 0, .distance = distance});
  }
}

// ---------------------------------------------------------------- Picking

namespace {
// The picker's row of what you could build, its names and how to take it
// stacked at the foot (LineStack): the row's middle, and its names' top
// below that (a DS's pixels)
constexpr float kPickNamesDown = 8;
float pickRowY(float height, float k) {
  return height - 3 * k - Scene::stackHeight(std::array{k, k}, 2 * k) - kPickNamesDown * k;
}
} // namespace

CardBox pickCardBox(float width, float height) {
  // (as big as fits between the title and the row)
  const float k = width / 256;
  const float top = 22 * k, bottom = pickRowY(height, k) - 12 * k;
  const float w = std::min(84 * k, CardFace::widthToFit(width, bottom - top));
  const float h = CardFace::heightOf(w);
  return {width / 2 - w / 2, (top + bottom) / 2 - h / 2, w, h};
}

void drawPick(const TopFrame &f, const PickShow &show, std::string_view title,
              const Resources &held, const Settings &settings, Scene::Colour yours) {
  const auto m = show.at(f.now);
  if (!m)
    return;
  const float k = f.k();
  backdrop(f, 0, {8, 12, 22}, 12);
  // The card big in the middle (pickCardBox), the next sliding in from the
  // side moved to and the last out of the other
  const CardBox box = pickCardBox(f.width, f.height);
  const float w = box.width, h = box.height;
  const float midX = f.width / 2, midY = box.y + h / 2, travel = f.width * 0.75f;
  const std::array wordScales{k, k};
  const float rowY = pickRowY(f.height, k);
  const auto dir = static_cast<float>(m->direction);
  if (m->leaving)
    cardFace(f, static_cast<Art::Card>(*m->leaving), midX - dir * travel * m->in - w / 2,
             midY - h / 2, w, {.title = 0});
  const float x = midX + dir * travel * (1 - m->in);
  cardFace(f, static_cast<Art::Card>(m->shown), x - w / 2, midY - h / 2, w, {.title = m->words});
  f.overlay.textCentred(f.width / 2, 12 * k, title, Palette::kWords, k);
  // What you could build with it: a ship, a station, a colony, a card,
  // each dimmed when you couldn't even with this one
  Resources after = held;
  ++after[static_cast<std::size_t>(m->shown)];
  const auto canAfford = [&](const Resources &cost) {
    for (std::size_t r = 0; r < after.size(); ++r)
      if (after[r] < cost[r])
        return false;
    return true;
  };
  constexpr Scene::Colour kDim{70, 78, 96};
  struct Thing {
    std::string_view name;
    const Resources &cost;
    int shape;
  };
  const std::array<Thing, 4> things{{{"Ship", settings.shipCost, 0},
                                     {"Station", settings.stationCost, 1},
                                     {"Colony", settings.colonyCost, 2},
                                     {"Card", settings.contractCost, 3}}};
  // The row of them, their names under them and how to take it under
  // those
  const auto [namesLine, hintLine] = Scene::stackDown(rowY + kPickNamesDown * k, wordScales, 2 * k);
  const float gap = 52 * k;
  for (std::size_t i = 0; i < things.size(); ++i) {
    const Thing &thing = things[i];
    const bool can = canAfford(thing.cost);
    const Scene::Colour c = can ? yours : kDim;
    const float tx = f.width / 2 + (static_cast<float>(i) - 1.5f) * gap, ty = rowY;
    switch (thing.shape) {
    case 0: // (a ship: the board's dart, from above)
      f.overlay.triangle(tx, ty - 6 * k, tx - 5 * k, ty + 5 * k, tx + 5 * k, ty + 5 * k, c);
      break;
    case 1: // (a station)
      f.overlay.rectangle(tx - 4 * k, ty - 4 * k, 8 * k, 8 * k, c);
      break;
    case 2: // (a colony: bigger, a window in it)
      f.overlay.rectangle(tx - 6 * k, ty - 6 * k, 12 * k, 12 * k, c);
      f.overlay.rectangle(tx - 2 * k, ty - 2 * k, 4 * k, 4 * k, {20, 22, 30});
      break;
    default: // (a card)
      f.overlay.rectangle(tx - 4 * k, ty - 6 * k, 8 * k, 12 * k, can ? Palette::kWords : kDim);
      break;
    }
    f.overlay.textCentred(tx, namesLine.middle(), thing.name, can ? Palette::kWords : kDim, k);
  }
  f.overlay.textCentred(f.width / 2, hintLine.middle(), "A takes it", Palette::kDimWords, k);
}

// ---------------------------------------------------------------- Home pay

namespace {
// Where the home die faces you (a DS's pixels, scaled): up the middle of
// the screen, `kDieAcross` across its sides; the card held under it
constexpr float kDieY = 50, kDieAcross = 40, kCardBelowDie = 28;
// Their scale on a screen: a DS's sizes by the width, but no more than by
// the height, so the die and its whole card fit the 3DS's shorter screen
float homePayScale(float width, float height) { return std::min(width / 256, height / 192); }
// The opening onto space left while the die is out: just under where the
// die seats (its face shows down to its settle's sink, and is gone into
// space as it dips: HomePayShow's kSeatSink and kDip)
constexpr float kHoleBelow = HomePayShow::kSeatSink + 0.02f;
static_assert(kHoleBelow < HomePayShow::kDip);

// The face a planet's tile shows at `hex`, as the board draws it
std::optional<Art::Face> faceAt(const State &state, Hex hex) {
  for (const Planet &planet : state.planets)
    if (planet.hex == hex)
      return faceOf(planet);
  return std::nullopt;
}

// The home's tile's place while its die is out: empty space, as though no
// tile had ever been there (homeDieHole: the board's own background colour,
// so the die's lower part is hidden in it as it rises out)
void emptyPlace(const TopFrame &f, Hex hex) {
  if (f.pictures.hexTile)
    f.renderer.draw(f.pictures.hexTile, homeDieHole(BoardPieces::centre(hex), kHoleBelow),
                    {.shading = Scene::Material::Shading::Unlit, .colour = BoardColours::kSpace});
}

// Space rippling at the home's place (spaceRipple: a wormhole opening as
// the die leaves, and closing as it seats), `ripple` of the way through
void homeDieRipple(const TopFrame &f, Hex hex, float ripple) {
  if (!f.pictures.hexRing)
    return;
  for (const RippleRing &ring : spaceRipple(ripple))
    if (ring.material.alpha > 0)
      f.renderer.draw(f.pictures.hexRing,
                      BoardPieces::onHex(hex) * Scene::Transform::rotation({0, 1, 0}, ring.turn) *
                          Scene::Transform::scaling({ring.spread, 1, ring.spread}),
                      ring.material);
}

// The die at `pose`: its body, and on its end the home's own tile (the
// board's tile mesh, with the face texture the board already has loaded)
void homeDieAt(const TopFrame &f, const Scene::Transform &pose, Art::Face face) {
  if (f.pictures.homeDie)
    f.renderer.draw(f.pictures.homeDie, homeDieBody(pose),
                    {.shading = Scene::Material::Shading::Lit});
  if (!f.pictures.hexTile)
    return;
  const Scene::Texture texture = f.pictures.faces[static_cast<std::size_t>(face)];
  f.renderer.draw(
      f.pictures.hexTile, homeDieFace(pose),
      texture ? Scene::Material{.shading = Scene::Material::Shading::Toon, .texture = texture}
              : Scene::Material{.shading = Scene::Material::Shading::Toon,
                                .colour = BoardColours::ofFace(face)});
}

// The die from its place to facing you, as the show has it now (its place
// empty space while it's out, rippling where it leaves and seats)
void homeDie(const TopFrame &f, const HomePayShow::Die &die, Hex hex, Art::Face face) {
  const float dk = homePayScale(f.width, f.height);
  if (die.out) {
    emptyPlace(f, hex);
    // (bowing up the screen: it leaps up and over, an arc)
    const Scene::Transform pose = homeDiePose(
        homeDieInBoard(BoardPieces::centre(hex)),
        homeDieFacing(f.camera, f.width, f.height, f.width / 2, kDieY * dk, kDieAcross * dk),
        viewAxes(f.camera).up * 1.2f, die.lifted, die.sink, die.turn);
    homeDieAt(f, pose, face);
  }
  if (die.ripple)
    homeDieRipple(f, hex, *die.ripple);
}

// What a home's card gives, written under it: its scale (a DS's)
constexpr float kGivesScale = 1.5f;

// The card out of the die, held big with its name, then off to its player
// (flyingCard), what it gives under it
void homePayCard(const TopFrame &f, const HomePayShow::Card &card, const State &state, int player,
                 bool yours) {
  const float k = f.k();
  const CardBox big = homePayCardBox(f.width, f.height);
  const float givesScale = kGivesScale * k, heldX = f.width / 2;
  // (rising out of the die to under it as it grows)
  const float dieY = kDieY * homePayScale(f.width, f.height);
  flyingCard(f, {.resource = card.resource,
                 .width = big.width,
                 .from = {heldX, dieY},
                 .held = {heldX, big.y + big.height / 2},
                 .to = CardPlaces::at(f, state, player, yours ? player : -1),
                 .bow = flightBow(yours, k),
                 .grown = card.grown,
                 .away = card.away,
                 .words = card.words});
  // (what it gives under it, once it's full size, fading in)
  if (card.away <= 0 && card.words > 0) {
    const auto [gives] = Scene::stackDown(big.y + big.height + 4 * k, std::array{givesScale}, 0);
    f.overlay.textCentred(f.width / 2, gives.middle(),
                          "+1 " + std::string(GameWords::of(card.resource)),
                          fadedIn(Palette::kWords, card.words), givesScale);
  }
}
} // namespace

CardBox homePayCardBox(float width, float height) {
  // (as big as fits under the die, with what it gives under it)
  const float k = width / 256, top = (kDieY + kCardBelowDie) * homePayScale(width, height);
  const float givesHeight = Scene::Overlay::lineHeight(kGivesScale * k);
  const float w = std::min(70 * k, CardFace::widthToFit(width, height - top - givesHeight - 8 * k));
  return {width / 2 - w / 2, top, w, CardFace::heightOf(w)};
}

void drawHomePay(const TopFrame &f, const HomePayShow &show, const State &state, bool yours) {
  const HomePayShow::Pay *pay = show.current();
  const auto die = show.die(f.now);
  if (!pay || !die)
    return;
  const float k = f.k(), dk = homePayScale(f.width, f.height);
  // A dim backdrop once it faces you, behind it (it rises through its
  // plane on the way up and back, so only near the end)
  const float facing = std::clamp((die->lifted - 0.85f) / 0.15f, 0.0f, 1.0f);
  if (facing > 0) {
    const Scene::Vector3 at =
        homeDieFacing(f.camera, f.width, f.height, f.width / 2, kDieY * dk, kDieAcross * dk)
            .position();
    constexpr float kBehindIt = 1.6f; // (clear of its corners as it spins)
    backdrop(f, 0, {8, 12, 22}, 12 * facing, (at - f.camera.eye).length() + kBehindIt);
  }
  const std::optional<Hex> home = CardPlaces::homeOf(state, pay->player);
  const std::optional<Art::Face> face = home ? faceAt(state, *home) : std::nullopt;
  if (home && face)
    homeDie(f, *die, *home, *face);
  if (facing > 0.5f) {
    const std::string whose = yours ? "Your home" : f.names[pay->player] + "'s home";
    f.overlay.textCentred(f.width / 2, 8 * k, whose + " pays", BoardColours::ofPlayer(pay->player),
                          k);
  }
  if (show.waitingForChoice(f.now))
    f.overlay.textCentred(f.width / 2, (kDieY + kCardBelowDie + 8) * dk, choiceWords(yours),
                          Palette::kHighlight, 1.5f * k);
  if (const auto card = show.card(f.now))
    homePayCard(f, *card, state, pay->player, yours);
}

// ----------------------------------------------------------------- Salvage

float salvageCardWidth(float width, float height, Scene::LineHeight lineHeight) {
  // (a DS's 64 at most, and 2:3 between the find's name and what it gives,
  // clear of both)
  const DiscoveryLayout layout = discoveryLayout(width, height, lineHeight);
  const float k = width / 256;
  return std::min(kSalvageCardWidth * k,
                  CardFace::widthToFit(width, layout.gives.top - layout.title.bottom - 4 * k));
}

std::vector<ScreenPoint> salvageCardMiddles(float width, float height, int cards,
                                            Scene::LineHeight lineHeight) {
  const DiscoveryLayout layout = discoveryLayout(width, height, lineHeight);
  const float k = width / 256;
  const float w = salvageCardWidth(width, height, lineHeight), gap = 12 * k;
  const float y = (layout.title.bottom + layout.gives.top) / 2;
  const float left =
      width / 2 -
      (static_cast<float>(cards) * w + static_cast<float>(std::max(0, cards - 1)) * gap) / 2;
  std::vector<ScreenPoint> middles;
  middles.reserve(static_cast<std::size_t>(std::max(0, cards)));
  for (int i = 0; i < cards; ++i)
    middles.push_back({left + w / 2 + static_cast<float>(i) * (w + gap), y});
  return middles;
}

void drawSalvage(const TopFrame &f, const SalvageShow &show, const State &state, int you) {
  if (!show.on())
    return;
  const float k = f.k();
  const std::vector<ScreenPoint> middles = salvageCardMiddles(f.width, f.height, show.howMany());
  const bool yours = you >= 0 && show.player() == you;
  if (show.waitingForChoice(f.now))
    f.overlay.textCentred(f.width / 2, middles.front().y, choiceWords(yours), Palette::kHighlight,
                          1.5f * k);
  const float w = salvageCardWidth(f.width, f.height), h = CardFace::heightOf(w);
  const CardPlaces::Point to = CardPlaces::at(f, state, show.player(), you);
  for (const SalvageShow::Card &card : show.cards(f.now)) {
    const ScreenPoint held = middles[static_cast<std::size_t>(card.slot) % middles.size()];
    // (up from below the screen, where it was picked)
    flyingCard(f, {.resource = card.resource,
                   .width = w,
                   .from = {held.x, f.height + h},
                   .held = held,
                   .to = to,
                   .bow = flightBow(yours, k),
                   .grown = card.in,
                   .away = card.away,
                   .words = card.words});
  }
}
} // namespace CosmicHex::Handheld
