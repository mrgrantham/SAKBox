"""Writes the number chips' SVGs (number_2.svg ... number_12.svg, no 7).

Round 87 (the user's): a planet's number on a round chip, with dots for how
likely it is: one dot for each way two dice make it (a 2 or 12: one; a 6 or
8: five). The digits are drawn as segments (rectangles), in the Tron look,
so the texture recipes need no font. Run it again after changing a size or
colour here:

    python3 projects/cosmic_hex/art/make_number_chips.py
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
CHIP = "#f2ead8"   # the chip
EDGE = "#3a3a3a"   # its rim
INK = "#1e1e24"    # digits and dots

# Each digit's segments: a, b, c, d, e, f, g (top, top right, bottom right,
# bottom, bottom left, top left, middle), as in a seven-segment display
SEGMENTS = {
    0: "abcdef", 1: "bc", 2: "abdeg", 3: "abcdg", 4: "bcfg",
    5: "acdfg", 6: "acdefg", 7: "abc", 8: "abcdefg", 9: "abcdfg",
}


def digit(x, y, w, h, t, value):
    """Rectangles for one digit, its top left at (x, y), w wide, h tall, t thick."""
    half = h / 2
    boxes = {
        "a": (x, y, w, t),
        "b": (x + w - t, y, t, half),
        "c": (x + w - t, y + half, t, half),
        "d": (x, y + h - t, w, t),
        "e": (x, y + half, t, half),
        "f": (x, y, t, half),
        "g": (x, y + half - t / 2, w, t),
    }
    return "".join(
        f'<rect x="{bx:.1f}" y="{by:.1f}" width="{bw:.1f}" height="{bh:.1f}" fill="{INK}"/>'
        for s, (bx, by, bw, bh) in boxes.items() if s in SEGMENTS[value])


def chip(number):
    ways = 6 - abs(7 - number)
    parts = [f'<circle cx="50" cy="50" r="46" fill="{CHIP}" stroke="{EDGE}" stroke-width="4"/>']
    digits = [int(c) for c in str(number)]
    w, h, t, gap = 20, 36, 6, 6
    # (a 1 is one bar, as narrow as a segment is thick, so 10 to 12 stay compact)
    widths = [t if d == 1 else w for d in digits]
    x = 50 - (sum(widths) + (len(digits) - 1) * gap) / 2
    for d, width in zip(digits, widths):
        parts.append(digit(x - (w - t), 20, w, h, t, d) if d == 1 else digit(x, 20, w, h, t, d))
        x += width + gap
    # The dots, a row under the number
    spacing = 9
    first = 50 - (ways - 1) * spacing / 2
    for i in range(ways):
        parts.append(f'<circle cx="{first + i * spacing:.1f}" cy="70" r="3.4" fill="{INK}"/>')
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">'
            + "".join(parts) + "</svg>\n")


if __name__ == "__main__":
    for n in [2, 3, 4, 5, 6, 8, 9, 10, 11, 12]:
        with open(os.path.join(HERE, f"number_{n}.svg"), "w") as f:
            f.write(chip(n))
