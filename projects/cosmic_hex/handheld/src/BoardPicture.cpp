#include <CosmicHex/Handheld/BoardPicture.hpp>

#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>

#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <HexGrid/CornersAndEdges.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace CosmicHex::Handheld {

namespace {

using ScratchPadd::Scene::Colour;
namespace TR = ScratchPadd::TextureRecipe;

// The board's hexes reach 4 from the centre: 9 across, pointy-topped, so
// 9 x sqrt(3) hex sizes wide and 14 tall (with a little margin)
constexpr float kBoardWide = 9 * 1.7320508f + 0.4f, kBoardTall = 14.4f;

// `over` on `under` by over's alpha. Without dividing (round 92): the DS's
// ARM9 has no divide instruction, so each "/ 255" was a library call, three
// a pixel, and blending the tiles' faces took most of a redraw's 125 to 260
// ms. x / 255 is (x + 1 + (x >> 8)) >> 8 for 0 <= x <= 255 * 255, exactly
// (Blinn, "Three Wrongs Make a Right"); opaque pixels are copied.
Colour blend(Colour under, Colour over) {
  const int a = over.a;
  if (a == 255)
    return over;
  const auto mix = [a](int u, int o) {
    const int x = o * a + u * (255 - a);
    return static_cast<std::uint8_t>((x + 1 + (x >> 8)) >> 8);
  };
  return {mix(under.r, over.r), mix(under.g, over.g), mix(under.b, over.b), 255};
}

float distance(HexGrid::Point a, HexGrid::Point b) { return std::hypot(a.x - b.x, a.y - b.y); }

// A pixel's place in rows of `width`
std::size_t indexOf(int x, int y, int width) {
  return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
         static_cast<std::size_t>(x);
}

} // namespace

// Each pixel (on the picture) of a line `thickness` pixels wide from a to b,
// in whole pixels a step at a time along the longer axis (Bresenham's
// line): no floating point a pixel, which the DS does in software
template <typename Plot>
void BoardPicture::forEachLinePixel(HexGrid::Point a, HexGrid::Point b, int thickness,
                                    Plot plot) const {
  int x0 = static_cast<int>(std::lround(a.x)), y0 = static_cast<int>(std::lround(a.y));
  const int x1 = static_cast<int>(std::lround(b.x)), y1 = static_cast<int>(std::lround(b.y));
  const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
  const int stepX = x0 < x1 ? 1 : -1, stepY = y0 < y1 ? 1 : -1;
  const int before = (thickness - 1) / 2; // (a thick line's pixels left of and above the middle)
  int error = dx + dy;
  for (;;) {
    for (int py = y0 - before; py < y0 - before + thickness; ++py)
      for (int px = x0 - before; px < x0 - before + thickness; ++px)
        if (px >= 0 && px < width_ && py >= 0 && py < height_)
          plot(px, py);
    if (x0 == x1 && y0 == y1)
      break;
    const int twice = 2 * error;
    if (twice >= dy) {
      error += dy;
      x0 += stepX;
    }
    if (twice <= dx) {
      error += dx;
      y0 += stepY;
    }
  }
}

BoardPicture::BoardPicture(int width, int height, ScratchPadd::TextureRecipe::Smoothing smoothing)
    : width_(width), height_(height), smoothing_(smoothing),
      pixels_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {}

void BoardPicture::setZoom(int zoom) {
  zoom_ = std::clamp(zoom, 1, kMostZoom);
  if (zoom_ == 1)
    offset_ = {}; // (the whole board, centred)
  pan(0, 0);      // (kept in view)
}

void BoardPicture::pan(float dx, float dy) {
  // Not past the board's edge: at most half the board's size, beyond what
  // the screen shows of it
  const float limitX = std::max(0.0f, (kBoardWide * hexSize() - static_cast<float>(width_)) / 2);
  const float limitY = std::max(0.0f, (kBoardTall * hexSize() - static_cast<float>(height_)) / 2);
  offset_.x = std::clamp(offset_.x + dx, -limitX, limitX);
  offset_.y = std::clamp(offset_.y + dy, -limitY, limitY);
}

float BoardPicture::hexSize() const {
  const float fit =
      std::min(static_cast<float>(width_) / kBoardWide, static_cast<float>(height_) / kBoardTall);
  return std::floor(fit) * static_cast<float>(zoom_);
}

