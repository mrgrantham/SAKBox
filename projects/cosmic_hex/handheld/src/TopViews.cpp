#include <CosmicHex/Handheld/TopViews.hpp>

#include <CosmicHex/Dice/DiceTray.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/CardsHeld.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/GameWords.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>

#include <ScratchPadd/Motion/Motion.hpp>
#include <ScratchPadd/Scene/TextWrap.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <tuple>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;
namespace Motion = ScratchPadd::Motion;

namespace {
constexpr float kPi = 3.14159265f;

// A flat picture covering screen pixels (x, y) to (x + w, y + h)
void picture(const TopFrame &f, Scene::Mesh square, Scene::Texture texture, float x, float y,
             float w, float h, float distance = kPictureDistance) {
  f.renderer.draw(square, onScreenAt(f.camera, f.width, f.height, x, y, w, h, distance),
                  {.shading = Scene::Material::Shading::Unlit, .texture = texture});
}
// A dark, see-through backdrop over the whole screen (in 3D, behind the
// pictures: the overlay's shapes would cover them), `top` down
void backdrop(const TopFrame &f, float top, Scene::Colour colour, float alpha) {
  f.renderer.draw(
      f.pictures.pictureSquare,
      onScreenAt(f.camera, f.width, f.height, 0, top, f.width, f.height, kPictureDistance * 1.2f),
      {.shading = Scene::Material::Shading::Unlit,
       .colour = colour,
       .alpha = static_cast<std::uint8_t>(alpha)});
}
} // namespace

// ---------------------------------------------------------------- Discovery

namespace {
// A number token's number over its chip, on a tile at `pose` facing you
void tokenNumber(const TopFrame &f, const Scene::Transform &pose, int number) {
  const float digitsDown = kChipDown - 0.24f * kChipRadius; // (the digits' middle)
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
  f.overlay.textCentred(x, y, words, kInk, scale);
}
} // namespace

void drawDiscovery(const TopFrame &f, const DiscoveryShow &show,
                   const ScratchPadd::PictureSlot &closeUp) {
  const auto moment = show.moment(f.now);
  const Discovery::Found *found = show.current();
  if (!moment || !found)
    return;
  const float k = f.k();
  const float lifted = moment->lifted();
  // A dim backdrop while it faces you: in as it turns over, gone early on
  // its way back (the tile passes through its plane on the way up and down)
  const float dim = moment->turn * std::max(0.0f, 1 - 4 * moment->down);
  if (dim > 0)
    backdrop(f, 0, {8, 12, 22}, 24 * dim);
  // On the board: where it lies, flat (its axes the world's)
  const Scene::Vector3 onBoard = BoardPieces::centre(found->hex);
  const float radius = (BoardPieces::at({found->hex, 0}) - onBoard).length();
  // Facing you: in front of the camera, about 55% of the screen tall, its
  // face towards you, its top up the screen
  const Scene::Camera &camera = f.camera;
  const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
  const Scene::Vector3 right = forward.cross(camera.up).normalised();
  const Scene::Vector3 screenUp = right.cross(forward);
  const float distance = 2 * radius / (0.55f * 2 * std::tan(camera.fieldOfView * kPi / 360));
  const Scene::Vector3 facing = camera.eye + forward * distance;
  // Between: its position and axes carried from one to the other (the axes
  // made square again: Gram-Schmidt)
  // Between: its axes carried from one to the other (made square again:
  // Gram-Schmidt; flat while it dips or settles: it stays rigid), its
  // place along an arc bowing up the screen (not a straight line)
  const float turned = std::clamp(lifted, 0.0f, 1.0f);
  const auto between = [turned](const Scene::Vector3 &a, const Scene::Vector3 &b) {
    return a + (b - a) * turned;
  };
  const Scene::Vector3 y = between({0, 1, 0}, forward * -1).normalised();
  Scene::Vector3 z = between({0, 0, 1}, screenUp * -1);
  z = (z - y * z.dot(y)).normalised();
  const Scene::Vector3 x = y.cross(z);
  const Scene::Vector3 place = Motion::arc(onBoard, facing, screenUp * (0.8f * radius), lifted);
  const Scene::Transform pose = Scene::Transform::fromAxes(x, y, z, place);
  // Turning over about its x axis, as on the board: face down till halfway
  const float turn = moment->turn * kPi;
  const bool faceUp = turn > kPi / 2;
  const auto spin = Scene::Transform::rotation({1, 0, 0}, faceUp ? turn - kPi : turn);
  const Art::Face face = faceUp ? found->face : found->faceDown;
  const Scene::Texture texture = closeUp.showing(static_cast<int>(face))
                                     ? closeUp.texture()
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
  // Its words, while it faces you
  if (lifted > 0.98f) {
    f.overlay.textCentred(f.width / 2, 10 * k, f.names[found->by] + " discovers",
                          BoardColours::ofPlayer(found->by), k);
    if (moment->turn >= 1) {
      f.overlay.textCentred(f.width / 2, f.height / 2 - 66 * k, found->name, Palette::kWords,
                            2 * k);
      f.overlay.textCentred(f.width / 2, f.height / 2 + 64 * k, found->gives, Palette::kSoftWords,
                            1.5f * k);
    }
    // The token's number written sharp over its picture once it's landed
    // (the user's A: a small texture's digits blur; the words are crisp
    // on every screen), on the chip's own cream, where its digits are (the
    // upper part: 20 to 56 of its picture's 100)
    if (found->number > 0 && moment->chip >= 1)
      tokenNumber(f, pose, found->number);
  }
}

