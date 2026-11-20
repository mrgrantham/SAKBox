#include <CosmicHex/Handheld/TopViews.hpp>

#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/GameWords.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>

#include <ScratchPadd/Motion/Motion.hpp>
#include <ScratchPadd/Scene/TextWrap.hpp>

#include <algorithm>
#include <cmath>
#include <string>

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
// A square turned by `angle` (radians) about its middle: two triangles (the
// overlay's rectangles don't turn)
void turnedSquare(Scene::Overlay &overlay, float x, float y, float half, float angle,
                  Scene::Colour colour) {
  std::array<std::array<float, 2>, 4> c{};
  for (int i = 0; i < 4; ++i) {
    const float a = angle + kPi / 4 + static_cast<float>(i) * kPi / 2;
    c[static_cast<std::size_t>(i)] = {x + half * std::sqrt(2.0f) * std::cos(a),
                                      y + half * std::sqrt(2.0f) * std::sin(a)};
  }
  overlay.triangle(c[0][0], c[0][1], c[1][0], c[1][1], c[2][0], c[2][1], colour);
  overlay.triangle(c[0][0], c[0][1], c[2][0], c[2][1], c[3][0], c[3][1], colour);
}
} // namespace

// ---------------------------------------------------------------- Discovery

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
  }
}

// ------------------------------------------------------------------- Dealt

void drawDealtCards(const TopFrame &f, const DealtShow &show, int chosen,
                    const std::array<ScratchPadd::PictureSlot, 2> &closeUps) {
  const std::vector<Art::Card> &cards = show.cards();
  if (!show.on() || cards.empty())
    return;
  const float k = f.k();
  const float w = 100 * k, pictureH = w * 52 / 63, cardH = w * 88 / 63, gap = 16 * k;
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
    const bool sharp = i < closeUps.size() && closeUps[i].showing(static_cast<int>(card));
    picture(f, sharp ? f.pictures.closeUpCardSquare : f.pictures.pictureSquare,
            sharp ? closeUps[i].texture() : f.pictures.cards[static_cast<std::size_t>(card)], cx, y,
            cw, pictureH);
    // The card's body under its picture, and once flat, its words
    f.overlay.rectangle(cx, y + pictureH, cw, cardH - pictureH, {236, 232, 220, 255});
    if (turned < 1)
      continue;
    const Art::CardWords words = Art::wordsOf(card);
    // (the title white, centred on the label band, 3 to 19 of the card's
    // 88 units, as large as fits its width: the user's)
    float titleScale = 2 * k;
    while (titleScale > k && Scene::Overlay::textWidth(words.shortTitle, titleScale) > w * 54 / 63)
      titleScale -= 0.25f * k;
    f.overlay.textCentred(x + w / 2, y + w * 11 / 63, words.shortTitle, Palette::kWords,
                          titleScale);
    // (wrapped by the width the letters take as drawn, spacing included)
    const auto lines = Scene::wrapText(words.words, w - 8 * k, [k](std::string_view line) {
      return Scene::Overlay::textWidth(line, k);
    });
    for (std::size_t l = 0; l < lines.size() && l < 6; ++l)
      f.overlay.text(x + 4 * k, y + pictureH + (3 + 9 * static_cast<float>(l)) * k, lines[l],
                     {40, 40, 48}, k);
    if (static_cast<int>(i) == chosen) {
      f.overlay.rectangle(x - 3 * k, y - 3 * k, w + 6 * k, 2 * k, Palette::kHighlight);
      f.overlay.rectangle(x - 3 * k, y + cardH + k, w + 6 * k, 2 * k, Palette::kHighlight);
      f.overlay.rectangle(x - 3 * k, y - 3 * k, 2 * k, cardH + 6 * k, Palette::kHighlight);
      f.overlay.rectangle(x + w + k, y - 3 * k, 2 * k, cardH + 6 * k, Palette::kHighlight);
    }
  }
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
  const float backW = 28 * k, backH = backW * 88 / 63, backStep = 9 * k, rowStep = 18 * k;
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
              y + 11 * k + static_cast<float>(i / kBacksARow) * rowStep, backW, backH,
              kPictureDistance * (1 - 0.002f * static_cast<float>(i)));
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
    const float y = top + (110 + 25 * static_cast<float>(i / 8)) * k + extra;
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

