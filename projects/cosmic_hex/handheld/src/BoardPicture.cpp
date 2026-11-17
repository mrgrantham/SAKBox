#include <CosmicHex/Handheld/BoardPicture.hpp>

#include <CosmicHex/Handheld/BoardPieces.hpp>

#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <HexGrid/CornersAndEdges.hpp>

#include <algorithm>
#include <cmath>

namespace CosmicHex::Handheld {

namespace {

using ScratchPadd::Scene::Colour;
namespace TR = ScratchPadd::TextureRecipe;

// The board's hexes reach 4 from the centre: 9 across, pointy-topped, so
// 9 x sqrt(3) hex sizes wide and 14 tall (with a little margin)
constexpr float kBoardWide = 9 * 1.7320508f + 0.4f, kBoardTall = 14.4f;

// `over` on `under` by over's alpha
Colour blend(Colour under, Colour over) {
  const int a = over.a;
  const auto mix = [a](int u, int o) {
    return static_cast<std::uint8_t>((o * a + u * (255 - a)) / 255);
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

BoardPicture::BoardPicture(int width, int height)
    : width_(width), height_(height),
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

void BoardPicture::ensureFaces() {
  // A face's square: the hex is 47% of it from centre to corner (Art.hpp)
  const int size = static_cast<int>(std::ceil(hexSize() / 0.47f));
  if (size == facesSize_)
    return;
  facesSize_ = size;
  for (int i = 0; i < Art::kFaces; ++i) {
    std::vector<Colour> &face = faces_[static_cast<std::size_t>(i)];
    face.assign(static_cast<std::size_t>(size) * static_cast<std::size_t>(size),
                Colour{0, 0, 0, 0});
    const auto recipe = TR::read(Art::recipeOf(static_cast<Art::Face>(i)));
    if (!recipe)
      continue;
    const TR::Image image = TR::draw(*recipe, size, size, TR::Format::Rgba8);
    for (std::size_t p = 0; p < face.size(); ++p)
      face[p] = {image.pixels[p * 4], image.pixels[p * 4 + 1], image.pixels[p * 4 + 2],
                 image.pixels[p * 4 + 3]};
  }
}

void BoardPicture::blendFace(Art::Face face, HexGrid::Point centre) {
  const std::vector<Colour> &sprite = faces_[static_cast<std::size_t>(face)];
  const int left = static_cast<int>(std::lround(centre.x)) - facesSize_ / 2;
  const int top = static_cast<int>(std::lround(centre.y)) - facesSize_ / 2;
  for (int y = 0; y < facesSize_; ++y) {
    const int sy = top + y;
    if (sy < 0 || sy >= height_)
      continue;
    for (int x = 0; x < facesSize_; ++x) {
      const int sx = left + x;
      if (sx < 0 || sx >= width_)
        continue;
      const Colour over = sprite[indexOf(x, y, facesSize_)];
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
  const float minX = std::min({a.x, b.x, c.x}), maxX = std::max({a.x, b.x, c.x});
  const float minY = std::min({a.y, b.y, c.y}), maxY = std::max({a.y, b.y, c.y});
  const auto side = [](HexGrid::Point p, HexGrid::Point q, float x, float y) {
    return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x);
  };
  for (int py = std::max(0, static_cast<int>(minY));
       py <= std::min(height_ - 1, static_cast<int>(maxY)); ++py)
    for (int px = std::max(0, static_cast<int>(minX));
         px <= std::min(width_ - 1, static_cast<int>(maxX)); ++px) {
      const float x = static_cast<float>(px) + 0.5f, y = static_cast<float>(py) + 0.5f;
      const float s0 = side(a, b, x, y), s1 = side(b, c, x, y), s2 = side(c, a, x, y);
      if ((s0 >= 0 && s1 >= 0 && s2 >= 0) || (s0 <= 0 && s1 <= 0 && s2 <= 0))
        pixels_[indexOf(px, py, width_)] = colour;
    }
}

void BoardPicture::draw(const State &state) {
  ensureFaces();
  std::fill(pixels_.begin(), pixels_.end(), BoardColours::kSpace);
  // The tiles' faces
  for (const Planet &planet : state.planets)
    blendFace(faceOf(planet), where(planet.hex));
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

void BoardPicture::line(HexGrid::Point a, HexGrid::Point b, float thickness, Colour colour) {
  const float length = distance(a, b);
  const int steps = std::max(1, static_cast<int>(length));
  for (int i = 0; i <= steps; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    fillRect(a.x + (b.x - a.x) * t - thickness / 2, a.y + (b.y - a.y) * t - thickness / 2,
             thickness, thickness, colour);
  }
}

void BoardPicture::mark(const Spot &spot) {
  constexpr Colour kHighlight{255, 214, 90, 255};
  for (int i = 0; i < 6; ++i)
    line(where(HexGrid::corner(spot.hex, i)), where(HexGrid::corner(spot.hex, i + 1)), 2,
         kHighlight);
  if (spot.corner) {
    const HexGrid::Point at = where(*spot.corner);
    const float r = 0.2f * hexSize();
    fillRect(at.x - r, at.y - r, 2 * r, 2 * r, kHighlight);
  }
}

std::optional<BoardPicture::Spot> BoardPicture::at(int x, int y) const {
  const HexGrid::Point touch{static_cast<float>(x), static_cast<float>(y)};
  const Hex hex = layout().hexAt(touch);
  if (HexGrid::length(hex) > 4)
    return std::nullopt;
  // A corner when the touch is within a third of a hex of it
  std::optional<Corner> nearest;
  float best = hexSize() / 3;
  for (int i = 0; i < 6; ++i) {
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
