"""Turns an SVG drawing into a texture recipe: the shapes as filled outlines,
flattened to straight lines, with their colours, in a short text format that
a device turns into pixels at whatever size and colour depth its profile
allows (ScratchPadd/TextureRecipe). The parsing happens here, at build time,
so no device needs an SVG parser.

Usage: svg_to_recipe.py IN.svg OUT.recipe

What it reads (enough for flat, drawn textures; anything else is an error,
so a drawing never quietly loses a part):
  <svg> with a viewBox (or width and height)
  <g>, <rect>, <circle>, <ellipse>, <line>, <polyline>, <polygon>, <path>
  path commands M L H V C S Q T Z, absolute and relative (not arcs, A)
  fill, stroke, stroke-width, opacity, fill-opacity, stroke-opacity, fill-rule,
  as attributes or in style=""; colours #rgb, #rrggbb, rgb(r,g,b) and a few names
  transform: translate, scale, rotate, matrix (nested in groups)
Strokes become filled outlines here (each segment a band, round joins and
caps), so a device only ever fills.

The recipe (one item a line; coordinates in 1/16ths of the drawing's units,
as integers, so a DS draws it without floating point):
  recipe 1
  size W H                 the drawing's size, in its units
  colour I R G B A         colour I (0 to 255 each)
  fill I nonzero|evenodd   a shape in colour I, with its fill rule
  contour X Y X Y ...      one closed outline of the shape above (several make holes)
"""

import math
import re
import sys
import xml.etree.ElementTree as ET

SCALE = 16  # recipe units per drawing unit
CURVE_STEPS = 16  # straight lines a curve becomes
ROUND_STEPS = 16  # sides of a round join or cap

NAMED = {
    "black": (0, 0, 0), "white": (255, 255, 255), "red": (255, 0, 0), "green": (0, 128, 0),
    "lime": (0, 255, 0), "blue": (0, 0, 255), "yellow": (255, 255, 0), "cyan": (0, 255, 255),
    "magenta": (255, 0, 255), "orange": (255, 165, 0), "purple": (128, 0, 128), "gray": (128, 128, 128),
    "grey": (128, 128, 128), "navy": (0, 0, 128), "teal": (0, 128, 128), "maroon": (128, 0, 0),
}


class RecipeError(Exception):
    pass


def tag(element):
    return element.tag.split("}")[-1]


def parse_colour(text):
    text = text.strip().lower()
    if text in ("none", "transparent"):
        return None
    if text.startswith("#"):
        h = text[1:]
        if len(h) == 3:
            return tuple(int(c * 2, 16) for c in h)
        if len(h) == 6:
            return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
    m = re.fullmatch(r"rgb\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)", text)
    if m:
        return tuple(min(255, int(v)) for v in m.groups())
    if text in NAMED:
        return NAMED[text]
    raise RecipeError(f"a colour I can't read: {text!r}")


# --- Transforms: (a, b, c, d, e, f) as SVG's matrix

IDENTITY = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)


def multiply(m, n):
    a, b, c, d, e, f = m
    a2, b2, c2, d2, e2, f2 = n
    return (a * a2 + c * b2, b * a2 + d * b2, a * c2 + c * d2, b * c2 + d * d2,
            a * e2 + c * f2 + e, b * e2 + d * f2 + f)


def parse_transform(text):
    m = IDENTITY
    for name, args in re.findall(r"(\w+)\s*\(([^)]*)\)", text or ""):
        v = [float(x) for x in re.split(r"[\s,]+", args.strip()) if x]
        if name == "translate":
            t = (1, 0, 0, 1, v[0], v[1] if len(v) > 1 else 0)
        elif name == "scale":
            t = (v[0], 0, 0, v[1] if len(v) > 1 else v[0], 0, 0)
        elif name == "rotate":
            r = math.radians(v[0])
            t = (math.cos(r), math.sin(r), -math.sin(r), math.cos(r), 0, 0)
            if len(v) == 3:
                t = multiply(multiply((1, 0, 0, 1, v[1], v[2]), t), (1, 0, 0, 1, -v[1], -v[2]))
        elif name == "matrix":
            t = tuple(v)
        else:
            raise RecipeError(f"a transform I can't do: {name}")
        m = multiply(m, t)
    return m


def apply(m, point):
    a, b, c, d, e, f = m
    x, y = point
    return (a * x + c * y + e, b * x + d * y + f)


def scale_of(m):
    a, b, c, d, _, _ = m
    return math.sqrt(abs(a * d - b * c))


# --- Shapes as lists of points (closed or open)

