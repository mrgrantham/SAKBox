// The overlay's fonts' data (make_fonts.py's), measured and laid out
#include <ScratchPadd/Scene/Font.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace ScratchPadd::Scene;

namespace {
const Font &kSilkscreen = Font::silkscreen();
const Font &kOrbitron = Font::orbitron();

std::string everyCharacter() {
  std::string all;
  for (char c = kFirstCharacter; c <= kLastCharacter; ++c)
    all += c;
  return all;
}
} // namespace

// Silkscreen has every printable character, each moving the pen, every one
// but the space with pixels, all within its advance and the line
TEST(FontTest, SilkscreenHasEveryCharacter) {
  const PixelFontData &data = *kSilkscreen.pixels();
  ASSERT_EQ(data.glyphs.size(), kCharacterCount);
  for (char c = kFirstCharacter; c <= kLastCharacter; ++c) {
    const PixelGlyph &glyph = data.glyphs[glyphIndex(c)];
    EXPECT_GT(glyph.advance, 0) << c;
    EXPECT_EQ(glyph.runCount == 0, c == ' ') << c;
    for (const PixelRun &run : data.runs.subspan(glyph.firstRun, glyph.runCount)) {
      EXPECT_GT(run.length, 0) << c;
      EXPECT_LE(run.x + run.length, glyph.advance) << c;
      EXPECT_LT(run.y, data.lineHeight) << c;
    }
  }
}

// Orbitron has every printable character at every baked size, each picture
// inside the atlas, the smallest size first
TEST(FontTest, OrbitronHasEveryCharacter) {
  const SmoothFontData &data = *kOrbitron.smooth();
  ASSERT_EQ(data.advances.size(), kCharacterCount);
  ASSERT_FALSE(data.sizes.empty());
  EXPECT_EQ(data.atlas.size(), static_cast<std::size_t>(data.atlasWidth * data.atlasHeight));
  float last = 0;
  for (const BakedSize &size : data.sizes) {
    EXPECT_GT(size.scale, last);
    last = size.scale;
    ASSERT_EQ(size.glyphs.size(), kCharacterCount);
    for (char c = kFirstCharacter; c <= kLastCharacter; ++c) {
      const AtlasGlyph &g = size.glyphs[glyphIndex(c)];
      EXPECT_GT(data.advances[glyphIndex(c)], 0) << c;
      EXPECT_EQ(g.width == 0, c == ' ') << c << " at " << size.scale;
      EXPECT_LE(g.x + g.width, data.atlasWidth) << c;
      EXPECT_LE(g.y + g.height, data.atlasHeight) << c;
    }
  }
}

// The atlas's top-left 2 x 2 is solid: the overlay's shapes are drawn from it
TEST(FontTest, TheAtlasHasASolidCorner) {
  for (const Font *font : {&kSilkscreen, &kOrbitron}) {
    const FontAtlas atlas = font->atlas();
    for (int y = 0; y < 2; ++y)
      for (int x = 0; x < 2; ++x)
        EXPECT_EQ(atlas.alpha[static_cast<std::size_t>(y) * static_cast<std::size_t>(atlas.width) +
                              static_cast<std::size_t>(x)],
                  255);
  }
}

// A character a font hasn't is drawn as '?'
TEST(FontTest, OtherCharactersAreQuestionMarks) {
  EXPECT_EQ(glyphIndex('\t'), glyphIndex('?'));
  EXPECT_EQ(glyphIndex(static_cast<char>(200)), glyphIndex('?'));
  EXPECT_EQ(glyphIndex('A'), static_cast<std::size_t>('A' - ' '));
  for (const Font *font : {&kSilkscreen, &kOrbitron}) {
    EXPECT_FLOAT_EQ(font->width("a\tb"), font->width("a?b"));
    std::vector<TextPiece> other, question;
    font->layOut("\x01", 1, other);
    font->layOut("?", 1, question);
    EXPECT_EQ(other.size(), question.size());
  }
}

