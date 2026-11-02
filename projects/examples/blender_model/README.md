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

[`ds`](ds/source/main.cpp) draws the `hangar` scene on a Nintendo DS with ScratchPadd's [scene layer](../../scratchpadd/README.md#scene-layer), from the versions the build cooked for the DS: the ship (Ship_DS) lit with its vertex colours, a rock, and a second rock translucent (alpha 14 of 31), over lines marking the floor, each labelled (the overlay's text on the top screen's text layer, over a translucent rectangle, placed with `toScreen`). The bottom screen gives what each frame drew: 1,763 of the DS's 2,048 polygons and 5,292 of its 6,144 vertices, the 1,740 polygons the `hangar` scene counted plus the lines (18) and the labels' rectangles (6), less one the DS leaves out as it's wholly off screen. A shows the ship as a wireframe; START quits.

```bash
bazel run //projects/examples/blender_model/ds:blender-model-ds   # needs Docker running (tools/devkitpro); opens melonDS
```

Its `extra_srcs` take the models' DS sources (`:ship_ds_sources`, `:rock_ds_sources`, which the two rules make visible to this package's subpackages) into `assets`, with the scene layer's sources and its DS backend's.

The binaries and the models are tagged `blender`: the Linux check builds them on x86-64 only, as blender.org has no Blender for Linux on ARM (and leaves the DS app out anyway, as it does every DS app: it needs Docker).