def ellipse_points(cx, cy, rx, ry):
    return [(cx + rx * math.cos(2 * math.pi * i / 64), cy + ry * math.sin(2 * math.pi * i / 64)) for i in range(64)]


def path_subpaths(d):
    """A path's subpaths: (points, closed)"""
    tokens = re.findall(r"[MmLlHhVvCcSsQqTtZzAa]|[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?", d)
    out, points, closed = [], [], False
    x = y = 0.0
    start = (0.0, 0.0)
    last_control = None
    command = None
    i = 0

    def number():
        nonlocal i
        value = float(tokens[i])
        i += 1
        return value

    def finish():
        nonlocal points, closed
        if len(points) > 1:
            out.append((points, closed))
        points, closed = [], False

    while i < len(tokens):
        if re.fullmatch(r"[A-Za-z]", tokens[i]):
            command = tokens[i]
            i += 1
        if command is None:
            raise RecipeError("a path that doesn't start with a command")
        relative = command.islower()
        c = command.upper()
        if c == "A":
            raise RecipeError("arcs (A) aren't supported: draw them as curves")
        if c == "Z":
            closed = True
            x, y = start
            finish()
            last_control = None
            command = None
            continue
        if c == "M":
            finish()
            nx, ny = number(), number()
            x, y = (x + nx, y + ny) if relative else (nx, ny)
            start = (x, y)
            points = [(x, y)]
            command = "l" if relative else "L"  # further pairs are lines
            last_control = None
            continue
        if c in "LHV":
            if c == "L":
                nx, ny = number(), number()
                x, y = (x + nx, y + ny) if relative else (nx, ny)
            elif c == "H":
                nx = number()
                x = x + nx if relative else nx
            else:
                ny = number()
                y = y + ny if relative else ny
            points.append((x, y))
            last_control = None
            continue
        if c in "CS":
            if c == "C":
                x1, y1 = number(), number()
                if relative:
                    x1, y1 = x + x1, y + y1
            else:
                x1, y1 = (2 * x - last_control[0], 2 * y - last_control[1]) if last_control else (x, y)
            x2, y2, ex, ey = number(), number(), number(), number()
            if relative:
                x2, y2, ex, ey = x + x2, y + y2, x + ex, y + ey
            for s in range(1, CURVE_STEPS + 1):
                t = s / CURVE_STEPS
                u = 1 - t
                points.append((u ** 3 * x + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t ** 3 * ex,
                               u ** 3 * y + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t ** 3 * ey))
            last_control = (x2, y2)
            x, y = ex, ey
            continue
        if c in "QT":
            if c == "Q":
                x1, y1 = number(), number()
                if relative:
                    x1, y1 = x + x1, y + y1
            else:
                x1, y1 = (2 * x - last_control[0], 2 * y - last_control[1]) if last_control else (x, y)
            ex, ey = number(), number()
            if relative:
                ex, ey = x + ex, y + ey
            for s in range(1, CURVE_STEPS + 1):
                t = s / CURVE_STEPS
                u = 1 - t
                points.append((u * u * x + 2 * u * t * x1 + t * t * ex, u * u * y + 2 * u * t * y1 + t * t * ey))
            last_control = (x1, y1)
            x, y = ex, ey
            continue
        raise RecipeError(f"a path command I can't read: {command}")
    finish()
    return out


# --- Strokes as filled outlines

def stroke_outlines(points, closed, width):
    """A stroked line as filled pieces (filled together with nonzero): a band
    for each segment, a round join at each point, round caps"""
    half = width / 2
    pieces = []
    segments = list(zip(points, points[1:] + ([points[0]] if closed else [])))
    for (x1, y1), (x2, y2) in segments:
        dx, dy = x2 - x1, y2 - y1
        length = math.hypot(dx, dy)
        if length == 0:
            continue
        nx, ny = -dy / length * half, dx / length * half
        pieces.append([(x1 + nx, y1 + ny), (x2 + nx, y2 + ny), (x2 - nx, y2 - ny), (x1 - nx, y1 - ny)])
    for x, y in points:
        pieces.append([(x + half * math.cos(2 * math.pi * i / ROUND_STEPS),
                        y + half * math.sin(2 * math.pi * i / ROUND_STEPS)) for i in range(ROUND_STEPS)])
    return pieces


# --- Reading the drawing