HexGrid::Layout BoardPicture::layout() const {
  return {.orientation = HexGrid::Orientation::PointyTop,
          .size = {hexSize(), hexSize()},
          .origin = {static_cast<float>(width_) / 2 + offset_.x,
                     static_cast<float>(height_) / 2 + offset_.y}};
}

HexGrid::Point BoardPicture::where(Hex hex) const { return layout().centre(hex); }
HexGrid::Point BoardPicture::where(Corner corner) const { return layout().point(corner); }

int BoardPicture::facesSize() const {
  // A face's square: the hex is 47% of it from centre to corner (Art.hpp)
  return static_cast<int>(std::ceil(hexSize() / 0.47f));
}
int BoardPicture::chipsSize() const {
  // The number chips, as big on the hex as on the 3D board's tiles
  return std::max(4, static_cast<int>(std::lround(2 * kChipRadius * hexSize())));
}

void BoardPicture::Sprites::startAt(int faceSize, int chipSize) {
  if (faceSize == facesSize && chipSize == chipsSize)
    return;
  facesSize = faceSize;
  chipsSize = chipSize;
  for (auto &face : faces)
    face.clear();
  for (auto &chip : chips)
    chip.clear();
  next = 0;
}

bool BoardPicture::Sprites::done() const {
  constexpr int kChips = Art::kHighestChip - Art::kLowestChip + 1;
  return next >= Art::kFaces + kChips;
}

void BoardPicture::Sprites::drawNext(ScratchPadd::TextureRecipe::Smoothing smoothing) {
  if (done())
    return;
  const int which = next++;
  if (which < Art::kFaces) {
    std::vector<Colour> face =
        recipePixels(Art::recipeOf(static_cast<Art::Face>(which)), facesSize, smoothing);
    // (a broken recipe: a see-through square, tested against on the Mac)
    if (face.empty())
      face.assign(static_cast<std::size_t>(facesSize) * static_cast<std::size_t>(facesSize),
                  Colour{0, 0, 0, 0});
    faces[static_cast<std::size_t>(which)] = std::move(face);
  } else {
    const int number = Art::kLowestChip + (which - Art::kFaces);
    chips[static_cast<std::size_t>(number)] =
        recipePixels(Art::chipRecipeOf(number), chipsSize, smoothing);
  }
}

bool BoardPicture::prepare() {
  // The whole board's first (zoom 1, kept); then this zoom's, if it isn't 1
  if (whole_.facesSize == 0) {
    const int keep = zoom_;
    zoom_ = 1;
    whole_.startAt(facesSize(), chipsSize());
    zoom_ = keep;
  }
  if (!whole_.done()) {
    whole_.drawNext(smoothing_);
    return false;
  }
  if (zoom_ == 1)
    return true;
  zoomed_.startAt(facesSize(), chipsSize());
  zoomed_.drawNext(smoothing_);
  return zoomed_.done();
}

bool BoardPicture::ready() const {
  return whole_.done() && (zoom_ == 1 || (zoomed_.facesSize == facesSize() && zoomed_.done()));
}

void BoardPicture::blendFace(Art::Face face, HexGrid::Point centre) {
  const auto f = static_cast<std::size_t>(face);
  if (zoom_ == 1)
    blendSprite(whole_.faces[f], whole_.facesSize, centre);
  else
    blendSpriteOf(zoomed_.facesSize == facesSize() ? zoomed_.faces[f] : std::vector<Colour>{},
                  whole_.faces[f], facesSize(), whole_.facesSize, centre);
}

void BoardPicture::blendSpriteOf(const std::vector<Colour> &own, const std::vector<Colour> &whole,
                                 int ownSize, int wholeSize, HexGrid::Point centre) {
  if (!own.empty()) {
    blendSprite(own, ownSize, centre);
    return;
  }
  if (whole.empty())
    return;
  // The whole board's, scaled up: each pixel its nearest (whole numbers)
  const int left = static_cast<int>(std::lround(centre.x)) - ownSize / 2;
  const int top = static_cast<int>(std::lround(centre.y)) - ownSize / 2;
  for (int y = std::max(0, -top); y < ownSize && top + y < height_; ++y) {
    const int from = y * wholeSize / ownSize;
    for (int x = std::max(0, -left); x < ownSize && left + x < width_; ++x) {
      const Colour over = whole[indexOf(x * wholeSize / ownSize, from, wholeSize)];
      if (over.a == 0)
        continue;
      Colour &under = pixels_[indexOf(left + x, top + y, width_)];
      under = blend(under, over);
    }
  }
}

