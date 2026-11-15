# Texture recipes

Textures drawn on the device, when an app starts, from SVG drawings. The build
turns each drawing into a **recipe**: its shapes as filled outlines, with their
colours and fill rules. The app draws the recipe into pixels at whatever size
and colour depth suits the device (a DS's 16-colour palette, a desktop's RGBA),
so no image files ship and no device needs an SVG parser. Drawing uses integers
only (a DS has no floating point), so a recipe draws the same pixels on every
platform.

## Use

In a `BUILD` file:

```python
load("//projects/scratchpadd/texture_recipe:texture_recipe.bzl", "texture_recipe")

texture_recipe(
    name = "tile_faces",
    svg = "art/tile_faces.svg",
    namespace = "Art",
    symbol = "kTileFaces",
)
```

That's a `cc_library` holding the recipe's text. Depend on it and on
`//projects/scratchpadd/texture_recipe`, then:

```cpp
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

namespace Art {
extern const unsigned char kTileFaces[];
extern const std::size_t kTileFacesSize;
}

namespace TR = ScratchPadd::TextureRecipe;
const auto recipe = TR::read(std::string_view(reinterpret_cast<const char *>(Art::kTileFaces), Art::kTileFacesSize));
if (!recipe)
  log->error("tile faces: {}", recipe.error());
const TR::Image image = TR::draw(*recipe, 64, 64, TR::Format::Rgba8, "tile faces");
```

Shapes are painted in order, each over those before, and the drawing is
stretched to the size asked for. The name is only for the log.

## Formats

| Format | Pixels | Colours | Edges |
| --- | --- | --- | --- |
| `Rgba8` | 4 bytes each, rows top to bottom | any | smoothed: 4 × 4 samples a pixel |
| `Palette256` | 1 byte each, an index into `image.palette` (0 is transparent) | up to 255 | sharp: the pixel's centre decides |
| `Palette16` | the same; packed 2 a byte when it goes to a DS | up to 15 | sharp |

**Each drawing gets its own palette**, made from its colours. If they fit,
the palette is exactly the recipe's colours. If they don't:

1. Colours no pixel uses are dropped. That costs nothing.
2. Then the two colours whose merging changes the picture least are merged
   into their average, again and again, until they fit. "Least" is how far
   apart the colours are times how many pixels would change, so a small
   detail moves to a big area's colour, and not the other way round.

So one drawing works everywhere: in full on a desktop, and as close as fits
on a DS, with nothing changed in the drawing. What happened is in
`image.reduction` (colours used, colours kept, and the most any colour moved,
of 255), and it's **logged as an error**, between rows of `!`, naming the
texture:

```
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!! TEXTURE COLOURS REDUCED: tile faces uses 20 colours; Palette16 holds 15, so 5 were merged
!!! The worst colour moved by 1 (of 255) in one channel. Simplify the drawing's colours, or draw it as Palette256 or Rgba8, to keep them all.
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
```

The build warns too, when it converts a drawing with more than 15 or 255
colours (`WARNING: … has 20 colours: drawn as Palette16 …`), so you see it
before the app runs.

**What a palette costs on a DS.** A palette is tiny next to the pixels.
The DS keeps each colour in 2 bytes (5 bits each of red, green and blue), in
its texture palette memory (16 KB or more), apart from the pixels:

| A 64 × 64 texture | Pixels | Its palette | Palettes that fit in 16 KB |
| --- | --- | --- | --- |
| DS direct colour (any colours, no partial transparency) | 8 KB | none | |
| `Palette256` | 4 KB | 512 bytes | 32 |
| `Palette16` | 2 KB | 32 bytes | 512 |

So a palette of its own for each drawing costs little, and a shared palette
would save almost nothing while forcing every drawing into the same colours.
The real choice is the pixels' size: `Palette16` halves `Palette256`'s. The
[device profile](../device_profile/README.md) says which a device uses. Also,
the DS's 5 bits a channel means colours closer than 8 apart look the same
there anyway.

## Drawing for it

The converter (`tools/svg_to_recipe.py`) reads:

- `rect`, `circle`, `ellipse`, `line`, `polyline`, `polygon` and `path`
  (`M L H V C S Q T Z`, absolute and relative; curves are flattened into 16
  straight pieces);
- `g` with `transform` (`translate`, `scale`, `rotate`, `matrix`), nested;
- `fill`, `stroke`, `stroke-width`, `opacity`, `fill-opacity`,
  `stroke-opacity` and `fill-rule`, as attributes or in `style`;
- colours in every CSS form: the 148 names (`tools/css_colours.py`, from
  CSS Color 4), `#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`, `rgb()`, `rgba()`,
  `hsl()` and `hsla()`, with numbers or percentages, commas or spaces.

Strokes become filled outlines (a band along each segment and a round piece
at each joint), so a stroke drawn over a fill keeps the drawing's look.
Anything else, such as arcs (`A`), text, gradients, images or a misspelt
colour, **fails the build** with what and where, rather than drawing
something different. Arcs aren't read yet: draw them as curves (`C`)
instead. The drawing needs a `viewBox`; its units become the recipe's, in
sixteenths.

## The recipe format

Plain text, one item a line:

```
recipe 1
size 64 64                 # the drawing's size, in its units
colour 0 11 26 46 255      # index, then red, green, blue, alpha (0-255)
fill 0 nonzero             # a shape in colour 0; nonzero or evenodd
contour 0 0 1024 0 1024 1024 0 1024   # an outline: x y pairs in 1/16 units
```

A fill's contours follow it; several make holes (with `evenodd`, or nonzero
with the hole drawn the other way round). A recipe can also be written by
hand or by a program, and `read` says which line is wrong when it can't read
one.

## Tests

| Test | Covers |
| --- | --- |
| `svg_to_recipe_test` | Shapes, paths and curves, nested transforms, the `viewBox` origin, every CSS colour form, colours shared and opacity kept, even-odd holes, strokes as filled outlines, `fill="none"`, the warning for more than 15 colours, and arcs, text, unknown colours and a missing `viewBox` refused |
| `TextureRecipe_test` | Reading, and errors naming the line; palette pixels exact; colours that fit kept exactly; 20 colours into `Palette16` merging the closest five pairs, and logged as an error; unused colours dropped first; merging moving a small detail's colour, not a big area's; RGBA edges half covered; both fill rules; later shapes over earlier ones; and the example drawing (`examples/tile.svg`, converted by the build) drawn with the expected colours where they belong, its pixels pinned by checksum so any platform drawing differently fails |
