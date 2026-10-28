# Scene layer: design

A proposal for review, not yet built. It replaces ScratchPadd's OpenGL-only `graphics` package with a small scene API that desktops and the Nintendo DS both implement, and adds an asset pipeline from Blender that checks every model against the DS's limits when it's built. The research behind it is in `reports/Scene layer for desktop and Nintendo.md`, outside the repo.

## Decisions

| Area | Decision |
| --- | --- |
| API | A small scene layer: meshes and their transforms, a camera, materials, lines, and a 2D overlay for text and panels |
| Backends | One per platform, chosen when building, with no virtual calls on the DS. OpenGL first, Metal later. The DS after OpenGL. The design leaves room for the 3DS, which isn't built yet. |
| Animation | Rigid parts only: each part is a mesh with its own transform (as the ragdoll's bones are). No skinning. |
| Surfaces on handhelds | Vertex colours and lighting; no textures yet |
| DS screens | 3D on the top screen; text on the bottom |
| ImGui | A separate desktop-only library, never compiled for the DS |
| Desktop extras | Custom shaders, post effects and the like go behind a desktop-only header; an app that uses them builds only for desktops |
| Assets | `.blend` files, exported by Blender inside the build |
| Platform versions | One file can hold several versions of a model (`Ship`, `Ship_DS`), told apart by name or a custom property |
| Budgets | Checked per model, and per scene against everything the scene shows at once |
| Over budget | `FAIL`, `SIMPLIFY` or `SIMPLIFY_AGGRESSIVE`, per platform, `FAIL` by default; a build flag can override every rule |
| Run-time detail switching | Not yet: one version of each model per platform, picked when building |

## The API

One header, `ScratchPadd/Scene/Scene.hpp`, the same on every platform:

```cpp
namespace ScratchPadd::Scene {

using Scalar = /* float on desktops; flexmath's FlexPoint16 (Q16.16) on the DS */;
struct Vector3 { Scalar x, y, z; };
struct Transform { Scalar rotation[3][3]; Vector3 position; };   // a rigid part's place

struct Colour { std::uint8_t r, g, b, a = 255; };
struct Material {
  enum class Shading { unlit, lit } shading = Shading::lit;
  Colour colour;            // multiplies the vertex colours (or is the colour, lit on the DS)
  std::uint8_t alpha = 31;  // 0-31, as the DS has it
  bool wireframe = false;   // only the triangles' edges (the DS: alpha 0)
};
struct Light { Vector3 direction; Colour colour; };   // directional; up to 4
struct Camera { Vector3 eye, target, up; float fieldOfView, near, far; };

// A model the build cooked (see "Assets"): its data, already in the backend's
// own format, compiled into the program
struct CookedMesh;
class Mesh;  // a cooked mesh uploaded for drawing

class Renderer {
public:
  Mesh load(const CookedMesh &mesh);
  void beginFrame(const Camera &camera, std::span<const Light> lights, Colour background);
  void draw(const Mesh &mesh, const Transform &where, const Material &material);
  void drawLines(std::span<const Vector3> ends, Colour colour);  // pairs of ends
  Overlay &overlay();  // text, rectangles and lines in screen points, over the 3D
  bool toScreen(const Vector3 &point, float &x, float &y) const; // a 3D point in the overlay's
  void endFrame();     // waits for the screen's refresh
  Capabilities capabilities() const;  // e.g. how many polygons a frame, if limited
};

} // namespace ScratchPadd::Scene
```

- **Retained meshes, frames drawn fresh.** Meshes are uploaded once and drawn by handle, so the DS can keep each as a display list and a desktop as a vertex buffer. Everything else is drawn each frame, so the API keeps no scene graph to sync.
- **The materials are what the DS can do in hardware:** unlit or lit with up to 4 directional lights, a colour, alpha in 32 steps, and wireframe; the environment adds fog. Desktops draw them all with one built-in shader. Outlines (the DS's edge marking) and toon shading can come later, the same way.
- **The DS can't take floats cheaply,** having no floating-point hardware. So `Scalar` is Q16.16 there, which is what Cyclone already runs in on the DS, and float on desktops. An app computes its transforms in `Scalar`. The ragdoll's bones go straight in, with no conversion. *For review:* an alternative is one fixed type everywhere, but desktop code would then pay for fixed point it doesn't need.
- **The backend is a build-time choice.** `Renderer`'s methods are defined in each backend's own `.cpp`, and its state is a `Renderer::Backend` each backend defines, behind a pointer. A program links one backend's library (the DS's Makefile build will get the DS's). Nothing is virtual, so the DS pays nothing to dispatch; the pointer costs one allocation, when the renderer is made.
- **Capabilities, not assumptions.** `capabilities()` says what a backend can't do, e.g. a polygon limit, so a shared app can check for it.
- **Lines** count against the DS's polygon budget: the DS draws each as a triangle with two corners in the same place.
- **The overlay** is bitmap text and filled rectangles, in screen points. On the DS it's drawn as 3D quads on the top screen; text on the bottom screen stays the DS console, as now.
- **Windows and input stay outside the API.** A desktop app opens a `Window` (GLFW) and gets a `Renderer` for it; a DS app gets one for the top screen. Frames are paced as `DisplayPadd` paces them now: each frame pushes the next, and `endFrame` waits for the screen.
- **Desktop-only extras** go in `ScratchPadd/Scene/Desktop.hpp`: custom shaders, render to texture, the ImGui hook. Only desktop builds have it.

### Packages

| Package | What it holds |
| --- | --- |
| `//projects/scratchpadd/scene` | `Scene.hpp`, the overlay's font, the shared maths |
| `//projects/scratchpadd/scene/gl` | The OpenGL 3.3 backend, and `Window` (GLFW) |
| `//projects/scratchpadd/scene/ds` | The DS backend (libnds GX): display lists, vertex colours, overlay quads; sources handed to DS apps' Makefile builds, as Cyclone's are now |
| `//projects/scratchpadd/debug_ui` | ImGui, the theme and the control panels, moved out of `graphics`; desktop only |
| later: `scene/metal`, `scene/3ds` | |

`graphics` goes away once its users have moved: the ScratchPadd demo and `DisplayPadd`, the shader playground (through `Desktop.hpp`), the Cyclone padds window, and Precision Flight's display. Each of the last three sets up its own GLFW window, OpenGL and ImGui today. The Cyclone desktop demos (fixed-function OpenGL through their own framework) move later, and separately.

## Assets

### The rules

```python
load("//tools/assets:defs.bzl", "scene_model", "scene")

scene_model(
    name = "ship",
    src = "ship.blend",
    object = "Ship",                       # the desktop version; Ship_DS etc. found beside it
    budget = {"ds": 600},                  # polygons, per platform (default: the platform's frame limit)
    over_budget = {"ds": "SIMPLIFY_AGGRESSIVE"},   # default FAIL
)

scene(
    name = "hangar",
    models = {":ship": 2, ":crate": 10},   # each model, and how many at once at most
)
```

- **One file, several versions.** For each platform the cook uses the object for that platform: `Ship_DS`, or any object with the custom property `sak.platform = "ds"`. If there isn't one, it uses `Ship`. A version made for a platform is still checked against that platform's limits, so a hand-made `Ship_DS` can't quietly break them.
- **What a rule produces.** `scene_model` produces one library per platform: `:ship` for desktops (meshes as vertex and index arrays) and `:ship_ds_sources` for DS apps' Makefile builds (packed display lists). Each holds a `CookedMesh` named after the model. It also writes a report: triangles and vertices before and after, the error if simplified, and which step of simplification it took.
- **Scenes.** `scene` adds up its models' worst-case cost, each times its count, and fails if the total is over the platform's frame limit: on the DS, 2048 polygons and 6144 vertices. Its report lists the models by cost. It does this by reading each model's report (a Bazel provider), so it never re-cooks.

### Over budget

| Mode | What it does when a model is over a platform's budget | When it still fails |
| --- | --- | --- |
| `FAIL` (default) | Nothing is changed. The build stops and names the model, the platform, the count against the limit, and how far over it is. | Always, when over |
| `SIMPLIFY` | meshoptimizer reduces the triangles, keeping UV seams, outer edges, normals and vertex colours where they are, with the error capped | If it can't reach the budget within that |
| `SIMPLIFY_AGGRESSIVE` | `SIMPLIFY` first, then lets the simplification cross UV seams, then merges nearby vertices regardless of the mesh's structure | Only if nothing reaches it |

- `--//tools/assets:over_budget=FAIL` (or either other mode) overrides every rule for one build. A CI run with `FAIL` lists every model that only fits because it was simplified.
- **On the DS, budgets count what the hardware counts.** The cook joins triangle pairs into quads and builds strips, which the DS draws natively. It then counts the polygons and vertices actually sent, not the triangles in Blender.
- **Desktops have no budget,** so a desktop build never simplifies.

### Blender in the build

- **Downloaded by Bazel.** A module extension downloads Blender 5.2.3 LTS for the machine Bazel runs on, checked against blender.org's published SHA-256, the way melonDS is fetched. On macOS it's a `.dmg`, which the repository rule mounts and copies Blender out of; on Linux it's a `.tar.xz`. `BLENDER` set to a Blender program uses that instead, as `MELONDS` does.
- **Run headless, one per file.** Each `.blend` is exported by `blender --background --factory-startup model.blend --python export.py -- model.glb`. `export.py` sets every glTF exporter option itself (modifiers applied, custom properties as extras, the active vertex colours, Y up), so a Blender update can't change them silently. The cook tool, written in C++ with cgltf and meshoptimizer, reads the `.glb` and writes each platform's data.
- **What the check found** (Blender 5.2.3 on an Apple-silicon Mac):

  | Question | Finding |
  | --- | --- |
  | Repeatable exports | Yes: the same `.blend` exported twice gave byte-identical `.glb` files, so Bazel's cache holds. Linux x86-64 (in Docker) gave the same bytes as the Mac, so one shared cache could serve both. |
  | Downloads | macOS on Apple silicon (`.dmg`, 346 MB), Linux x86-64 (`.tar.xz`, 385 MB), Windows. None for Intel Macs or Linux on ARM. |
  | Size, unpacked | 907 MB |
  | Time | 8.5 s for the first start; about 1 s an export after that |
  | Linux needs | X11/Wayland client and GL libraries even headless (`libxkbcommon0`, `libxkbcommon-x11-0`, `libsm6`, `libice6`, `libxi6`, `libxrender1`, `libxfixes3`, `libxxf86vm1`, `libgl1`, `libegl1`, `libwayland-client0`, `libdecor-0-0`); the top-level README's Linux notes and the Linux check's Dockerfiles would gain them |
  | The test model | `Ship` with 15,744 triangles, and `Ship_DS`, a linked duplicate with a Decimate modifier at 0.1, exported with 1,573. Its `sak.platform` property and the vertex colours came through. |

- **Machines with no Blender build** (Intel Macs, Linux on ARM) can't export models, though the cook tool runs anywhere. On such a machine a model target fails with a plain message: Blender has no build for this machine, so build as x86-64 or point `BLENDER` at a Blender.
- **The Linux check skips models on ARM64.** Model targets, and every app and test that uses one, carry a `blender` tag. The Linux check's ARM64 runs (its default on Apple silicon) leave them out with `--build_tag_filters=-blender`, as they already leave out the 3DS apps (`-devkitpro`). Its `--x86-64` runs build and test them. So the ARM64 run no longer covers models, and a Linux-only problem in one shows up only in the slower x86-64 run. Nothing extra is committed, and nothing can go stale. (Considered and not chosen, for now: committing each model's `.glb` beside its `.blend` as a fallback for such machines, checked against a fresh export where Blender runs.)

## Phases

0. **This review.**
1. **The scene API and the OpenGL backend** *(done)*, with `debug_ui` split out of `graphics`, and `graphics` removed. The ScratchPadd demo, the shader playground, the Cyclone padds window and Precision Flight moved onto it, which removed their three copies of window and ImGui setup. Two things were added on the way: `Renderer::toScreen` and `Overlay::line`, for 3D lines drawn over everything (the padds window's joints through the bones), which the DS can't do with depth testing; and fog in the environment. Tests: each material, fog, wireframe, lines, the overlay, snapping and `toScreen` drawn into a hidden window and checked pixel by pixel (rather than against reference images, which would differ between GPUs), plus the transforms, meshes and overlay on their own.
2. **The asset pipeline:**
   - the Blender download and export;
   - the cook tool;
   - `scene_model` and `scene`;
   - the three modes, the override flag and the reports.

   Tests: a model under and over budget in each mode, a hand-made `Ship_DS` that's itself over, a scene over its frame limit, and the flag.
3. **The DS backend:** display lists, vertex colours, lights, the overlay, and a debug-build check of the DS's polygon and vertex counters. The DS ragdoll moves onto it.
4. **Later:** Metal, then the 3DS once its budgets are measured.
