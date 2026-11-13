"""The default pictures of a console app, so each can be told apart:
- a 3DS app's HOME Menu banner: a 256x128 PNG, a blue gradient with the
  app's title, description and author on it (on the top screen when it's
  selected);
- its icon: the title's first two letters, in a colour of the app's own,
  on dark (a 3DS app's, 48x48, on the HOME Menu; a DS app's, 32x32, in a
  flash card's menu).

The letters are stb_easy_font's (the font the desktop overlay draws with):
each is a few horizontal and vertical strokes, read from the tables in
stb_easy_font.h and drawn as filled rectangles, scaled up.
"""

import colorsys
import re
import struct
import zlib

WIDTH, HEIGHT = 256, 128  # the banner's
MARGIN = 10
LINE_GAP = 4
# stb_easy_font's letters are 7 points tall, set in 12-point lines (a
# lowercase letter that descends, such as g, is drawn 1 point lower)
LETTER_HEIGHT = 9

TITLE_COLOUR = (255, 255, 255)
DESCRIPTION_COLOUR = (215, 230, 255)
AUTHOR_COLOUR = (150, 180, 225)


class EasyFont:
    """stb_easy_font's letters, from its header."""

    def __init__(self, header_text):
        def table(name):
            body = re.search(name + r"\[\d+\]\s*=\s*\{(.*?)\};", header_text, re.S).group(1)
            return [int(number) for number in re.findall(r"\d+", body)]

        numbers = table("stb_easy_font_charinfo")
        # (advance, first horizontal stroke, first vertical stroke), for
        # the characters from space (32) on
        self.info = [tuple(numbers[i:i + 3]) for i in range(0, len(numbers), 3)]
        self.horizontal = table("stb_easy_font_hseg")
        self.vertical = table("stb_easy_font_vseg")

    def _index(self, character):
        index = ord(character) - 32
        return index if 0 <= index < len(self.info) - 1 else ord("?") - 32

    def width(self, text):
        """The text's width in points, as stb_easy_font_width measures one line."""
        return sum(self.info[self._index(c)][0] & 15 for c in text)

    def rectangles(self, text):
        """The strokes of a line of text, as (x, y, width, height) in points."""
        x = 0
        for character in text:
            index = self._index(character)
            advance, h_first, v_first = self.info[index]
            _, h_next, v_next = self.info[index + 1]
            y = 1 if advance & 16 else 0
            for strokes, vertical in ((self.horizontal[h_first:h_next], False),
                                      (self.vertical[v_first:v_next], True)):
                # (as stb_easy_font_draw_segs: each byte is a stroke's length
                # in its low 3 bits, a step right in bit 3, and its row above)
                stroke_x = x
                for stroke in strokes:
                    length = stroke & 7
                    stroke_x += (stroke >> 3) & 1
                    if length:
                        top = y + (stroke >> 4)
                        yield ((stroke_x, top, 1, length) if vertical else
                               (stroke_x, top, length, 1))
            x += advance & 15


def wrap(font, text, scale, width):
    """The text broken into lines no wider than `width` pixels at `scale`,
    at spaces (a word wider than that is left whole)."""
    lines = []
    for word in text.split():
        candidate = f"{lines[-1]} {word}" if lines else word
        if lines and font.width(candidate) * scale <= width:
            lines[-1] = candidate
        else:
            lines.append(word)
    return lines


def layout(font, title, description, author):
    """Where each line goes: (text, scale, colour), top to bottom. The
    largest letters that fit: the title at 3, 2 or 1 times, on two lines at
    most when it's bigger than 1, the description at 2 or 1."""
    room = WIDTH - 2 * MARGIN
    candidates = []
    for title_scale, description_scale in ((3, 2), (2, 2), (3, 1), (2, 1), (1, 1)):
        title_lines = wrap(font, title, title_scale, room)
        if title_scale > 1 and len(title_lines) > 2:
            continue
        lines = [(line, title_scale, TITLE_COLOUR) for line in title_lines]
        lines += [(line, description_scale, DESCRIPTION_COLOUR)
                  for line in wrap(font, description, description_scale, room)]
        if author:
            lines.append((author, 1, AUTHOR_COLOUR))
        candidates.append(lines)
        if height(lines) <= HEIGHT - 2 * MARGIN:
            return lines
    return candidates[-1]


def height(lines):
    """The lines' height in pixels, with the gaps between them."""
    return sum(LETTER_HEIGHT * scale + LINE_GAP for _, scale, _ in lines) - LINE_GAP


