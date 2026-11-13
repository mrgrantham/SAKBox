# Models from Blender

Two models made in Blender (`models/models.blend`), exported and cooked by the build ([`tools/assets`](../../../tools/assets/README.md)), each drawn with ScratchPadd's scene layer as desktops draw it, beside the DS's version of it (and [on a DS](#on-a-ds)):

```bash
bazel run //projects/examples/blender_model    # downloads Blender the first time (tools/blender)
```

| Model | Desktop | DS | How the DS's was made |
| --- | --- | --- | --- |
| Ship (a small spaceship made of Blender's shapes: hull, canopy, wings, tail fin, engines) | 13,400 triangles | 1,340 | By hand in the `.blend`: `Ship_DS`, a linked duplicate with a Decimate modifier (0.1), the custom property `sak.platform = "ds"` |
| Rock (a bumpy sphere, flat-shaded) | 5,120 | 200 | By the build: no `Rock_DS`, so the rule's `SIMPLIFY_AGGRESSIVE` simplified it to its 200-polygon budget (`SIMPLIFY`, keeping its look, got it to 396) |

`hangar`, a `scene`, checks that a ship and two rocks fit a DS frame: 1,740 of its 2,048 polygons (`bazel build //projects/examples/blender_model:hangar`; the report is `hangar.scene.txt`). The cooks' reports are in `bazel-bin/projects/examples/blender_model/*.report.json`.

`models/make_models.py` made `models.blend`, as an artist might: `blender --background --factory-startup --python models/make_models.py -- models/models.blend`.

## On a DS

[`ds`](ds/source/main.cpp) draws the `hangar` scene on a Nintendo DS with ScratchPadd's [scene layer](../../scratchpadd/README.md#scene-layer), from the versions the build cooked for the DS: the ship (Ship_DS) lit with its vertex colours, a rock, and a second rock translucent (alpha 14 of 31), in front of a pane of glass that's see-through by its vertex colours' alpha, over lines marking the floor, in fog the background's colour, each labelled (the overlay's text on the top screen's bitmap layer, over a translucent rectangle, placed with `toScreen`), under a title at twice the size. The models are drawn as the build cooked them, in quads and strips: the bottom screen gives each model's triangles and polygons, and what each frame drew, 1,625 of the DS's 2,048 polygons and 4,848 of its 6,144 vertices: the 1,601 polygons and 4,772 vertices the `hangar` scene counted, plus the lines (18 and 54), the glass (a quad) and the labels' rectangles (6 and 18), less a polygon the DS leaves out as it's wholly off screen (its vertices are still sent, shared in a strip). A shows the ship as a wireframe, B turns the fog off and on; START quits.

```bash
bazel run //projects/examples/blender_model/ds:blender-model-ds   # needs Docker running (tools/devkitpro); opens melonDS
```

Its `extra_srcs` take the models' DS sources (`:ship_ds_sources`, `:rock_ds_sources`, which the two rules make visible to this package's subpackages) into `assets`, with the scene layer's sources and its DS backend's.

## On a 3DS

[`3ds`](3ds/source/main.cpp) draws the same scene on a Nintendo 3DS with the scene layer's 3DS backend, from the versions the build cooked for it: the full ship (13,400 triangles: the 3DS has room for it, so there's no `Ship_3DS`) and the rock (5,120), in stereo while the 3D slider is up. The bottom screen gives each model's triangles and, twice a second, what each frame drew (23,678 triangles in 5 draws: the scene's 23,640 plus the lines' 36) and how long the GPU and CPU took. A shows the ship as a wireframe, B turns the fog off and on; START quits.

```bash
python3 tools/fbi_install/fbi_install.py --3ds ADDRESS //projects/examples/blender_model/3ds:blender-model-3ds
bazel run //projects/examples/blender_model/3ds:blender-model-3ds   # opens Azahar
```

Its `extra_srcs` take the models' 3DS sources (`:ship_3ds_sources`, `:rock_3ds_sources`) into `assets`, with the scene layer's sources, its 3DS backend's, and the overlay's text (and stb_easy_font.h).

The binaries and the models are tagged `blender`: the Linux check builds them on x86-64 only, as blender.org has no Blender for Linux on ARM (and leaves the DS app out anyway, as it does every DS app: it needs Docker).
