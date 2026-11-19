#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/Palette.hpp>

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

// `over` on `under` by over's alpha. Without dividing: the DS's
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

// ---------------------------------------------------------------------------
// Drawing, clipped to clip_: everything below draws only inside it, so a
// part of the picture can be drawn again on its own (draw())

namespace {
constexpr Colour kGrid{58, 74, 104, 255};
constexpr Colour kHighlight = Palette::kHighlight;
constexpr Colour kDark{10, 14, 24, 255};

int rounded(float v) { return static_cast<int>(std::lround(v)); }

// A square sprite `size` a side centred on `centre`: its pixels
BoardPicture::Area spriteArea(HexGrid::Point centre, int size) {
  return {rounded(centre.x) - size / 2, rounded(centre.y) - size / 2, size, size};
}
// A rectangle's pixels, rounded as fillRect rounds it
BoardPicture::Area rectArea(float x, float y, float w, float h) {
  const int x0 = rounded(x), y0 = rounded(y);
  return {x0, y0, rounded(x + w) - x0, rounded(y + h) - y0};
}
// The smallest area holding both
BoardPicture::Area joined(BoardPicture::Area a, BoardPicture::Area b) {
  if (a.width <= 0 || a.height <= 0)
    return b;
  if (b.width <= 0 || b.height <= 0)
    return a;
  const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
  const int x1 = std::max(a.x + a.width, b.x + b.width),
            y1 = std::max(a.y + a.height, b.y + b.height);
  return {x0, y0, x1 - x0, y1 - y0};
}
bool overlap(BoardPicture::Area a, BoardPicture::Area b) {
  return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}
} // namespace

void BoardPicture::blendSpriteOf(const std::vector<Colour> &own, const std::vector<Colour> &whole,
                                 int ownSize, int wholeSize, HexGrid::Point centre) {
  if (!own.empty()) {
    blendSprite(own, ownSize, centre);
    return;
  }
  if (whole.empty())
    return;
  // The whole board's, scaled up: each pixel its nearest (whole numbers)
  const Area area = spriteArea(centre, ownSize);
  for (int y = std::max(area.y, clip_.y); y < std::min(area.y + ownSize, clip_.y + clip_.height);
       ++y) {
    const int from = (y - area.y) * wholeSize / ownSize;
    for (int x = std::max(area.x, clip_.x); x < std::min(area.x + ownSize, clip_.x + clip_.width);
         ++x) {
      const Colour over = whole[indexOf((x - area.x) * wholeSize / ownSize, from, wholeSize)];
      if (over.a == 0)
        continue;
      Colour &under = pixels_[indexOf(x, y, width_)];
      under = blend(under, over);
    }
  }
}

void BoardPicture::blendSprite(const std::vector<Colour> &sprite, int size, HexGrid::Point centre) {
  if (sprite.empty())
    return;
  const Area area = spriteArea(centre, size);
  for (int y = std::max(area.y, clip_.y); y < std::min(area.y + size, clip_.y + clip_.height); ++y)
    for (int x = std::max(area.x, clip_.x); x < std::min(area.x + size, clip_.x + clip_.width);
         ++x) {
      const Colour over = sprite[indexOf(x - area.x, y - area.y, size)];
      if (over.a == 0)
        continue;
      Colour &under = pixels_[indexOf(x, y, width_)];
      under = blend(under, over);
    }
}

void BoardPicture::fillRect(float x, float y, float w, float h, Colour colour) {
  const Area area = rectArea(x, y, w, h);
  for (int py = std::max(area.y, clip_.y);
       py < std::min(area.y + area.height, clip_.y + clip_.height); ++py)
    for (int px = std::max(area.x, clip_.x);
         px < std::min(area.x + area.width, clip_.x + clip_.width); ++px)
      pixels_[indexOf(px, py, width_)] = colour;
}

namespace {
// A triangle's corners in whole numbers, 1/256ths of a pixel (// in floats, which the DS hasn't
// hardware for, each ship's pixels took about a millisecond)
struct FixedPoint2 {
  std::int64_t x, y;
};
FixedPoint2 fixedOf(HexGrid::Point p) { return {std::lround(p.x * 256), std::lround(p.y * 256)}; }
int pixelOf(std::int64_t v) { return static_cast<int>(v >> 8); } // (rounded down)
} // namespace