// ------------------------------------------------------------------- Dealt

namespace {
// An action card's face, `w` wide with its top left at (x, y), turned over
// `turned` of the way (its width through |cos|: past halfway, its face):
// its picture (sharp when `closeUp` shows it), its body, and once flat its
// title and words
void actionCardFace(const TopFrame &f, Art::Card card, float x, float y, float w, float turned,
                    const ScratchPadd::PictureSlot *closeUp) {
  const float k = f.k();
  const float pictureH = w * 52 / 63, cardH = w * 88 / 63;
  const float across = std::abs(std::cos(turned * kPi));
  const float cw = w * across, cx = x + w * (1 - across) / 2;
  const bool sharp = closeUp && closeUp->showing(static_cast<int>(card));
  picture(f, sharp ? f.pictures.closeUpCardSquare : f.pictures.pictureSquare,
          sharp ? closeUp->texture() : f.pictures.cards[static_cast<std::size_t>(card)], cx, y, cw,
          pictureH);
  // The card's body under its picture, and once flat, its words
  f.overlay.rectangle(cx, y + pictureH, cw, cardH - pictureH, {236, 232, 220, 255});
  if (turned < 1)
    return;
  const Art::CardWords words = Art::wordsOf(card);
  // (the title white, centred on the label band, 3 to 19 of the card's
  // 88 units, as large as fits its width: the user's)
  float titleScale = 2 * k;
  while (titleScale > k && Scene::Overlay::textWidth(words.shortTitle, titleScale) > w * 54 / 63)
    titleScale -= 0.25f * k;
  f.overlay.textCentred(x + w / 2, y + w * 11 / 63, words.shortTitle, Palette::kWords, titleScale);
  // (wrapped by the width the letters take as drawn, spacing included)
  const auto lines = Scene::wrapText(words.words, w - 8 * k, [k](std::string_view line) {
    return Scene::Overlay::textWidth(line, k);
  });
  for (std::size_t l = 0; l < lines.size() && l < 6; ++l)
    f.overlay.text(x + 4 * k, y + pictureH + (3 + 9 * static_cast<float>(l)) * k, lines[l],
                   {40, 40, 48}, k);
}
} // namespace