void drawFirstRoll(const TopFrame &f, const FirstRoll &roll, float since) {
  if (roll.rounds.empty())
    return;
  const float k = f.k();
  Scene::Overlay &overlay = f.overlay;
  overlay.rectangle(0, 0, f.width, f.height, {10, 15, 26, 240});
  overlay.textCentred(f.width / 2, 14 * k, "WHO GOES FIRST", Palette::kWords, 2 * k);
  const int players = static_cast<int>(roll.rounds.front().size());
  const float gap = 58 * k;
  const float left = f.width / 2 - gap * static_cast<float>(players - 1) / 2;
  for (int p = 0; p < players; ++p)
    overlay.textCentred(left + gap * static_cast<float>(p), 32 * k, f.names.shortOf(p),
                        Palette::player(p), k);
  const std::size_t rounds = std::min(roll.rounds.size(), FirstRollShow::kRoundsShown);
  const bool settled = FirstRollShow::settled(roll, since);
  float y = 54 * k;
  for (std::size_t r = 0; r < rounds; ++r) {
    const auto tumble = FirstRollShow::tumble(r, since);
    if (!tumble)
      break;
    const float t = *tumble;
    const float half = (r == 0 ? 14 : 10) * k;
    if (r > 0)
      overlay.textCentred(f.width / 2, y - half - 7 * k, "tied: they throw again",
                          Palette::kHighlight, k);
    for (int p = 0; p < players; ++p) {
      const int face = roll.rounds[r][static_cast<std::size_t>(p)];
      if (face == 0)
        continue;
      const float x = left + gap * static_cast<float>(p);
      // Dropping in from just under the title and settling (Settle: a
      // small bounce), turning less and less, its number flickering until
      // it lands
      const float landed = Motion::along(Motion::Curve::Settle, t);
      const float dy = (1 - landed) * -24 * k;
      const float angle = (1 - t) * (1 - t) * (5.0f + static_cast<float>(p));
      const bool winner = settled && r + 1 == rounds && p == roll.first;
      turnedSquare(overlay, x, y + dy, half + 2 * k, angle,
                   winner ? Palette::kHighlight : Palette::player(p));
      turnedSquare(overlay, x, y + dy, half, angle, {242, 242, 242, 255});
      const int shown = t >= 1 ? face : 1 + (p * 7 + static_cast<int>(since * 14)) % 6;
      overlay.textCentred(x, y + dy, std::to_string(shown), {30, 30, 36}, (r == 0 ? 2 : 1.5f) * k);
    }
    y += 2 * half + 22 * k;
  }
  // Once the last throw has settled: the order, by name
  if (since < FirstRollShow::kSecondsRound * static_cast<float>(rounds - 1) +
                  FirstRollShow::kSecondsTumble + 0.3f)
    return;
  std::string homes = "Homes: ", ships = "Ships: ";
  for (int i = 0; i < players; ++i) {
    homes += (i ? ", " : "") + f.names[(roll.first + i) % players];
    ships += (i ? ", " : "") + f.names[(roll.first + players - 1 - i) % players];
  }
  const float bottom = f.height - 44 * k;
  overlay.textCentred(f.width / 2, bottom, f.names[roll.first] + " goes first",
                      Palette::player(roll.first), 1.5f * k);
  overlay.textCentred(f.width / 2, bottom + 16 * k, homes, Palette::kWords, k);
  overlay.textCentred(f.width / 2, bottom + 28 * k, ships + " (back again)", Palette::kSoftWords,
                      k);
}

// ---------------------------------------------------------------- Home pay

namespace {
// A five-pointed star (the home die's "your choice" face), from triangles
void star(Scene::Overlay &overlay, float x, float y, float r, Scene::Colour colour) {
  std::array<std::array<float, 2>, 10> p{};
  for (int i = 0; i < 10; ++i) {
    const float a = -kPi / 2 + static_cast<float>(i) * kPi / 5;
    const float d = i % 2 == 0 ? r : r * 0.42f;
    p[static_cast<std::size_t>(i)] = {x + d * std::cos(a), y + d * std::sin(a)};
  }
  for (int i = 0; i < 10; ++i) {
    const auto &a = p[static_cast<std::size_t>(i)];
    const auto &b = p[static_cast<std::size_t>((i + 1) % 10)];
    overlay.triangle(x, y, a[0], a[1], b[0], b[1], colour);
  }
}
// A home die's face: a resource's colour as a disc, or the star
void dieFace(Scene::Overlay &overlay, float x, float y, float half, int face) {
  if (face >= kResources)
    star(overlay, x, y, half * 0.75f, Palette::kHighlight);
  else
    overlay.hexagon(x, y, half * 0.6f, BoardColours::ofKind(static_cast<PlanetKind>(face)), true);
}
} // namespace

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
  // Tumbling: turning less and less, its faces flickering; landing on its
  // face with a small pop (Settle: a small thing may squash a little)
  const float landed = Motion::along(Motion::Curve::Settle, m.settle);
  const float half =
      (6 + 10 * std::min(1.0f, m.up)) * k * (m.settle > 0 ? 0.85f + 0.15f * landed : 1);
  const float angle = (1 - m.tumble) * (1 - m.tumble) * 7 + (1 - m.up) * 2;
  const bool showsFace = m.tumble >= 1;
  const int face =
      showsFace ? pay->face : (pay->face + 1 + static_cast<int>(*seconds * 12)) % (kResources + 1);
  if (m.away < 1) {
    turnedSquare(overlay, dieX, dieY, half + 1.5f * k, angle, BoardColours::ofPlayer(pay->player));
    turnedSquare(overlay, dieX, dieY, half, angle, {244, 242, 236, 255});
    dieFace(overlay, dieX, dieY, half, face);
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
    overlay.textCentred(cx, cy - h / 2 + h * 11 / 52, std::string(GameWords::of(*pay->resource)),
                        Palette::kWords, 1.5f * k);
  if (m.away <= 0 && m.card >= 1)
    overlay.textCentred(f.width / 2, heldY + bigH / 2 + 10 * k,
                        "+1 " + std::string(GameWords::of(*pay->resource)), Palette::kWords,
                        1.5f * k);
}

} // namespace CosmicHex::Handheld
