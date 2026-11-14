# Toon shading and outlines: research and design

Status: research, for review. Nothing is built yet.

The goal is the planned look: untextured, flat-coloured surfaces lit in a few hard bands, with dark outlines around shapes. It's a Tron-like style that suits all three platforms and costs little. Each platform gets there differently, because their GPUs are very different.

## What each platform can do

### DS: built into the hardware

The DS's 3D engine has toon shading and outlines in hardware ([GBATEK](https://problemkaputt.de/gbatek.htm), "DS 3D Toon, Edge, Fog, Alpha-Blending"):

- **Toon table.** `DISP3DCNT` bit 1 switches the engine from highlight shading to toon shading, and a polygon opts in with its polygon attribute (`POLY_TOON_HIGHLIGHT` in libnds). For those polygons, the red channel of the lit vertex colour (0 to 31, interpolated per pixel) indexes a 32-entry colour table (`TOON_TABLE`, from `0x04000380`). So lighting gives an intensity, and the table turns it into hard bands per pixel. libnds: `glSetToonTable()`, `glSetToonTableRange()`.
  - The catch: the table is **one for the whole frame**. So the colour comes from the table, not from each object's own colour. Ways round it:
    1. Use the table only for brightness bands (dark, mid, light: white to black), and give objects their colour some other way (below).
    2. Use the other mode, highlight shading. It *adds* the table's colour to the vertex colour, so each object keeps its colour and the table adds bands of light. It brightens rather than darkens, but with a dark-to-light table it reads as bands.
  - Option 2 seems the better fit for coloured objects. I'd prototype both on hardware.
- **Edge marking (outlines).** `DISP3DCNT` bit 5 draws a 1-pixel outline wherever a polygon's ID (0 to 63) differs from the polygon behind it or the background. The colour comes from an 8-entry table (`EDGE_COLOR`, by polygon ID / 8). libnds: `glEnable(GL_OUTLINE)`, `glSetOutlineColor()`. It's free: no extra geometry, no extra pass. The scene layer's DS renderer already gives polygons IDs (`POLY_ID`), so each object, or each part, needs its own ID and the outlines appear.
  - Limits: outlines are only drawn against a *different* ID, so a shape's own creases don't get lines. They're 1 pixel thick, and they show some flicker with antialiasing on. GBATEK has the details.

### 3DS: by lookup table or by shader

The 3DS's PICA200 GPU has no toon mode, but it has two ways in:

1. **Fragment lighting with a stepped lookup table.** The PICA200's fragment lighting computes lighting per pixel through lookup tables (LUTs) indexed by, for example, the angle between the normal and the light. citro3d builds them with `LightLut_FromArray()` (any curve you give it) and attaches them with `C3D_LightEnvLut()` ([citro3d lightlut.h](https://oreo639.github.io/citro3d/lightlut_8h.html), [light.h](https://oreo639.github.io/citro3d/light_8h.html), [libctru's "lenny" example](https://libctru.devkitpro.org/graphics_2gpu_2lenny_2source_2main_8c-example.html)). A stepped curve gives crisp toon bands, per pixel, in hardware, at no shader cost. But the scene layer's 3DS renderer **lights in its vertex shader** today (`scene.v.pica`), not with fragment lighting, so this means moving its lighting to fragment lighting. That's a real change, though it's the 3DS's intended way to light.
2. **Bands in the vertex shader.** Quantise the lighting per vertex. It's cheap and needs no new GPU setup, but the bands are blurred, because the GPU interpolates colour between vertices. On our low-polygon, flat-shaded models (one normal per face), every vertex of a face has the same light, so the bands come out crisp anyway. It's worth trying first: it's a few lines in the existing shader.

**Outlines** on the 3DS have no hardware help. The usual method is the **inverted hull**: draw each outlined mesh a second time, a little larger along its normals, with front faces culled, in the outline colour. Only the rim shows, as an outline. It doubles those meshes' vertices, so it's for key objects only (the ship, obstacles). Stereo 3D works with it unchanged, since it's real geometry.

### Desktops (OpenGL, Metal, Vulkan): shaders

- **Bands:** quantise the lighting in the fragment shader, either a stepped ramp (`floor(NdotL * bands) / bands`) or a small 1D ramp texture with nearest filtering. The ramp is a lookup table, not a surface texture, so it fits the no-textures look.
- **Outlines:** two good choices:
  1. **Inverted hull**, the same as the 3DS, so the platforms match.
  2. **Screen-space edges:** a post-process pass that finds edges in the depth buffer and in the normals (or object IDs), as the DS does in hardware. It outlines creases too, at any thickness. It costs a full-screen pass and an extra render target in each backend (OpenGL, Metal, Vulkan).

## A design across the platforms

The scene layer gets a **material style** rather than per-platform code in apps:

```cpp
Scene::Material ship{.colour = {80, 220, 255}, .shading = Scene::Shading::Toon, .outline = true};
Scene::Environment environment;
environment.toonBands = 3;                 // light, mid, dark
environment.outlineColour = {10, 10, 20};
```

Each backend does what it can:

| | Bands | Outlines |
| --- | --- | --- |
| DS | Hardware toon or highlight table, set from `toonBands` | Edge marking, by polygon ID (an object each) |
| 3DS | Vertex-shader bands first, fragment-lighting LUT if needed | Inverted hull for `outline` materials |
| Desktops | Fragment shader ramp | Inverted hull (to match the 3DS), or screen-space edges |

Budgets: the DS's outlines and bands cost nothing. The inverted hull doubles an outlined mesh's vertices, which counts against the 3DS's measured vertex budget (the Blender scene was vertex-bound, about 14 ms a frame), so outlines stay on a few key objects there.

## Questions

1. **Coloured objects on the DS:** toon mode (one shared table, so colour by other means) or highlight mode (keeps each object's colour, adds light bands)? I'd prototype both on your DS Lite.
2. **Outlines on desktops:** the inverted hull, matching the 3DS, or screen-space edges, which also outline creases?
3. **3DS:** try vertex-shader bands first (cheap, crisp on flat-shaded models), and move to fragment lighting only if they don't look right?
4. **How many bands,** and should outline colour and thickness be per object or per scene?
5. **Which apps first?** The Cyclone ragdoll and the Blender model scene on all three platforms would show it well, before the ship game uses it.