BoardPicture::Area BoardPicture::triangleArea(HexGrid::Point a, HexGrid::Point b,
                                              HexGrid::Point c) {
  const FixedPoint2 p0 = fixedOf(a), p1 = fixedOf(b), p2 = fixedOf(c);
  const int x0 = pixelOf(std::min({p0.x, p1.x, p2.x})), x1 = pixelOf(std::max({p0.x, p1.x, p2.x}));
  const int y0 = pixelOf(std::min({p0.y, p1.y, p2.y})), y1 = pixelOf(std::max({p0.y, p1.y, p2.y}));
  return {x0, y0, x1 - x0 + 1, y1 - y0 + 1};
}

void BoardPicture::fillTriangle(HexGrid::Point a, HexGrid::Point b, HexGrid::Point c,
                                Colour colour) {
  // Each pixel's middle tested against the three sides, in whole numbers
  const FixedPoint2 p0 = fixedOf(a), p1 = fixedOf(b), p2 = fixedOf(c);
  const auto side = [](FixedPoint2 p, FixedPoint2 q, std::int64_t x, std::int64_t y) {
    return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x);
  };
  const Area area = triangleArea(a, b, c);
  for (int py = std::max(area.y, clip_.y);
       py < std::min(area.y + area.height, clip_.y + clip_.height); ++py)
    for (int px = std::max(area.x, clip_.x);
         px < std::min(area.x + area.width, clip_.x + clip_.width); ++px) {
      const std::int64_t x = px * 256 + 128, y = py * 256 + 128;
      const std::int64_t s0 = side(p0, p1, x, y), s1 = side(p1, p2, x, y), s2 = side(p2, p0, x, y);
      if ((s0 >= 0 && s1 >= 0 && s2 >= 0) || (s0 <= 0 && s1 <= 0 && s2 <= 0))
        pixels_[indexOf(px, py, width_)] = colour;
    }
}

