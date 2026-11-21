"""Writes the overlay's fonts as C++ data (src/SilkscreenFont.cpp and
src/OrbitronFont.cpp), from the fonts in third_party/fonts:

- Silkscreen, a pixel font, at its own 8 pixels: each letter's lit pixels as
  runs along its rows, which the DS fills into its text layer and a Canvas
  (no texture);
- Orbitron, a smooth font, baked at the sizes the 3DS's screens use into one
  alpha atlas, drawn as textured quads (the 3DS, desktops) or blended into a
  Canvas.

The build needs neither Python nor PIL: the output is committed. Run it
again after changing a size here (it needs Pillow):

    python3 projects/scratchpadd/scene/overlay_text/make_fonts.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
SILKSCREEN = os.path.join(ROOT, "third_party", "fonts", "silkscreen", "Silkscreen-Regular.ttf")
ORBITRON = os.path.join(ROOT, "third_party", "fonts", "orbitron", "Orbitron[wght].ttf")

CHARACTERS = [chr(c) for c in range(ord(" "), ord("~") + 1)]  # (printable ASCII)

# Silkscreen's pixels are whole at 8 pixels to the em
SILKSCREEN_SIZE = 8
# Space between lines, below the tallest letters ('|', brackets)
SILKSCREEN_LINE_GAP = 2

# Orbitron at scale 1: 9 pixels to the em (capitals 7 tall and lines as wide
# as stb_easy_font's were), its capitals' tops a pixel below the line's top,
# lines 12 apart (as stb_easy_font's)
ORBITRON_EM = 9.0
ORBITRON_BASELINE = 8.0
ORBITRON_LINE_HEIGHT = 12.0
# SemiBold: the regular weight's strokes thin out at 9 pixels
ORBITRON_WEIGHT = 600
# The overlay scales it's baked for: 1 (desktops; the 3DS's bottom screen),
# the 3DS's bottom screen's 1.25 (320 / 256) and top screen's 1.5625
# (400 / 256), and bigger words (titles, numbers). Other scales draw the
# nearest, scaled.
ORBITRON_SCALES = [1.0, 1.25, 1.5625, 2.0, 2.5, 3.125]
ATLAS_WIDTH = 512
SOLID = 2  # (the atlas's top-left 2 x 2: solid, for the overlay's shapes)


def silkscreen():
    """Each letter's advance and its lit pixels' runs (x, y, length), y from the line's top.

    The line's top is a row above the capitals (as stb_easy_font's letters
    sat); the grave accent, the one letter higher, is moved down to it."""
    font = ImageFont.truetype(SILKSCREEN, SILKSCREEN_SIZE)
    letters = []
    for ch in CHARACTERS:
        image = Image.new("L", (16, 16), 0)
        draw = ImageDraw.Draw(image)
        draw.fontmode = "1"  # (no smoothing: the font's own pixels)
        draw.text((0, 0), ch, font=font, fill=255)
        lit = {(x, y) for y in range(16) for x in range(16) if image.getpixel((x, y)) > 127}
        advance = font.getlength(ch)
        assert advance == int(advance), ch
        letters.append((ch, int(advance), lit))
    top = min(y for ch, _, lit in letters if ch == "H" for _, y in lit) - 1
    bottom = max(y for _, _, lit in letters for _, y in lit)
    glyphs = []
    for ch, advance, lit in letters:
        if lit and min(y for _, y in lit) < top:
            drop = top - min(y for _, y in lit)
            lit = {(x, y + drop) for x, y in lit}
        runs = []
        for y in sorted({y for _, y in lit}):
            xs = sorted(x for x, yy in lit if yy == y)
            start = previous = xs[0]
            for x in xs[1:] + [None]:
                if x is not None and x == previous + 1:
                    previous = x
                    continue
                runs.append((start, y - top, previous - start + 1))
                if x is not None:
                    start = previous = x
        glyphs.append((ch, advance, runs))
    return {"line_height": bottom - top + 1 + SILKSCREEN_LINE_GAP, "glyphs": glyphs}


def orbitron_font(size):
    font = ImageFont.truetype(ORBITRON, size)
    font.set_variation_by_axes([ORBITRON_WEIGHT])
    return font


def orbitron_picture(font, ch, baseline):
    """A letter's smoothed picture, cut to its ink: (alpha rows, left from the pen, top from the line's top)."""
    pad = 8
    width = int(font.getlength(ch)) + 2 * pad + 8
    height = int(font.size * 2) + 2 * pad
    image = Image.new("L", (width, height), 0)
    origin_y = pad + int(round(font.size * 1.2))
    ImageDraw.Draw(image).text((pad, origin_y), ch, font=font, fill=255, anchor="ls")
    box = image.getbbox()
    if box is None:
        return None
    x0, y0, x1, y1 = box
    rows = [[image.getpixel((x, y)) for x in range(x0, x1)] for y in range(y0, y1)]
    return rows, x0 - pad, y0 - origin_y + baseline


def orbitron():
    """The advances (points at scale 1), and each scale's letters packed into one atlas."""
    precise = orbitron_font(ORBITRON_EM * 64)
    advances = [precise.getlength(ch) / 64 for ch in CHARACTERS]
    pictures = []  # (scale index, character index, rows, left, top)
    for s, scale in enumerate(ORBITRON_SCALES):
        font = orbitron_font(ORBITRON_EM * scale)
        baseline = int(round(ORBITRON_BASELINE * scale))
        for c, ch in enumerate(CHARACTERS):
            made = orbitron_picture(font, ch, baseline)
            if made:
                pictures.append((s, c) + made)
    placed, height = pack(pictures)
    atlas = [[0] * ATLAS_WIDTH for _ in range(height)]
    for y in range(SOLID):
        for x in range(SOLID):
            atlas[y][x] = 255
    sizes = [[(0, 0, 0, 0, 0, 0)] * len(CHARACTERS) for _ in ORBITRON_SCALES]
    for (s, c, rows, left, top), (ax, ay) in zip(pictures, placed):
        for y, row in enumerate(rows):
            atlas[ay + y][ax:ax + len(row)] = row
        sizes[s][c] = (ax, ay, len(rows[0]), len(rows), left, top)
    return {"advances": advances, "sizes": sizes, "atlas": atlas}