void BoardPicture::blendSprite(const std::vector<Colour> &sprite, int size, HexGrid::Point centre) {
  if (sprite.empty())
    return;
  const int left = static_cast<int>(std::lround(centre.x)) - size / 2;
  const int top = static_cast<int>(std::lround(centre.y)) - size / 2;
  for (int y = 0; y < size; ++y) {
    const int sy = top + y;
    if (sy < 0 || sy >= height_)
      continue;
    for (int x = 0; x < size; ++x) {
      const int sx = left + x;
      if (sx < 0 || sx >= width_)
        continue;
      const Colour over = sprite[indexOf(x, y, size)];
      if (over.a == 0)
        continue;
      Colour &under = pixels_[indexOf(sx, sy, width_)];
      under = blend(under, over);
    }
  }
}

void BoardPicture::fillRect(float x, float y, float w, float h, Colour colour) {
  const int x0 = std::max(0, static_cast<int>(std::lround(x))),
            y0 = std::max(0, static_cast<int>(std::lround(y)));
  const int x1 = std::min(width_, static_cast<int>(std::lround(x + w))),
            y1 = std::min(height_, static_cast<int>(std::lround(y + h)));
  for (int py = y0; py < y1; ++py)
    for (int px = x0; px < x1; ++px)
      pixels_[indexOf(px, py, width_)] = colour;
}

void BoardPicture::fillTriangle(HexGrid::Point a, HexGrid::Point b, HexGrid::Point c,
                                Colour colour) {
  // In whole numbers, 1/256ths of a pixel (round 105: in floats, which the
  // DS hasn't hardware for, each ship's pixels took about a millisecond):
  // each pixel's middle tested against the three sides
  struct Fixed {
    std::int64_t x, y;
  };
  const auto fixed = [](HexGrid::Point p) {
    return Fixed{std::lround(p.x * 256), std::lround(p.y * 256)};
  };
  const Fixed p0 = fixed(a), p1 = fixed(b), p2 = fixed(c);
  const auto side = [](Fixed p, Fixed q, std::int64_t x, std::int64_t y) {
    return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x);
  };
  const auto pixel = [](std::int64_t v) { return static_cast<int>(v >> 8); }; // (rounded down)
  const int fromY = std::max(0, pixel(std::min({p0.y, p1.y, p2.y})));
  const int toY = std::min(height_ - 1, pixel(std::max({p0.y, p1.y, p2.y})));
  const int fromX = std::max(0, pixel(std::min({p0.x, p1.x, p2.x})));
  const int toX = std::min(width_ - 1, pixel(std::max({p0.x, p1.x, p2.x})));
  for (int py = fromY; py <= toY; ++py)
    for (int px = fromX; px <= toX; ++px) {
      const std::int64_t x = px * 256 + 128, y = py * 256 + 128;
      const std::int64_t s0 = side(p0, p1, x, y), s1 = side(p1, p2, x, y), s2 = side(p2, p0, x, y);
      if ((s0 >= 0 && s1 >= 0 && s2 >= 0) || (s0 <= 0 && s1 <= 0 && s2 <= 0))
        pixels_[indexOf(px, py, width_)] = colour;
    }
}

void BoardPicture::buildGrid() {
  if (zoom_ == gridZoom_ && offset_.x == gridOffset_.x && offset_.y == gridOffset_.y &&
      !gridPixels_.empty())
    return;
  gridZoom_ = zoom_;
  gridOffset_ = offset_;
  gridPixels_.clear();
  for (const Hex hex : HexGrid::spiral(Hex{0, 0}, 4))
    for (int i = 0; i < 6; ++i) // (shared edges twice: the same pixels)
      forEachLinePixel(where(HexGrid::corner(hex, i)), where(HexGrid::corner(hex, i + 1)), 1,
                       [&](int x, int y) {
                         gridPixels_.push_back(static_cast<std::uint32_t>(indexOf(x, y, width_)));
                       });
}