void drawDealtCards(const TopFrame &f, const DealtShow &show, int chosen,
                    const std::array<ScratchPadd::PictureSlot, 2> &closeUps) {
  const std::vector<Art::Card> &cards = show.cards();
  if (!show.on() || cards.empty())
    return;
  const float k = f.k();
  const float w = 100 * k, cardH = w * 88 / 63, gap = 16 * k;
  const auto count = static_cast<float>(cards.size());
  const float left = (f.width - (count * w + (count - 1) * gap)) / 2, top = 8 * k;
  f.overlay.textCentred(f.width / 2, f.height - 6 * k, "Keep one: touch it below",
                        Palette::kDimWords, k);
  for (std::size_t i = 0; i < cards.size(); ++i) {
    const float homeX = left + static_cast<float>(i) * (w + gap);
    // Thrown in from the bottom left along an arc (bowing up), landing
    // with a settle (DealtShow::thrown)
    const float flown = show.thrown(i, f.now);
    const float x = Motion::arc(-w, homeX, -6 * k, flown);
    const float y = Motion::arc(f.height, top, -30 * k, flown);
    // Then turned over: its width through |cos|, its back until halfway
    const float turned = show.turned(i, f.now);
    const float across = std::abs(std::cos(turned * kPi));
    const float cw = w * across, cx = x + w * (1 - across) / 2;
    if (turned < 0.5f) {
      picture(f, f.pictures.backSquare,
              f.pictures.backs[static_cast<std::size_t>(Art::kActionBack)], cx, y, cw, cardH);
      continue;
    }
    const Art::Card card = cards[i];
    const ScratchPadd::PictureSlot *closeUp = i < closeUps.size() ? &closeUps[i] : nullptr;
    actionCardFace(f, card, x, y, w, turned, closeUp);
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

void drawCardPlay(const TopFrame &f, const CardPlayShow &show, const State &state, int you) {
  const auto m = show.at(f.now);
  const CardPlayShow::Play *play = show.current();
  if (!m || !play)
    return;
  const float k = f.k();
  backdrop(f, 0, {8, 12, 22}, 12 * std::min(1.0f, m->up * 2) * (1 - m->away));
  // From its player (your hand below, or their row of the points bars) up
  // to the middle, face down until it turns; then off up and away
  const float w = 90 * k, cardH = w * 88 / 63;
  const float fromX = play->player == you ? f.width * 0.7f : f.width - 20 * k;
  const float fromY = play->player == you
                          ? f.height + cardH
                          : PointsBars::kTop + static_cast<float>(play->player) *
                                                   (PointsBars::kRowHeight + PointsBars::kRowGap);
  const float midX = f.width / 2, midY = f.height / 2 + 6 * k;
  const float cx =
      Motion::arc(Motion::arc(fromX, midX, -10 * k, m->up), f.width + w, 0.0f, m->away);
  const float cy = Motion::arc(Motion::arc(fromY, midY, -20 * k, m->up), -cardH, -20 * k, m->away);
  const float size = std::max(0.2f, std::min(1.0f, m->up)) * (1 - 0.5f * m->away);
  const float cw = w * size;
  const float x = cx - cw / 2, y = cy - cw * 88 / 63 / 2;
  if (m->turned < 0.5f) {
    const float across = std::abs(std::cos(m->turned * kPi));
    picture(f, f.pictures.backSquare, f.pictures.backs[static_cast<std::size_t>(Art::kActionBack)],
            x + cw * (1 - across) / 2, y, cw * across, cw * 88 / 63);
  } else {
    actionCardFace(f, play->card, x, y, cw, m->away > 0 ? 0.999f : m->turned, nullptr);
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

// --------------------------------------------------------- Show resources

namespace {
// The five piles of resource cards along the bottom (Show resources' and
// cards gained's): `heldOf` each; each card's height by `showSlid` (0 below
// the screen, 1 in place: by its order across the piles), and a pile's
// newest `fresh` cards, at its back, by `dropped` (0 above the screen, 1
// in place). The order counted on past the piles, for what's drawn next
int drawPiles(const TopFrame &f, const Resources &heldOf, const std::function<float(int)> &showSlid,
              const Resources &fresh, const std::function<float(Resource, int)> &dropped) {
  const float k = f.k();
  const TopPictures &p = f.pictures;
  const auto rw = static_cast<float>(p.resourceWidth), rh = rw * 52 / 63;
  constexpr float kPileStep = 5; // (pixels between a pile's cards: their bands show)
  // Five piles across with a little room between, their bottoms just above
  // the screen's edge
  const float pileGap = 6 * k;
  const float pilesLeft = (f.width - (5 * rw + 4 * pileGap)) / 2;
  const float pileY = f.height - rh - 6 * k;
  const float u = rw / Art::kCardWidth;
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
      f.renderer.draw(p.resourceSquare,
                      onScreenAt(f.camera, f.width, f.height, x, slid, rw, rh,
                                 kPictureDistance - 0.02f * static_cast<float>(shown - j)),
                      {.shading = Scene::Material::Shading::Unlit,
                       .texture = p.resources[static_cast<std::size_t>(r)],
                       .alpha = static_cast<std::uint8_t>(held == 0 ? 10 : 31)});
    }
    // The title in the front card's band, dark on a light band, faint when
    // there are none; past five, how many in a badge above the pile
    const float front = pileY + (1 - showSlid(order - 1)) * (f.height - pileY + 4);
    const Scene::Colour band = p.resourceBands[static_cast<std::size_t>(r)];
    const float light = 0.2126f * band.r + 0.7152f * band.g + 0.0722f * band.b;
    const Scene::Colour ink = held == 0     ? Scene::Colour{120, 128, 145}
                              : light > 140 ? Scene::Colour{16, 20, 30}
                                            : Scene::Colour{242, 245, 251};
    f.overlay.textCentred(x + rw / 2, front + (Art::kCardTitle.y + Art::kCardTitle.height / 2) * u,
                          GameWords::of(static_cast<Resource>(r)), ink);
    if (held > 5) {
      const float top = pileY - static_cast<float>(shown - 1) * kPileStep * k;
      const float bw = 14 * k, bh = 10 * k, by = top - 13 * k;
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
  const auto w = static_cast<float>(p.cardWidth), h = w * 52 / 63;
  const float gap = 8 * k, total = static_cast<float>(actions.size()) * (w + gap) - gap;
  for (std::size_t i = 0; i < actions.size(); ++i) {
    const float x = (f.width - total) / 2 + static_cast<float>(i) * (w + gap), y = 44 * k;
    const float in = show.arrived(order++, f.now);
    picture(f, p.pictureSquare, p.cards[static_cast<std::size_t>(actions[i])], x,
            y - (1 - in) * (y + h + 4), w, h);
    f.overlay.textCentred(x + w / 2, y + h + 6 * k, Art::wordsOf(actions[i]).shortTitle,
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

void drawOtherPlayer(const TopFrame &f, const PlayerView &view, float down) {
  const float k = f.k();
  Scene::Overlay &overlay = f.overlay;
  const float top = -f.height * (1 - down);
  const Scene::Colour colour = BoardColours::ofPlayer(view.player);
  backdrop(f, top, {12, 18, 32}, 29);
  overlay.rectangle(3 * k, top + 3 * k, f.width - 6 * k, 1, colour);
  overlay.rectangle(3 * k, top + f.height - 4 * k, f.width - 6 * k, 1, colour);
  // Name and points
  overlay.text(10 * k, top + 10 * k, f.names.shortOf(view.player), colour, 1.5f * k);
  overlay.text(10 * k, top + 32 * k,
               std::to_string(view.points) + (view.points == 1 ? " point" : " points"),
               Palette::kWords, k);
  // Their cards, face down, in the agreed backs: fanned, six a row, a
  // second row under the first when there are more (up to 12 shown), the
  // count by the label. What's under them moves down to make room
  constexpr int kBacksARow = 6, kBacksShown = 12;
  // (36 wide, the user's: bigger; fanned closer, 8 apart, so six still fit
  // in each column: 36 + 5 x 8 = 76, the columns at 84 and 170)
  const float backW = 36 * k, backH = backW * 88 / 63, backStep = 8 * k, rowStep = 18 * k;
  const auto rowsOf = [&](int count) {
    return std::max(1, (std::min(count, kBacksShown) + kBacksARow - 1) / kBacksARow);
  };
  const int rows = std::max(rowsOf(view.resourceCards), rowsOf(view.actionCards));
  const float extra = backH - 34 * k + rowStep * static_cast<float>(rows - 1);
  const auto backs = [&](float x, float y, int count, std::string_view label,
                         Art::CardBack design) {
    overlay.text(x, y, std::string(label) + " " + std::to_string(count), Palette::kDimWords, k);
    const Scene::Texture back = f.pictures.backs[static_cast<std::size_t>(design)];
    if (!f.pictures.backSquare || !back)
      return;
    for (int i = 0; i < std::min(count, kBacksShown); ++i)
      picture(f, f.pictures.backSquare, back, x + static_cast<float>(i % kBacksARow) * backStep,
              y + 11 * k + static_cast<float>(static_cast<int>(i / kBacksARow)) * rowStep, backW,
              backH, kPictureDistance * (1 - 0.002f * static_cast<float>(i)));
  };
  backs(84 * k, top + 10 * k, view.resourceCards, "RESOURCES", Art::kResourceBack);
  backs(170 * k, top + 10 * k, view.actionCards, "CARDS", Art::kActionBack);
  // Badges, ships, stations and colonies as the board draws them (the
  // user's): each a group of its pieces, up to four, or one and how many;
  // its name small under it
  const auto pieces = [&](float x, int count, std::string_view name, auto &&drawOne) {
    const float y = top + 66 * k + extra;
    constexpr int kMostDrawn = 4;
    for (int i = 0; i < (count > kMostDrawn ? 1 : count); ++i)
      drawOne(x + 5 * k + static_cast<float>(i) * 11 * k, y);
    if (count > kMostDrawn)
      overlay.text(x + 12 * k, y - 4 * k, "x" + std::to_string(count), Palette::kWords, k);
    if (count == 0)
      overlay.text(x + 1 * k, y - 4 * k, "none", Palette::kDimWords, k);
    overlay.text(x, y + 8 * k, name, Palette::kDimWords, k);
  };
  pieces(10 * k, view.badges, "badges",
         [&](float x, float y) { overlay.hexagon(x, y, 4.5f * k, Palette::kHighlight, true); });
  pieces(72 * k, view.ships, "ships", [&](float x, float y) {
    overlay.triangle(x, y - 5 * k, x - 4.5f * k, y + 4 * k, x + 4.5f * k, y + 4 * k, colour);
  });
  pieces(134 * k, view.stations, "stations", [&](float x, float y) {
    overlay.rectangle(x - 3.5f * k, y - 3.5f * k, 7 * k, 7 * k, colour);
  });
  pieces(196 * k, view.colonies, "colonies", [&](float x, float y) {
    overlay.rectangle(x - 4.5f * k, y - 4.5f * k, 9 * k, 9 * k, colour);
    overlay.rectangle(x - 1.5f * k, y - 1.5f * k, 3 * k, 3 * k, {20, 22, 30});
  });
  // Their planets: a hexagon each in its kind's colour, its number on it
  overlay.text(10 * k, top + 88 * k + extra, "PLANETS", Palette::kDimWords, k);
  if (view.planets.empty())
    overlay.text(64 * k, top + 88 * k + extra, "none yet", Palette::kDimWords, k);
  for (std::size_t i = 0; i < view.planets.size() && i < 16; ++i) {
    const float x = (22 + 26 * static_cast<float>(i % 8)) * k;
    const float y = top + (110 + 25 * static_cast<float>(static_cast<int>(i / 8))) * k + extra;
    const PlayerView::Planet &planet = view.planets[i];
    overlay.hexagon(x, y, 12 * k, BoardColours::ofKind(planet.kind), true);
    if (planet.colony)
      overlay.hexagon(x, y, 8 * k, colour, true);
    overlay.textCentred(x, y, std::to_string(planet.number), {20, 22, 30}, k);
  }
  // Their revealed Charters
  if (!view.charters.empty()) {
    std::string charters = "Charters:";
    for (const Art::Card card : view.charters)
      charters += " " + std::string(Art::wordsOf(card).shortTitle);
    overlay.text(10 * k, std::min(top + 156 * k + extra, top + f.height - 14 * k), charters,
                 Palette::kWords, k);
  }
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
  // A dark backdrop behind the glass (in 3D: the overlay's shapes would
  // cover the dice), in front of the board
  f.renderer.draw(f.pictures.pictureSquare,
                  onScreenAt(f.camera, f.width, f.height, 0, 0, f.width, f.height, 7.5f),
                  {.shading = Scene::Material::Shading::Unlit, .colour = {10, 15, 26}});
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
          {.shading = Scene::Material::Shading::Lit, .colour = Palette::player(p)});
    }
    // The walls between the lanes: faint lines down the glass
    for (int p = 1; p < players; ++p) {
      const double x = dice->laneMiddle(p) - dice->laneWidth() / 2;
      const auto top = onFloor(x, -Dice::DiceTray::kDepth / 2),
                 bottom = onFloor(x, Dice::DiceTray::kDepth / 2);
      if (top && bottom)
        overlay.line((*top)[0], std::max((*top)[1], 50 * k), (*bottom)[0], (*bottom)[1],
                     {60, 72, 98}, 1);
    }
  }
  overlay.textCentred(f.width / 2, 12 * k, "WHO GOES FIRST", Palette::kWords, 2 * k);
  // Each player's name over their lane, and under it their number rolling
  // into place as their die lands (an odometer, the user's); the numbers of
  // rounds before (the ties') small under it
  for (int p = 0; p < players; ++p) {
    float x = f.width / 2 + (static_cast<float>(p) - static_cast<float>(players - 1) / 2) * 58 * k;
    if (dice)
      if (const auto top = onFloor(dice->laneMiddle(p), -Dice::DiceTray::kDepth / 2))
        x = (*top)[0];
    const auto round = static_cast<std::size_t>(show.round());
    const bool winner = show.allLanded() && p == roll.first;
    if (const int n = show.shown(round, p))
      odometer(f, x, 44 * k, n, static_cast<float>(f.now - show.shownAt(round, p)),
               winner ? Palette::kHighlight : Palette::kWords);
    std::string before;
    for (std::size_t r = 0; r < round && r < roll.rounds.size(); ++r)
      if (const int n = show.shown(r, p))
        before += (before.empty() ? "" : " ") + std::to_string(n);
    if (!before.empty())
      overlay.textCentred(x, 58 * k, before, Palette::kDimWords, k);
    overlay.textCentred(x, 28 * k, f.names.shortOf(p), Palette::player(p), k);
  }
  if (show.round() > 0 && !show.allLanded())
    overlay.textCentred(f.width / 2, 70 * k, "tied: they throw again", Palette::kHighlight, k);
  // Once all have landed: the order, by name
  if (!show.allLanded())
    return;
  std::string homes = "Homes: ", ships = "Ships: ";
  for (int i = 0; i < players; ++i) {
    homes += (i ? ", " : "") + f.names[(roll.first + i) % players];
    ships += (i ? ", " : "") + f.names[(roll.first + players - 1 - i) % players];
  }
  const float bottom = f.height - 40 * k;
  overlay.textCentred(f.width / 2, bottom, f.names[roll.first] + " goes first",
                      Palette::player(roll.first), 1.5f * k);
  overlay.textCentred(f.width / 2, bottom + 15 * k, homes, Palette::kWords, k);
  overlay.textCentred(f.width / 2, bottom + 27 * k, ships + " (back again)", Palette::kSoftWords,
                      k);
}

// ---------------------------------------------------------------- Roll pay

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
    // A player's place: your hand below, or their row of the points bars
    const float barsRight = f.width / 2 + (static_cast<float>(state.settings.winPoints) *
                                           (PointsBars::kDashWidth + PointsBars::kDashGap)) /
                                              2;
    const auto placeOf = [&](int player) {
      if (player == RollPayShow::kBank) // (the bank: off the top of the screen's middle)
        return std::pair{f.width / 2, -16 * k};
      return player == you
                 ? std::pair{f.width * 0.7f, f.height + 24 * k}
                 : std::pair{barsRight + 6 * k,
                             PointsBars::kTop + static_cast<float>(player) *
                                                    (PointsBars::kRowHeight + PointsBars::kRowGap)};
    };
    // Where it comes from: its planet, or the player (or bank) it's from
    float fromX = f.width / 2, fromY = f.height / 2;
    if (card.planet >= 0)
      f.renderer.toScreen(
          BoardPieces::centre(state.planets[static_cast<std::size_t>(card.planet)].hex), fromX,
          fromY);
    else
      std::tie(fromX, fromY) = placeOf(card.from);
    const bool yours = card.player == you;
    const auto [toX, toY] = placeOf(card.player);
    const float x = Motion::arc(fromX, toX, 0.0f, m->fly);
    const float y = Motion::arc(fromY, toY, -28 * k, m->fly);
    // Popping out of the planet (a settle), then shrinking as it goes to a
    // bot's bar (yours stays its size, off to your hand)
    const float size = 20 * k * m->pop * (yours ? 1 : 1 - 0.6f * m->fly);
    if (size < 1)
      continue;
    // (a card stolen between two others: face down, as at a table)
    const bool hidden = card.secret && card.player != you && card.from != you;
    picture(f, p.resourceSquare,
            hidden ? p.backs[static_cast<std::size_t>(Art::kResourceBack)]
                   : p.resources[static_cast<std::size_t>(card.resource)],
            x - size / 2, y - size * 52 / 63 / 2, size, size * 52 / 63);
  }
}

// ---------------------------------------------------------------- Picking

void drawPick(const TopFrame &f, const PickShow &show, std::string_view title,
              const Resources &held, const Settings &settings, Scene::Colour yours) {
  const auto m = show.at(f.now);
  if (!m)
    return;
  const float k = f.k();
  backdrop(f, 0, {8, 12, 22}, 12);
  // The card big in the middle, the next sliding in from the side moved to
  // and the last out of the other
  const float w = 84 * k, h = w * 52 / 63;
  const float midX = f.width / 2, midY = f.height * 0.42f, travel = f.width * 0.75f;
  const auto card = [&](Resource r, float x) {
    picture(f, f.pictures.resourceSquare, f.pictures.resources[static_cast<std::size_t>(r)],
            x - w / 2, midY - h / 2, w, h);
  };
  const auto dir = static_cast<float>(m->direction);
  if (m->leaving)
    card(*m->leaving, midX - dir * travel * m->in);
  const float x = midX + dir * travel * (1 - m->in);
  card(m->shown, x);
  if (m->in >= 1) {
    f.overlay.textCentred(x, midY - h / 2 + h * 11 / 52, GameWords::of(m->shown), Palette::kWords,
                          1.5f * k);
  }
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
  const float rowY = midY + h / 2 + 26 * k, gap = 52 * k;
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
    f.overlay.textCentred(tx, ty + 14 * k, thing.name, can ? Palette::kWords : kDim, k);
  }
  f.overlay.textCentred(f.width / 2, rowY + 30 * k, "A takes it", Palette::kDimWords, k);
}

// ---------------------------------------------------------------- Home pay

namespace {} // namespace

void drawHomePay(const TopFrame &f, const HomePayShow &show, const State &state, bool yours,
                 const ScratchPadd::PictureSlot &closeUp) {
  const auto seconds = show.seconds(f.now);
  const HomePayShow::Pay *pay = show.current();
  if (!seconds || !pay)
    return;
  const float k = f.k();
  const HomePayShow::Moment m = show.at(*seconds);
  Scene::Overlay &overlay = f.overlay;
  // Where the die starts: the home on the board, on the screen
  float homeX = f.width / 2, homeY = f.height * 0.7f;
  for (const Planet &planet : state.planets)
    if (planet.homeOf == pay->player || planet.formerHomeOf == pay->player) {
      float x = 0, y = 0;
      if (f.renderer.toScreen(BoardPieces::centre(planet.hex), x, y)) {
        homeX = x;
        homeY = y;
      }
    }
  backdrop(f, 0, {8, 12, 22}, 12 * std::min(1.0f, m.up * 2) * (1 - m.away));
  // The die: out of the home along an arc, winding up first (Anticipate:
  // it dips into the home before it leaps), growing as it comes
  const float topX = f.width / 2, topY = 46 * k;
  const float rise = Motion::along(Motion::Curve::Anticipate, m.up);
  const float dieX = Motion::arc(homeX, topX, -10 * k, rise);
  const float dieY = Motion::arc(homeY, topY, -26 * k, rise);
  // The die (the user's: an extruded white hexagon, a resource's colour on
  // each side, the star on the sixth) rolling about its long axis as it
  // flies, slowing, landing with the side chosen facing you, a small
  // wobble as it settles (Settle: a small thing may squash a little)
  if (m.away < 1 && f.pictures.homeDie) {
    constexpr float kTurn = 6.2831853f;
    const float flown = (m.up * HomePayShow::kSecondsUp + m.tumble * HomePayShow::kSecondsTumble) /
                        (HomePayShow::kSecondsUp + HomePayShow::kSecondsTumble);
    const float slowing = 1 - (1 - flown) * (1 - flown) * (1 - flown); // (fast, then slower)
    const float wobble = std::sin(m.settle * kTurn) * (1 - m.settle) * 0.35f;
    const float roll = homeDieTurn(pay->face) - (1 - slowing) * 3 * kTurn + wobble;
    const float landed = Motion::along(Motion::Curve::Settle, m.settle);
    const float size =
        (12 + 18 * std::min(1.0f, m.up)) * k * (m.settle > 0 ? 0.9f + 0.1f * landed : 1);
    // (turned a little about the vertical, so an end shows: a prism, not a
    // flat hexagon; less once it's landed)
    const float tilt = 0.25f + 0.35f * (1 - slowing);
    f.renderer.draw(f.pictures.homeDie,
                    solidOnScreen(f.camera, f.width, f.height, dieX, dieY, size) *
                        Scene::Transform::rotation({0, 1, 0}, tilt) *
                        Scene::Transform::rotation({1, 0, 0}, roll),
                    {.shading = Scene::Material::Shading::Lit});
  }
  const std::string whose = yours ? "Your home" : f.names[pay->player] + "'s home";
  if (m.up > 0 && m.away <= 0)
    overlay.textCentred(f.width / 2, 8 * k, whose + " pays", BoardColours::ofPlayer(pay->player),
                        k);
  if (m.waitingForChoice) {
    overlay.textCentred(f.width / 2, topY + 28 * k, yours ? "Your choice: below" : "Choosing...",
                        Palette::kHighlight, 1.5f * k);
    return;
  }
  if (m.card <= 0 || !pay->resource)
    return;
  // The card: out of the die with a pop (Settle), held big with its name,
  // then winding up and off along an arc to the hand, shrinking
  const auto r = static_cast<std::size_t>(*pay->resource);
  const float grown = Motion::along(Motion::Curve::Settle, m.card);
  const float leave = Motion::along(Motion::Curve::Anticipate, m.away);
  const float bigW = 70 * k, bigH = bigW * 52 / 63;
  const float w = bigW * grown * (1 - 0.8f * std::max(0.0f, leave));
  const float h = w * 52 / 63;
  const float heldX = f.width / 2, heldY = topY + 22 * k + bigH / 2;
  const float toX = yours ? f.width - 30 * k : f.width / 2;
  const float toY = yours ? f.height + 40 * k : -40 * k;
  const float cx = Motion::arc(heldX, toX, yours ? 18 * k : -24 * k, leave);
  const float cy = Motion::arc(heldY, toY, yours ? -30 * k : 0, leave);
  const bool sharp = closeUp.showing(static_cast<int>(*pay->resource));
  if (w > 1)
    picture(f, sharp ? f.pictures.closeUpCardSquare : f.pictures.resourceSquare,
            sharp ? closeUp.texture() : f.pictures.resources[r], cx - w / 2, cy - h / 2, w, h);
  // (its name on its label band, 3 to 19 of the card's 52-unit picture,
  // once it's full size)
  if (m.away <= 0 && m.card >= 1)
    overlay.textCentred(cx, cy - h / 2 + h * 11 / 52, GameWords::of(*pay->resource),
                        Palette::kWords, 1.5f * k);
  if (m.away <= 0 && m.card >= 1)
    overlay.textCentred(f.width / 2, heldY + bigH / 2 + 10 * k,
                        "+1 " + std::string(GameWords::of(*pay->resource)), Palette::kWords,
                        1.5f * k);
}

} // namespace CosmicHex::Handheld