def pack(pictures):
    """Places the pictures in rows (tallest first), a clear pixel round each; returns where, and the atlas's height (a power of two)."""
    order = sorted(range(len(pictures)), key=lambda i: -len(pictures[i][2]))
    where = [None] * len(pictures)
    x, y, row_height = SOLID + 1, 0, SOLID
    for i in order:
        rows = pictures[i][2]
        w, h = len(rows[0]), len(rows)
        if x + w + 1 > ATLAS_WIDTH:
            x, y, row_height = 0, y + row_height + 1, 0
        where[i] = (x, y)
        x += w + 1
        row_height = max(row_height, h)
    used = y + row_height
    height = 8
    while height < used:
        height *= 2
    return where, height


HEADER = """// Written by make_fonts.py from {source} (SIL Open Font
// License, OFL.txt beside it): don't edit, run it again.
#include <ScratchPadd/Scene/Font.hpp>

#include <cstdint>

namespace ScratchPadd::Scene {{

namespace {{
// clang-format off
"""

FOOTER = """// clang-format on
}} // namespace

const Font &Font::{name}() {{
  static constexpr Font kFont({data});
  return kFont;
}}

}} // namespace ScratchPadd::Scene
"""


def lines_of(items, per_line):
    return "\n".join("    " + ", ".join(items[i:i + per_line]) + ","
                     for i in range(0, len(items), per_line))


def write_silkscreen(font):
    runs, glyphs = [], []
    for ch, advance, glyph_runs in font["glyphs"]:
        glyphs.append(f"{{{len(runs)}, {len(glyph_runs)}, {advance}}}")
        runs.extend(f"{{{x}, {y}, {n}}}" for x, y, n in glyph_runs)
    text = HEADER.format(source="third_party/fonts/silkscreen/Silkscreen-Regular.ttf")
    text += "// Each letter's first run, how many, and its advance (' ' to '~')\n"
    text += "constexpr PixelGlyph kGlyphs[] = {\n" + lines_of(glyphs, 8) + "\n};\n"
    text += "// The letters' lit pixels: x, y and length along a row\n"
    text += "constexpr PixelRun kRuns[] = {\n" + lines_of(runs, 8) + "\n};\n"
    text += f"constexpr PixelFontData kData{{{font['line_height']}, kGlyphs, kRuns}};\n"
    text += FOOTER.format(name="silkscreen", data="kData")
    with open(os.path.join(HERE, "src", "SilkscreenFont.cpp"), "w") as f:
        f.write(text)
    return len(glyphs) * 4 + len(runs) * 3


def write_orbitron(font):
    atlas = font["atlas"]
    text = HEADER.format(source="third_party/fonts/orbitron/Orbitron[wght].ttf")
    text += f"// Each letter's advance, points at scale 1 (' ' to '~')\n"
    text += "constexpr float kAdvances[] = {\n" + lines_of(
        [f"{a:.4f}f" for a in font["advances"]], 8) + "\n};\n"
    text += "// Each size's letters: x, y, width, height in the atlas; left from the pen\n"
    text += "// and top from the line's top, in its pixels\n"
    names = []
    for s, glyphs in enumerate(font["sizes"]):
        name = f"kSize{s}"
        names.append(f"{{{ORBITRON_SCALES[s]}f, {name}}}")
        text += f"constexpr AtlasGlyph {name}[] = {{\n" + lines_of(
            ["{" + ", ".join(str(v) for v in g) + "}" for g in glyphs], 4) + "\n};\n"
    text += "constexpr BakedSize kSizes[] = {" + ", ".join(names) + "};\n"
    text += f"// The atlas, {ATLAS_WIDTH} x {len(atlas)}: alpha, rows from the top\n"
    flat = [str(v) for row in atlas for v in row]
    text += "constexpr std::uint8_t kAtlas[] = {\n" + lines_of(flat, 24) + "\n};\n"
    text += (f"constexpr SmoothFontData kData{{{ORBITRON_LINE_HEIGHT}f, kAdvances, kSizes, "
             f"{ATLAS_WIDTH}, {len(atlas)}, kAtlas}};\n")
    text += FOOTER.format(name="orbitron", data="kData")
    with open(os.path.join(HERE, "src", "OrbitronFont.cpp"), "w") as f:
        f.write(text)
    used = sum(g[2] * g[3] for size in font["sizes"] for g in size)
    return ATLAS_WIDTH * len(atlas), used


def main():
    silkscreen_bytes = write_silkscreen(silkscreen())
    atlas_bytes, used = write_orbitron(orbitron())
    print(f"Silkscreen: {silkscreen_bytes} bytes of letters")
    print(f"Orbitron: a {atlas_bytes}-byte atlas ({used} bytes of letters)")


if __name__ == "__main__":
    main()