def banner(font, title, description="", author=""):
    """The banner's pixels: rows of (r, g, b), the lines centred."""
    pixels = [[(20 + y // 4, 60 + y // 2, 140 + y // 2)] * WIDTH for y in range(HEIGHT)]
    lines = layout(font, title, description, author)
    top = max(MARGIN, (HEIGHT - height(lines)) // 2)
    for text, scale, colour in lines:
        left = max(MARGIN, (WIDTH - font.width(text) * scale) // 2)
        for x, y, w, h in font.rectangles(text):
            x0, x1 = max(0, left + x * scale), min(WIDTH, left + (x + w) * scale)
            for row in range(top + y * scale, min(HEIGHT, top + (y + h) * scale)):
                pixels[row][x0:x1] = [colour] * max(0, x1 - x0)
        top += LETTER_HEIGHT * scale + LINE_GAP
    return pixels


def icon_letters(title):
    """An icon's default letters: the title's first two letters or digits
    ("Models from Blender": "Mo")."""
    return "".join(c for c in title if c.isalnum())[:2]


def _colour(hue, saturation, value):
    return tuple(round(channel * 255) for channel in colorsys.hsv_to_rgb(hue, saturation, value))


def icon(font, letters, size, title):
    """An icon's pixels, size x size: `letters` as large as fit, and a
    border, in a bright colour picked from `title` (so apps with the same
    letters still differ), on a dark shade of it. Two colours, so a DS
    icon (16 at most) can have them too."""
    hue = (zlib.crc32(title.encode()) % 360) / 360
    bright, dark = _colour(hue, 0.75, 1.0), _colour(hue, 0.6, 0.16)
    border = max(1, size // 24)
    pixels = [[bright if min(x, y, size - 1 - x, size - 1 - y) < border else dark
               for x in range(size)] for y in range(size)]
    room = size - 6 * border
    width = max(1, font.width(letters))
    scale = max(1, min(room // width, room // LETTER_HEIGHT))
    left = (size - width * scale) // 2
    # (centred on the capitals' 7 points, below which only descenders go)
    top = (size - 7 * scale) // 2
    for x, y, w, h in font.rectangles(letters):
        x0, x1 = max(0, left + x * scale), min(size, left + (x + w) * scale)
        for row in range(max(0, top + y * scale), min(size, top + (y + h) * scale)):
            pixels[row][x0:x1] = [bright] * max(0, x1 - x0)
    return pixels


def write_png(pixels, path):
    """Writes rows of (r, g, b) as an 8-bit RGB PNG."""
    rows = b"".join(b"\x00" + bytes(channel for pixel in row for channel in pixel)
                    for row in pixels)

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", len(pixels[0]), len(pixels), 8, 2, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
                     chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def write_bmp(pixels, path):
    """Writes rows of (r, g, b) as an 8-bit BMP with a 16-colour palette,
    the kind devkitPro's ndstool takes for a DS icon (its default icon is
    one). Palette entry 0 is unused: on a DS it's transparent."""
    palette = [(0, 0, 0)]
    for row in pixels:
        for pixel in row:
            if pixel not in palette:
                palette.append(pixel)
    if len(palette) > 16:
        raise ValueError(f"{len(palette) - 1} colours: a DS icon has 15 at most")
    palette += [(0, 0, 0)] * (16 - len(palette))
    width, height = len(pixels[0]), len(pixels)
    stride = (width + 3) // 4 * 4
    # (rows bottom up, each padded to 4 bytes; the palette's entries b, g, r, 0)
    rows = b"".join(bytes(palette.index(pixel) for pixel in row) + bytes(stride - width)
                    for row in reversed(pixels))
    colours = b"".join(bytes((b, g, r, 0)) for r, g, b in palette)
    offset = 14 + 40 + len(colours)
    path.write_bytes(b"BM" + struct.pack("<IHHI", offset + len(rows), 0, 0, offset) +
                     struct.pack("<IiiHHIIiiII", 40, width, height, 1, 8, 0, len(rows),
                                 2835, 2835, 16, 0) +
                     colours + rows)


def makefile_settings(makefile_text,
                      keys=("APP_TITLE", "APP_DESCRIPTION", "APP_AUTHOR")):
    """Settings from a devkitPro Makefile: by default a 3DS app's
    APP_TITLE, APP_DESCRIPTION and APP_AUTHOR (a DS app's is GAME_TITLE)."""
    settings = {}
    for key in keys:
        found = re.search(rf"^{key}\s*:?=\s*(.*?)\s*$", makefile_text, re.M)
        if found:
            settings[key] = found.group(1)
    return settings
