# Models from Blender

Rules that turn a `.blend` into C++ for ScratchPadd's [scene layer](../../projects/scratchpadd/README.md#scene-layer), checked against each platform's budget when it's built. The design is in [`scene/DESIGN.md`](../../projects/scratchpadd/scene/DESIGN.md); the [example](../../projects/examples/blender_model/README.md) shows it working.

```python
load("//tools/assets:defs.bzl", "scene", "scene_model")

scene_model(
    name = "rock",
    src = "models/models.blend",
    object = "Rock",                          # the object in the .blend
    budget = {"ds": 200},                     # polygons; the DS's frame limit if not given
    over_budget = {"ds": "SIMPLIFY_AGGRESSIVE"},  # FAIL if not given
)

scene(
    name = "hangar",
    models = {":rock": 2, ":ship": 1},        # each model, and how many shown at once
)
```

```cpp
#include "rock.h"                             // from :rock (desktops)
Scene::Mesh rock = renderer.load(Assets::rock);
```

## What the rules make

`scene_model(name = "rock", ...)` exports the `.blend` with [Blender](../blender/README.md), then cooks it for each platform with the cook tool (`cook/`), each platform in an action of its own, so a model over the DS's budget stops only what's built for the DS.

| Target | What it is |
| --- | --- |
| `:rock` | A `cc_library` for desktops: `rock.h`, with `Assets::rock` (a `Scene::MeshData`), and `Assets::rockTriangles`, `rockVertices` and `rockPolygons` (polygons as the platform draws it: on the DS, its quads, strips and triangles, which its `MeshData`'s `primitives` give) |
| `:rock_ds_sources` | The DS's version, the same names, for a DS app's Makefile build: `rock_ds.h` and `rock_ds.cpp`, given by `devkitpro_nds_app`'s `extra_srcs` (the example's DS app has `"...:rock_ds_sources": "assets"`, and `assets` in its Makefile's `SOURCES` and `INCLUDES`). A DS app in another package needs the rule's `visibility` |
| `:rock_3ds_sources` | The 3DS's version, the same way, for a 3DS app's Makefile build (`devkitpro_3ds_app`'s `extra_srcs`): `rock_3ds.h` and `rock_3ds.cpp`. Its budget counts triangles (the 3DS draws triangles), against a frame of 45,000 by default (see `scene`) |
| `:rock_ds_preview` | The DS's version as a `cc_library` for desktops, in `Assets::Ds`, to see what the DS will draw |
| `:rock_cooked` | The files; its `report` output group has a report a platform (`rock_ds.report.json`): the object used, the mode, the budget, the polygons and vertices before and after, the steps taken and the error |

`scene` adds up what its models cost a frame on the DS and the 3DS, each times how many are shown at once, and fails if it's over the DS's 2048 polygons or 6144 vertices, or the 3DS's 45,000 triangles (about what a New 3DS draws lit at 60 fps in stereo, measured by [`scene/3ds_benchmark`](../../projects/scratchpadd/scene/3ds_benchmark/README.md); large triangles fill fewer, which only drawing shows). Its output, `hangar.scene.txt`, gives each platform's total and the costliest models. It's also a validation output, so anything depending on the scene checks it.

## A model's versions

One `.blend` can hold a model's versions for each platform. For the DS, the cook uses an object named after the model with `_DS` (`Rock_DS`), or one whose custom property `sak.platform` is `"ds"` and whose name starts with the model's; otherwise the model's own object. A version made by hand is still checked against the budget. A linked duplicate with a Decimate modifier makes a good one: it shares the original's mesh, so editing the original changes it too, and the export applies the modifier.

A model is its object's mesh in the object's own coordinates: where it sits in the `.blend` doesn't matter. Its vertex colours come out as sRGB, as Blender shows them (the glTF export makes them linear; the cook turns them back), and white where it has none.

## Over budget

| Mode | What it does when a model is over a platform's budget | When it still fails |
| --- | --- | --- |
| `FAIL` (default) | Nothing. The build stops, saying which model, the platform, its polygons and vertices against the budget and how far over, and what would fix it | Always, when over |
| `SIMPLIFY` | meshoptimizer's attribute-aware simplifier, aiming at the budget, with the outer edges and seams kept and the surface moving at most 5% of the model's size (`kSimplifyError`) | If it can't reach the budget within that |
| `SIMPLIFY_AGGRESSIVE` | `SIMPLIFY`, then without the error cap, then across seams, then the sloppy simplifier (merging nearby vertices whatever the mesh's shape), each from the original, until one fits | Only if none does |

- `--//tools/assets:over_budget=FAIL` (or either other mode) overrides every rule for one build. A CI run with `FAIL` lists every model that fits only because it was simplified.
- **A flat-shaded mesh** (each face its own vertices with the face's normal, as Blender exports flat shading) has every edge as a seam, so the simplifier could collapse nothing. The cook welds it (vertices at the same place with the same colour made one), simplifies that, and makes it flat-shaded again. The example's rock: `SIMPLIFY` took it from 5,120 to 396 triangles within the 5% cap, short of its 200; `SIMPLIFY_AGGRESSIVE` reached 200.
- **How the DS counts:** as the DS backend draws the model, in quads and strips (ScratchPadd's `Scene::buildPrimitives`): two triangles sharing an edge in one plane are a quad, and the rest are chained into strips. A polygon each triangle or quad; in vertices, 3 a triangle, 4 a quad, and a strip's once each. The cook writes those runs into the DS's version, so the DS draws what was counted; in the example's DS app the hardware's counters agreed to the vertex. Simplifying aims at the budget as triangles first, then higher by how far under it came (up to three times), keeping the most triangles that fit. A flat-shaded model gains only its quads: its triangles share no vertices to strip.
- Desktops have no budget, so their version is never simplified.

## Requirements

Blender is downloaded by Bazel the first time a model is built (see [`tools/blender`](../blender/README.md)): macOS on Apple silicon and Linux x86-64 only. Elsewhere (an Intel Mac, Linux on ARM) a model fails to build, saying so; `--action_env=BLENDER=/path/to/blender` uses a Blender of your own. Every `scene_model` target is tagged `blender`, and so must be anything using one (the example's binary is), so the [Linux check](../linux_check/README.md)'s ARM64 runs can leave them out.

## Tests

| Test | Covers |
| --- | --- |
| `Cook_test` | The cook on meshes made in the test: costs (a polygon and three vertices a triangle; on the DS, a flat grid's squares as quads, and simplifying keeping as many triangles as quads and strips allow), a model within budget left as it is in every mode, `FAIL` changing nothing, `SIMPLIFY` bringing a flat grid within budget without moving it, a bumpy one too rough for `SIMPLIFY`'s error cap that `SIMPLIFY_AGGRESSIVE` fits with its later steps, a flat-shaded mesh simplified and still flat-shaded, and the modes' names |
| `cook_tool_test` | The cook tool on the example's rock as Blender exported it in the build: `FAIL` over budget with its message and the fixes it suggests, `SIMPLIFY` falling short and saying `SIMPLIFY_AGGRESSIVE` goes further, `SIMPLIFY_AGGRESSIVE` fitting and writing its report, the desktop's version as it is, a missing object, and scenes within and over the DS's frame limit, with the report. Tagged `blender`. (Not `cook_test`: on a Mac's case-insensitive disk, its logs and `Cook_test`'s would share a folder) |