void BoardPicture::draw(const State &state) {
  // (the whole board's faces at least: they're what the rest falls back on)
  while (!whole_.done())
    prepare();
  std::fill(pixels_.begin(), pixels_.end(), BoardColours::kSpace);
  // The grid: every hex of the board outlined (round 81: easy to see, to
  // plan paths along the edges, in empty space too)
  constexpr Colour kGrid{58, 74, 104, 255};
  buildGrid();
  for (const std::uint32_t pixel : gridPixels_)
    pixels_[pixel] = kGrid;
  // The tiles' faces
  for (const Planet &planet : state.planets) {
    blendFace(faceOf(planet), where(planet.hex));
    // its number chip, over the face's lower part (round 87)
    if (const int number = chipOf(planet); number > 0) {
      HexGrid::Point at = where(planet.hex);
      at.y += kChipDown * hexSize();
      const auto n = static_cast<std::size_t>(number);
      if (zoom_ == 1)
        blendSprite(whole_.chips[n], whole_.chipsSize, at);
      else
        blendSpriteOf(zoomed_.chipsSize == chipsSize() ? zoomed_.chips[n] : std::vector<Colour>{},
                      whole_.chips[n], chipsSize(), whole_.chipsSize, at);
    }
  }
  for (const ZoneSite &site : state.zone)
    blendFace(faceOf(site), where(site.hex));
  // Stations (a colony bigger, with a dark middle), on their corners
  const float s = hexSize();
  for (const Station &station : state.stations) {
    const HexGrid::Point at = where(station.at);
    const Colour colour = BoardColours::ofPlayer(station.owner);
    const float half = station.colony ? 0.26f * s : 0.18f * s;
    fillRect(at.x - half - 1, at.y - half - 1, 2 * half + 2, 2 * half + 2, {10, 14, 24, 255});
    fillRect(at.x - half, at.y - half, 2 * half, 2 * half, colour);
    if (station.colony)
      fillRect(at.x - 0.08f * s, at.y - 0.08f * s, 0.16f * s, 0.16f * s, {10, 14, 24, 255});
  }
  // Ships: darts pointing up, on their corners
  for (const Ship &ship : state.ships) {
    const HexGrid::Point at = where(ship.corner);
    const float r = 0.3f * s;
    fillTriangle({at.x, at.y - r - 1}, {at.x - r - 1, at.y + r + 1}, {at.x + r + 1, at.y + r + 1},
                 {10, 14, 24, 255});
    fillTriangle({at.x, at.y - r}, {at.x - r, at.y + r}, {at.x + r, at.y + r},
                 BoardColours::ofPlayer(ship.owner));
  }
  // The blockade: a dark dart with a light rim, on its planet
  if (state.blockade >= 0) {
    const HexGrid::Point at = where(state.planets[static_cast<std::size_t>(state.blockade)].hex);
    const float r = 0.4f * s;
    fillTriangle({at.x, at.y - r - 2}, {at.x - r - 2, at.y + r + 2}, {at.x + r + 2, at.y + r + 2},
                 {220, 70, 70, 255});
    fillTriangle({at.x, at.y - r}, {at.x - r, at.y + r}, {at.x + r, at.y + r},
                 BoardColours::kBlockade);
  }
}

void BoardPicture::line(HexGrid::Point a, HexGrid::Point b, int thickness, Colour colour) {
  forEachLinePixel(a, b, thickness, [&](int x, int y) { pixels_[indexOf(x, y, width_)] = colour; });
}

namespace {
constexpr Colour kHighlight{255, 214, 90, 255};
}

void BoardPicture::markCorner(Corner corner) {
  const HexGrid::Point at = where(corner);
  const float r = 0.2f * hexSize();
  fillRect(at.x - r, at.y - r, 2 * r, 2 * r, kHighlight);
}

void BoardPicture::markHex(Hex hex) {
  for (int i = 0; i < 6; ++i)
    line(where(HexGrid::corner(hex, i)), where(HexGrid::corner(hex, i + 1)), 2, kHighlight);
}

std::optional<BoardPicture::Spot> BoardPicture::at(int x, int y) const {
  const HexGrid::Point touch{static_cast<float>(x), static_cast<float>(y)};
  const Hex hex = layout().hexAt(touch);
  if (HexGrid::length(hex) > 4)
    return std::nullopt;
  // The hex's corner nearest the touch
  Corner nearest = HexGrid::corner(hex, 0);
  float best = distance(touch, where(nearest));
  for (int i = 1; i < 6; ++i) {
    const Corner corner = HexGrid::corner(hex, i);
    const float d = distance(touch, where(corner));
    if (d < best) {
      best = d;
      nearest = corner;
    }
  }
  return Spot{hex, nearest};
}

} // namespace CosmicHex::Handheld
