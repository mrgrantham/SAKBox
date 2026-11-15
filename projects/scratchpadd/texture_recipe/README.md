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
const TR::Image image = TR::draw(*recipe, 64, 64, TR::Format::Rgba8); // or Format::Indexed
```

| Format | Pixels | Edges |
| --- | --- | --- |
| `Rgba8` | 4 bytes each, rows top to bottom | smoothed: 4 × 4 samples a pixel |
| `Indexed` | 1 byte each: 0 is transparent, then the recipe's colours in order (`image.palette` holds them) | sharp: the pixel's centre decides |

A recipe's colours are its palette, so a recipe meant for a DS's 16-colour
textures needs 15 colours or fewer. Shapes are painted in order, each over
those before, and the drawing is stretched to the size asked for.

## Drawing for it

The converter (`tools/svg_to_recipe.py`) reads:

- `rect`, `circle`, `ellipse`, `line`, `polyline`, `polygon` and `path`
  (`M L H V C S Q T Z`, absolute and relative; curves are flattened into 16
  straight pieces);
- `g` with `transform` (`translate`, `scale`, `rotate`, `matrix`), nested;
- `fill`, `stroke`, `stroke-width`, `opacity`, `fill-opacity`,
  `stroke-opacity` and `fill-rule`, as attributes or in `style`; colours as
  names, `#rgb`, `#rrggbb` or `rgb()`.

Strokes become filled outlines (a band along each segment and a round piece
at each joint), so a stroke drawn over a fill keeps the drawing's look.
Anything else, such as arcs (`A`), text, gradients, images or a colour name it
doesn't know, **fails the build** with what and where, rather than drawing
something different. Arcs aren't read yet: draw them as curves (`C`) instead. The drawing needs a `viewBox`; its
units become the recipe's, in sixteenths.

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
| `svg_to_recipe_test` | Shapes, paths and curves, nested transforms, the `viewBox` origin, colours shared and opacity kept, even-odd holes, strokes as filled outlines, `fill="none"`, and arcs, text, unknown colours and a missing `viewBox` refused |
| `TextureRecipe_test` | Reading, and errors naming the line; indexed pixels exact; RGBA edges half covered; both fill rules; later shapes over earlier ones; and the example drawing (`examples/tile.svg`, converted by the build) drawn with the expected colours where they belong, its pixels pinned by checksum so any platform drawing differently fails |
