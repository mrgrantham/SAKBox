# Models from Blender

Two models made in Blender (`models/models.blend`), exported and cooked by the build ([`tools/assets`](../../../tools/assets/README.md)), each drawn with ScratchPadd's scene layer as desktops draw it, beside the DS's version of it:

```bash
bazel run //projects/examples/blender_model    # downloads Blender the first time (tools/blender)
```

| Model | Desktop | DS | How the DS's was made |
| --- | --- | --- | --- |
| Ship (Blender's Suzanne, subdivided) | 15,744 triangles | 1,573 | By hand in the `.blend`: `Ship_DS`, a linked duplicate with a Decimate modifier (0.1), the custom property `sak.platform = "ds"` |
| Rock (a bumpy sphere, flat-shaded) | 5,120 | 200 | By the build: no `Rock_DS`, so the rule's `SIMPLIFY_AGGRESSIVE` simplified it to its 200-polygon budget (`SIMPLIFY`, keeping its look, got it to 396) |

`hangar`, a `scene`, checks that a ship and two rocks fit a DS frame: 1,973 of its 2,048 polygons (`bazel build //projects/examples/blender_model:hangar`; the report is `hangar.scene.txt`). The cooks' reports are in `bazel-bin/projects/examples/blender_model/*.report.json`.

`models/make_models.py` made `models.blend`, as an artist might: `blender --background --factory-startup --python models/make_models.py -- models/models.blend`.

The binary and the models are tagged `blender`: the Linux check builds them on x86-64 only, as blender.org has no Blender for Linux on ARM.