def style_of(element, inherited):
    style = dict(inherited)
    for key in ("fill", "stroke", "stroke-width", "opacity", "fill-opacity", "stroke-opacity", "fill-rule"):
        if key in element.attrib:
            style[key] = element.attrib[key]
    for part in element.attrib.get("style", "").split(";"):
        if ":" in part:
            key, value = part.split(":", 1)
            style[key.strip()] = value.strip()
    # opacity multiplies down the tree
    style["opacity"] = str(float(inherited.get("opacity", "1")) * float(element.attrib.get("opacity", "1")))
    return style


def shapes_of(element, transform, style):
    """(subpaths, closed-ness) for one drawing element, in its own units"""
    t = tag(element)
    a = element.attrib
    f = lambda k, d=0.0: float(a.get(k, d))
    if t == "rect":
        x, y, w, h = f("x"), f("y"), f("width"), f("height")
        return [([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], True)]
    if t == "circle":
        return [(ellipse_points(f("cx"), f("cy"), f("r"), f("r")), True)]
    if t == "ellipse":
        return [(ellipse_points(f("cx"), f("cy"), f("rx"), f("ry")), True)]
    if t == "line":
        return [([(f("x1"), f("y1")), (f("x2"), f("y2"))], False)]
    if t in ("polyline", "polygon"):
        v = [float(n) for n in re.split(r"[\s,]+", a.get("points", "").strip()) if n]
        return [(list(zip(v[0::2], v[1::2])), t == "polygon")]
    if t == "path":
        return path_subpaths(a.get("d", ""))
    raise RecipeError(f"an element I can't draw: <{t}>")


def colour_with_opacity(text, opacity):
    rgb = parse_colour(text)
    if rgb is None:
        return None
    return rgb + (round(255 * max(0.0, min(1.0, opacity))),)


def walk(element, transform, style, out):
    t = tag(element)
    if t in ("title", "desc", "metadata", "defs"):
        return
    transform = multiply(transform, parse_transform(element.attrib.get("transform")))
    style = style_of(element, style)
    if t in ("svg", "g"):
        for child in element:
            walk(child, transform, style, out)
        return
    subpaths = shapes_of(element, transform, style)
    opacity = float(style.get("opacity", "1"))
    fill = colour_with_opacity(style.get("fill", "black"), opacity * float(style.get("fill-opacity", "1")))
    if fill and any(closed or len(p) > 2 for p, closed in subpaths):
        rule = "evenodd" if style.get("fill-rule") == "evenodd" else "nonzero"
        out.append((fill, rule, [[apply(transform, p) for p in points] for points, _ in subpaths]))
    stroke = colour_with_opacity(style.get("stroke", "none"), opacity * float(style.get("stroke-opacity", "1")))
    if stroke:
        width = float(style.get("stroke-width", "1"))
        pieces = []
        for points, closed in subpaths:
            moved = [apply(transform, p) for p in points]
            pieces += stroke_outlines(moved, closed, width * scale_of(transform))
        out.append((stroke, "nonzero", pieces))


def convert(svg_text):
    root = ET.fromstring(svg_text)
    if tag(root) != "svg":
        raise RecipeError("not an SVG drawing")
    view = root.attrib.get("viewBox")
    if view:
        vx, vy, vw, vh = (float(n) for n in re.split(r"[\s,]+", view.strip()))
    else:
        vx, vy = 0.0, 0.0
        vw = float(re.sub(r"[a-z]+$", "", root.attrib.get("width", "0")))
        vh = float(re.sub(r"[a-z]+$", "", root.attrib.get("height", "0")))
    if vw <= 0 or vh <= 0:
        raise RecipeError("the drawing has no size (a viewBox, or width and height)")
    shapes = []
    walk(root, (1, 0, 0, 1, -vx, -vy), {}, shapes)
    colours = []
    lines = ["recipe 1", f"size {round(vw)} {round(vh)}"]
    body = []
    for colour, rule, contours in shapes:
        if colour not in colours:
            colours.append(colour)
        body.append(f"fill {colours.index(colour)} {rule}")
        for contour in contours:
            if len(contour) >= 3:
                body.append("contour " + " ".join(f"{round(x * SCALE)} {round(y * SCALE)}" for x, y in contour))
    lines += [f"colour {i} {r} {g} {b} {a}" for i, (r, g, b, a) in enumerate(colours)]
    return "\n".join(lines + body) + "\n"


def main(argv):
    if len(argv) != 3:
        print(__doc__.split("\n\n")[1], file=sys.stderr)
        return 2
    try:
        with open(argv[1]) as f:
            recipe = convert(f.read())
    except (RecipeError, ET.ParseError, ValueError) as error:
        print(f"{argv[1]}: {error}", file=sys.stderr)
        return 1
    with open(argv[2], "w") as f:
        f.write(recipe)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
