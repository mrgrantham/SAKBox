#include <ScratchPadd/Scene/Font.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ScratchPadd::Scene {

namespace {
// A pixel font's atlas: its solid corner alone (8 x 8, the smallest texture
// a 3DS takes), for a backend drawing the overlay's shapes from it
constexpr int kSolidAtlasSide = 8;
constexpr std::array<std::uint8_t, kSolidAtlasSide * kSolidAtlasSide> kSolidAtlas = [] {
  std::array<std::uint8_t, kSolidAtlasSide * kSolidAtlasSide> solid{};
  solid.fill(255);
  return solid;
}();

// Calls `line` with each of `text`'s lines
template <typename Line> void forEachLine(std::string_view text, Line line) {
  std::size_t from = 0;
  while (true) {
    const std::size_t end = text.find('\n', from);
    line(text.substr(from, end == std::string_view::npos ? std::string_view::npos : end - from));
    if (end == std::string_view::npos)
      return;
    from = end + 1;
  }
}
} // namespace

std::size_t glyphIndex(char c) {
  if (c < kFirstCharacter || c > kLastCharacter)
    c = '?';
  return static_cast<std::size_t>(c - kFirstCharacter);
}

const Font &Font::platform() {
#if defined(__NDS__)
  return silkscreen();
#else
  return orbitron();
#endif
}

int Font::pixelHalves(float scale) const {
  // (a sixteenth's leeway, for a scale worked out as 1.9999)
  constexpr float kLeeway = 1.0f / 16;
  return pixels_ ? std::max(2, static_cast<int>(std::floor((scale * 2) + kLeeway))) : 2;
}

float Font::tracking(float scale) const {
  if (pixels_)
    return 0;
  // (a point up to the 3DS's top screen's scale, none from titles' 2.5 up)
  constexpr float kFullUpTo = 1.5625f, kNoneFrom = 2.5f, kMost = 1;
  return kMost * std::clamp((kNoneFrom - scale) / (kNoneFrom - kFullUpTo), 0.0f, 1.0f);
}

float Font::lineHeight(float scale) const {
  if (pixels_)
    return static_cast<float>(
        sized({0, 0, 0, static_cast<std::int16_t>(pixels_->lineHeight)}, pixelHalves(scale))
            .height);
  return smooth_->lineHeight * scale;
}

float Font::width(std::string_view text, float scale) const {
  const float size = pixels_ ? static_cast<float>(pixelHalves(scale)) / 2 : scale;
  const float between = tracking(scale);
  float widest = 0;
  forEachLine(text, [&](std::string_view line) {
    float pen = 0;
    for (const char c : line)
      pen += size * (pixels_ ? static_cast<float>(pixels_->glyphs[glyphIndex(c)].advance)
                             : smooth_->advances[glyphIndex(c)]);
    // (the room between letters: none after the last)
    if (!line.empty())
      pen += between * static_cast<float>(line.size() - 1);
    widest = std::max(widest, pen);
  });
  return widest;
}

Overlay::Ink Font::ink(std::string_view text, float scale) const {
  thread_local std::vector<TextPiece> pieces; // (no allocation a call)
  layOut(text, scale, pieces);
  if (pieces.empty())
    return {};
  float left = std::numeric_limits<float>::max(), top = left;
  float right = std::numeric_limits<float>::lowest(), bottom = right;
  for (const TextPiece &p : pieces) {
    left = std::min(left, p.x);
    top = std::min(top, p.y);
    right = std::max(right, p.x + p.width);
    bottom = std::max(bottom, p.y + p.height);
  }
  return {left, top, right - left, bottom - top};
}

void Font::layOut(std::string_view text, float scale, std::vector<TextPiece> &out) const {
  out.clear();
  if (pixels_)
    layOutPixels(text, scale, out);
  else
    layOutSmooth(text, scale, out);
}

void Font::pixelRectangles(std::string_view text, std::vector<FontRectangle> &out) const {
  out.clear();
  if (!pixels_)
    return;
  int pen = 0, top = 0;
  for (const char c : text) {
    if (c == '\n') {
      pen = 0;
      top += pixels_->lineHeight;
      continue;
    }
    const PixelGlyph &glyph = pixels_->glyphs[glyphIndex(c)];
    for (const PixelRun &run : pixels_->runs.subspan(glyph.firstRun, glyph.runCount))
      out.push_back({static_cast<std::int16_t>(pen + run.x), static_cast<std::int16_t>(top + run.y),
                     static_cast<std::int16_t>(run.length), 1});
    pen += glyph.advance;
  }
}

void Font::layOutPixels(std::string_view text, float scale, std::vector<TextPiece> &out) const {
  thread_local std::vector<FontRectangle> runs;
  pixelRectangles(text, runs);
  const int halves = pixelHalves(scale);
  for (const FontRectangle &run : runs) {
    const FontRectangle r = sized(run, halves);
    out.push_back({static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.width),
                   static_cast<float>(r.height)});
  }
}

// The baked size nearest `scale` (by their ratio: drawn scaled the least)
const BakedSize &Font::sizeFor(float scale) const {
  const auto apart = [scale](const BakedSize &size) {
    return std::abs(std::log(scale / size.scale));
  };
  return *std::ranges::min_element(smooth_->sizes, {}, apart);
}

void Font::layOutSmooth(std::string_view text, float scale, std::vector<TextPiece> &out) const {
  const BakedSize &size = sizeFor(scale);
  const float toPoints = scale / size.scale; // (the size's pixels to points)
  const float between = tracking(scale);
  float pen = 0, top = 0;
  for (const char c : text) {
    if (c == '\n') {
      pen = 0;
      top += lineHeight(scale);
      continue;
    }
    const std::size_t i = glyphIndex(c);
    const AtlasGlyph &g = size.glyphs[i];
    if (g.width > 0)
      out.push_back({pen + static_cast<float>(g.left) * toPoints,
                     top + static_cast<float>(g.top) * toPoints,
                     static_cast<float>(g.width) * toPoints,
                     static_cast<float>(g.height) * toPoints, g.x, g.y, g.width, g.height});
    pen += smooth_->advances[i] * scale + between;
  }
}

FontAtlas Font::atlas() const {
  if (smooth_)
    return {smooth_->atlasWidth, smooth_->atlasHeight, smooth_->atlas};
  return {kSolidAtlasSide, kSolidAtlasSide, kSolidAtlas};
}

} // namespace ScratchPadd::Scene