void BoardPicture::line(HexGrid::Point a, HexGrid::Point b, int thickness, Colour colour) {
  forEachLinePixel(a, b, thickness, [&](int x, int y) {
    if (x >= clip_.x && x < clip_.x + clip_.width && y >= clip_.y && y < clip_.y + clip_.height)
      pixels_[indexOf(x, y, width_)] = colour;
  });
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

// ---------------------------------------------------------------------------
// The pieces: what's drawn, listed, so that what changed can be found

void BoardPicture::listPieces(const State &state, const Marks &marks,
                              std::vector<Piece> &out) const {
  out.clear();
  const float s = hexSize();
  const auto on = [](Hex hex) {
    return std::array<std::int16_t, 3>{static_cast<std::int16_t>(hex.q),
                                       static_cast<std::int16_t>(hex.r), 0};
  };
  const auto at = [](Corner corner) {
    return std::array<std::int16_t, 3>{static_cast<std::int16_t>(corner.hex.q),
                                       static_cast<std::int16_t>(corner.hex.r),
                                       static_cast<std::int16_t>(corner.index)};
  };
  // (a face or chip drawn sharp, or stood in for, looks different)
  const bool zoomedFaces = zoom_ != 1 && zoomed_.facesSize == facesSize();
  const bool zoomedChips = zoom_ != 1 && zoomed_.chipsSize == chipsSize();
  const auto faceLook = [&](Art::Face face) {
    const bool sharp =
        zoom_ == 1 || (zoomedFaces && !zoomed_.faces[static_cast<std::size_t>(face)].empty());
    return std::array<std::int16_t, 2>{static_cast<std::int16_t>(face), sharp};
  };
  for (const Planet &planet : state.planets) {
    const HexGrid::Point centre = where(planet.hex);
    out.push_back({Piece::Kind::Face, on(planet.hex), faceLook(faceOf(planet)),
                   spriteArea(centre, facesSize())});
    if (const int number = chipOf(planet); number > 0) {
      const bool sharp =
          zoom_ == 1 || (zoomedChips && !zoomed_.chips[static_cast<std::size_t>(number)].empty());
      out.push_back({Piece::Kind::Chip,
                     on(planet.hex),
                     {static_cast<std::int16_t>(number), sharp},
                     spriteArea({centre.x, centre.y + kChipDown * s}, chipsSize())});
    }
  }
  for (const ZoneSite &site : state.zone)
    out.push_back({Piece::Kind::Face, on(site.hex), faceLook(faceOf(site)),
                   spriteArea(where(site.hex), facesSize())});
  for (const Station &station : state.stations) {
    const HexGrid::Point c = where(station.at);
    const float half = station.colony ? 0.26f * s : 0.18f * s;
    out.push_back({Piece::Kind::Station,
                   at(station.at),
                   {static_cast<std::int16_t>(station.owner), station.colony},
                   rectArea(c.x - half - 1, c.y - half - 1, 2 * half + 2, 2 * half + 2)});
  }
  for (const Ship &ship : state.ships) {
    const HexGrid::Point c = where(ship.corner);
    const float r = 0.3f * s;
    out.push_back(
        {Piece::Kind::Ship,
         at(ship.corner),
         {static_cast<std::int16_t>(ship.owner), 0},
         triangleArea({c.x, c.y - r - 1}, {c.x - r - 1, c.y + r + 1}, {c.x + r + 1, c.y + r + 1})});
  }
  if (state.blockade >= 0) {
    const Hex hex = state.planets[static_cast<std::size_t>(state.blockade)].hex;
    const HexGrid::Point c = where(hex);
    const float r = 0.4f * s;
    out.push_back(
        {Piece::Kind::Blockade,
         on(hex),
         {0, 0},
         triangleArea({c.x, c.y - r - 2}, {c.x - r - 2, c.y + r + 2}, {c.x + r + 2, c.y + r + 2})});
  }
  if (marks.hex) {
    // (its outline two pixels thick: a pixel's room round its corners)
    Area area{};
    for (int i = 0; i < 6; ++i) {
      const HexGrid::Point p = where(HexGrid::corner(*marks.hex, i));
      area = joined(area, {rounded(p.x) - 1, rounded(p.y) - 1, 3, 3});
    }
    out.push_back({Piece::Kind::MarkHex, on(*marks.hex), {0, 0}, area});
  }
  for (const Corner &corner : marks.corners) {
    const HexGrid::Point c = where(corner);
    const float r = 0.2f * s;
    out.push_back(
        {Piece::Kind::MarkCorner, at(corner), {0, 0}, rectArea(c.x - r, c.y - r, 2 * r, 2 * r)});
  }
}

void BoardPicture::drawPiece(const Piece &piece) {
  const Hex hex{piece.place[0], piece.place[1]};
  const Corner corner{hex, piece.place[2]};
  const float s = hexSize();
  switch (piece.kind) {
  case Piece::Kind::Face: {
    const auto face = static_cast<std::size_t>(piece.look[0]);
    if (zoom_ == 1)
      blendSprite(whole_.faces[face], whole_.facesSize, where(hex));
    else
      blendSpriteOf(zoomed_.facesSize == facesSize() ? zoomed_.faces[face] : kNoSprite,
                    whole_.faces[face], facesSize(), whole_.facesSize, where(hex));
    break;
  }
  case Piece::Kind::Chip: {
    // over the face's lower part
    HexGrid::Point c = where(hex);
    c.y += kChipDown * s;
    const auto n = static_cast<std::size_t>(piece.look[0]);
    if (zoom_ == 1)
      blendSprite(whole_.chips[n], whole_.chipsSize, c);
    else
      blendSpriteOf(zoomed_.chipsSize == chipsSize() ? zoomed_.chips[n] : kNoSprite,
                    whole_.chips[n], chipsSize(), whole_.chipsSize, c);
    break;
  }
  case Piece::Kind::Station: {
    // a colony bigger, with a dark middle
    const HexGrid::Point c = where(corner);
    const bool colony = piece.look[1] != 0;
    const float half = colony ? 0.26f * s : 0.18f * s;
    fillRect(c.x - half - 1, c.y - half - 1, 2 * half + 2, 2 * half + 2, kDark);
    fillRect(c.x - half, c.y - half, 2 * half, 2 * half, BoardColours::ofPlayer(piece.look[0]));
    if (colony)
      fillRect(c.x - 0.08f * s, c.y - 0.08f * s, 0.16f * s, 0.16f * s, kDark);
    break;
  }
  case Piece::Kind::Ship: {
    // a dart pointing up
    const HexGrid::Point c = where(corner);
    const float r = 0.3f * s;
    fillTriangle({c.x, c.y - r - 1}, {c.x - r - 1, c.y + r + 1}, {c.x + r + 1, c.y + r + 1}, kDark);
    fillTriangle({c.x, c.y - r}, {c.x - r, c.y + r}, {c.x + r, c.y + r},
                 BoardColours::ofPlayer(piece.look[0]));
    break;
  }
  case Piece::Kind::Blockade: {
    // a dark dart with a light rim, on its planet
    const HexGrid::Point c = where(hex);
    const float r = 0.4f * s;
    fillTriangle({c.x, c.y - r - 2}, {c.x - r - 2, c.y + r + 2}, {c.x + r + 2, c.y + r + 2},
                 {220, 70, 70, 255});
    fillTriangle({c.x, c.y - r}, {c.x - r, c.y + r}, {c.x + r, c.y + r}, BoardColours::kBlockade);
    break;
  }
  case Piece::Kind::MarkHex:
    for (int i = 0; i < 6; ++i)
      line(where(HexGrid::corner(hex, i)), where(HexGrid::corner(hex, i + 1)), 2, kHighlight);
    break;
  case Piece::Kind::MarkCorner: {
    const HexGrid::Point c = where(corner);
    const float r = 0.2f * s;
    fillRect(c.x - r, c.y - r, 2 * r, 2 * r, kHighlight);
    break;
  }
  }
}

void BoardPicture::drawArea(Area area) {
  clip_ = area;
  // Space, then the grid (every hex outlined, to plan paths along
  // the edges, in empty space too), then each piece over it, in order
  for (int y = area.y; y < area.y + area.height; ++y)
    std::fill_n(pixels_.begin() + static_cast<std::ptrdiff_t>(indexOf(area.x, y, width_)),
                area.width, BoardColours::kSpace);
  for (const std::uint32_t pixel : gridPixels_) {
    const int x = static_cast<int>(pixel % static_cast<std::uint32_t>(width_)),
              y = static_cast<int>(pixel / static_cast<std::uint32_t>(width_));
    if (x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height)
      pixels_[pixel] = kGrid;
  }
  for (const Piece &piece : pieces_)
    if (overlap(piece.area, area))
      drawPiece(piece);
}

void BoardPicture::redrawNext(Area area) { waiting_.push_back(area); }

void BoardPicture::draw(const State &state, const Marks &marks) {
  // (the whole board's faces at least: they're what the rest falls back on)
  while (!whole_.done())
    prepare();
  buildGrid();
  const Area whole{0, 0, width_, height_};
  std::swap(pieces_, drawnPieces_);
  listPieces(state, marks, pieces_);
  changed_.clear();
  // All of it: the first time, or seen differently (zoomed or moved)
  const View view{zoom_, offset_.x, offset_.y};
  if (!drawnView_ || *drawnView_ != view) {
    drawnView_ = view;
    waiting_.clear();
    changed_.push_back(whole);
    drawArea(whole);
    return;
  }
  // Else where a piece has gone, come or changed (the two lists compared,
  // sorted: a changed piece is one gone and one come), and the areas asked
  // for (redrawNext)
  std::vector<Area> &areas = waiting_;
  std::vector<Piece> before = drawnPieces_, after = pieces_;
  std::sort(before.begin(), before.end());
  std::sort(after.begin(), after.end());
  std::size_t b = 0, a = 0;
  while (b < before.size() || a < after.size()) {
    if (a == after.size() || (b < before.size() && before[b] < after[a]))
      areas.push_back(before[b++].area); // (gone)
    else if (b == before.size() || after[a] < before[b])
      areas.push_back(after[a++].area); // (come)
    else
      ++a, ++b; // (the same)
  }
  // Each on the picture, overlapping ones joined (so no pixel's drawn
  // twice), and at most kMostAreas, else one round them all
  for (Area &area : areas) {
    const int x0 = std::max(area.x, 0), y0 = std::max(area.y, 0);
    const int x1 = std::min(area.x + area.width, width_),
              y1 = std::min(area.y + area.height, height_);
    area = {x0, y0, x1 - x0, y1 - y0};
  }
  std::erase_if(areas, [](Area area) { return area.width <= 0 || area.height <= 0; });
  for (bool merged = true; merged;) {
    merged = false;
    for (std::size_t i = 0; i < areas.size() && !merged; ++i)
      for (std::size_t j = i + 1; j < areas.size() && !merged; ++j)
        if (overlap(areas[i], areas[j])) {
          areas[i] = joined(areas[i], areas[j]);
          areas.erase(areas.begin() + static_cast<std::ptrdiff_t>(j));
          merged = true;
        }
  }
  if (areas.size() > kMostAreas) {
    Area all{};
    for (const Area &area : areas)
      all = joined(all, area);
    areas = {all};
  }
  for (const Area &area : areas)
    drawArea(area);
  changed_ = std::move(areas);
  waiting_.clear();
}

void BoardPicture::redrawAll() { drawnView_.reset(); }

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