// Silkscreen is drawn at whole multiples of its own pixels, at or below the
// scale (never wider than laid out for): 1 to 1.9 are its size, 2 twice
// it; its H is 6 pixels on, 5 tall
TEST(FontTest, SilkscreenScalesByWholePixels) {
  EXPECT_EQ(kSilkscreen.pixelSize(1), 1);
  EXPECT_EQ(kSilkscreen.pixelSize(1.5f), 1);
  EXPECT_EQ(kSilkscreen.pixelSize(1.9f), 1);
  EXPECT_EQ(kSilkscreen.pixelSize(1.9999f), 2);
  EXPECT_EQ(kSilkscreen.pixelSize(2), 2);
  EXPECT_EQ(kSilkscreen.pixelSize(0.3f), 1);
  EXPECT_FLOAT_EQ(kSilkscreen.width("H"), 6);
  EXPECT_FLOAT_EQ(kSilkscreen.width("Hi", 1.5f), kSilkscreen.width("Hi"));
  EXPECT_FLOAT_EQ(kSilkscreen.width("Hi", 2), 2 * kSilkscreen.width("Hi"));
  EXPECT_FLOAT_EQ(kSilkscreen.lineHeight(3), 3 * kSilkscreen.lineHeight());
  const Overlay::Ink h = kSilkscreen.ink("H");
  EXPECT_FLOAT_EQ(h.height, 5);
  EXPECT_FLOAT_EQ(h.width, 4);
  // ...and its pieces are its pixels, on whole points
  std::vector<TextPiece> pieces;
  kSilkscreen.layOut("H", 2, pieces);
  for (const TextPiece &piece : pieces) {
    EXPECT_TRUE(piece.solid());
    EXPECT_EQ(static_cast<int>(piece.x) % 2, 0);
    EXPECT_FLOAT_EQ(piece.height, 2);
  }
}

// Orbitron measures in proportion to the scale; at a baked size its
// letters are their pictures' own size, at another the nearest scaled
TEST(FontTest, OrbitronScalesSmoothly) {
  EXPECT_FLOAT_EQ(kOrbitron.width("Build", 2), 2 * kOrbitron.width("Build"));
  EXPECT_FLOAT_EQ(kOrbitron.lineHeight(1.5f), 1.5f * kOrbitron.lineHeight());
  std::vector<TextPiece> pieces;
  kOrbitron.layOut("H", 1.5625f, pieces);
  ASSERT_EQ(pieces.size(), 1u);
  EXPECT_FALSE(pieces[0].solid());
  EXPECT_FLOAT_EQ(pieces[0].width, pieces[0].atlasWidth);
  EXPECT_FLOAT_EQ(pieces[0].height, pieces[0].atlasHeight);
  std::vector<TextPiece> bigger;
  kOrbitron.layOut("H", 1.6f, bigger);
  EXPECT_EQ(bigger[0].atlasX, pieces[0].atlasX); // (the same picture, scaled)
  EXPECT_GT(bigger[0].width, pieces[0].width);
}

// Both fonts: a text's ink inside its width and lines, a new line a line
// height lower, the widest line its width
TEST(FontTest, MeasuringFollowsTheLetters) {
  for (const Font *font : {&kSilkscreen, &kOrbitron}) {
    const std::string all = everyCharacter();
    const Overlay::Ink ink = font->ink(all);
    EXPECT_GE(ink.left, 0);
    EXPECT_GE(ink.top, 0);
    EXPECT_LE(ink.left + ink.width, font->width(all) + 1);
    EXPECT_LE(ink.top + ink.height, font->lineHeight());
    EXPECT_FLOAT_EQ(font->width("ab\nabcd"), font->width("abcd"));
    EXPECT_GE(font->ink("a\na").height, font->lineHeight());
    EXPECT_EQ(font->ink("").width, 0);
    EXPECT_GT(font->width("Hello"), font->width("Hi"));
  }
}

// The DS's whole-number runs are the pieces, before they're scaled
TEST(FontTest, PixelRectanglesAreThePiecesUnscaled) {
  std::vector<FontRectangle> runs;
  kSilkscreen.pixelRectangles("Rock 12\n+ 7", runs);
  std::vector<TextPiece> pieces;
  kSilkscreen.layOut("Rock 12\n+ 7", 3, pieces);
  ASSERT_EQ(runs.size(), pieces.size());
  for (std::size_t i = 0; i < runs.size(); ++i) {
    EXPECT_FLOAT_EQ(pieces[i].x, 3.0f * runs[i].x);
    EXPECT_FLOAT_EQ(pieces[i].y, 3.0f * runs[i].y);
    EXPECT_FLOAT_EQ(pieces[i].width, 3.0f * runs[i].width);
  }
  kOrbitron.pixelRectangles("Rock", runs);
  EXPECT_TRUE(runs.empty());
}
