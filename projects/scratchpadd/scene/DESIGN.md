# Scene layer: design

The design of ScratchPadd's scene layer, reviewed before it was built and built in phases (see [Phases](#phases)). It replaces ScratchPadd's OpenGL-only `graphics` package with a small scene API that desktops and the Nintendo DS both implement, and adds an asset pipeline from Blender that checks every model against the DS's limits when it's built. The research behind it is in `reports/Scene layer for desktop and Nintendo.md`, outside the repo.

## Decisions

| Area | Decision |
| --- | --- |
| API | A small scene layer: meshes and their transforms, a camera, materials, lines, and a 2D overlay for text and panels |
| Backends | One per platform, chosen when building, with no virtual calls on the DS. OpenGL first, then the DS, then Metal (the default on Macs). The design leaves room for the 3DS, which isn't built yet. |
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
- **The overlay** is bitmap text and filled rectangles, in screen points. On the DS its shapes are drawn as 3D triangles on the top screen and its text on a 2D text layer over them (phase 3); text on the bottom screen stays the DS console, as now.
- **Windows and input stay outside the API.** A desktop app opens a `Window` (GLFW) and gets a `Renderer` for it; a DS app gets one for the top screen. Frames are paced as `DisplayPadd` paces them now: each frame pushes the next, and `endFrame` waits for the screen.
- **Desktop-only extras** go in `ScratchPadd/Scene/Desktop.hpp`: custom shaders, render to texture, the ImGui hook. Only desktop builds have it.

### Packages

| Package | What it holds |
| --- | --- |
| `//projects/scratchpadd/scene` | `Scene.hpp`, the overlay's font, the shared maths |
| `//projects/scratchpadd/scene/desktop` | What desktop apps depend on (the backend the build picks) and what desktop backends share: `Display` (GLFW), the extras' API (`Desktop.hpp`), the overlay's text (phase 4) |
| `//projects/scratchpadd/scene/gl` | The OpenGL 3.3 backend |
| `//projects/scratchpadd/scene/metal` | The Metal backend, on Macs (phase 4) |
| `//projects/scratchpadd/scene/ds` | The DS backend (libnds GX): display lists, vertex colours, the overlay; sources handed to DS apps' Makefile builds, as Cyclone's are now |
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
  | The test model | `Ship` with 15,744 triangles, and `Ship_DS`, a linked duplicate with a Decimate modifier at 0.1, exported with 1,573 (then Blender's monkey head; the example's ship is now a small spaceship, 13,400 and 1,340). Its `sak.platform` property and the vertex colours came through. |

- **Machines with no Blender build** (Intel Macs, Linux on ARM) can't export models, though the cook tool runs anywhere. On such a machine a model target fails with a plain message: Blender has no build for this machine, so build as x86-64 or point `BLENDER` at a Blender.
- **The Linux check skips models on ARM64.** Model targets, and every app and test that uses one, carry a `blender` tag. The Linux check's ARM64 runs (its default on Apple silicon) leave them out with `--build_tag_filters=-blender`, as they already leave out the 3DS apps (`-devkitpro`). Its `--x86-64` runs build and test them. So the ARM64 run no longer covers models, and a Linux-only problem in one shows up only in the slower x86-64 run. Nothing extra is committed, and nothing can go stale. (Considered and not chosen, for now: committing each model's `.glb` beside its `.blend` as a fallback for such machines, checked against a fresh export where Blender runs.)

## Phases

0. **This review.**
1. **The scene API and the OpenGL backend** *(done)*, with `debug_ui` split out of `graphics`, and `graphics` removed. The ScratchPadd demo, the shader playground, the Cyclone padds window and Precision Flight moved onto it, which removed their three copies of window and ImGui setup. Two things were added on the way: `Renderer::toScreen` and `Overlay::line`, for 3D lines drawn over everything (the padds window's joints through the bones), which the DS can't do with depth testing; and fog in the environment. Tests: each material, fog, wireframe, lines, the overlay, snapping and `toScreen` drawn into a hidden window and checked pixel by pixel (rather than against reference images, which would differ between GPUs), plus the transforms, meshes and overlay on their own.
2. **The asset pipeline** *(done)*: [`tools/blender`](../../../tools/blender/README.md) (Blender downloaded by Bazel) and [`tools/assets`](../../../tools/assets/README.md) (the export script, the cook tool, `scene_model` and `scene`, the three modes, the override flag and the reports), shown working by [`examples/blender_model`](../../examples/blender_model/README.md). What it settled on the way:
   - Each platform is cooked in an action of its own, so a model over the DS's budget stops only what's built for the DS.
   - The DS's version is a `Scene::MeshData` like the desktop's (triangle lists), counted at a polygon and three vertices a triangle, which the DS backend makes display lists of (phase 3).
   - Flat-shaded meshes are welded to simplify (otherwise every edge is a seam) and made flat again after.
   - A `scene` is a validation output too, so anything depending on it checks it.
   - Tests: the cook on meshes made in code, and the cook tool on a model Blender exported in the build (each mode's outcome and message, scenes within and over the limit).

3. **The DS backend** *(done)*: [`scene/ds`](ds), on the top screen, with display lists, vertex colours, lights, translucency, lines, the overlay, `toScreen`, and a debug-build check of the DS's polygon and vertex memory. The DS ragdoll moved onto it, and the Blender example has a DS app ([`examples/blender_model/ds`](../../examples/blender_model/README.md)) drawing the models the build cooked for the DS. What it settled:
   - **`Scalar` is Q16.16 on the DS** (flexmath's `FlexPoint16`). Desktops build the shared code and the display lists that way too (`SCRATCHPADD_SCENE_FIXED_POINT`), so tests cover it. `Vector3::length` and `normalised` divide by the largest component first: in Q16.16 a vector 182 long squares past the range (a 16 m quad's normal came out wrong).
   - **Display lists are packed on the DS, when a mesh is first drawn each way** (lit or unlit, the material's colour, the ambient light), and a mesh keeps four. The packer is plain C++ ([`ds/src/DisplayList.cpp`](ds/src/DisplayList.cpp)), tested on desktops. A DS vertex is lit or coloured, not both, so a lit one sets the material's diffuse colour to its colour and its emission to that times the ambient light, then its normal: the desktop's colour x (ambient + lights), exactly. Colours and normals are sent only when they change.
   - **Triangle lists**, as the cook counts them (a polygon and three vertices a triangle): the hardware's counters matched the counts the build gave. Strips and quads, which cost fewer vertices, are left for later.
   - **The DS's 4.12 vertices reach only +-8**, so a bigger mesh is stored smaller by a power of two and scaled back up by the matrix. A transform's scale is applied to positions only (the DS's `MTX_SCALE`), so normals stay unit length, which the DS doesn't restore.
   - **The overlay's text is on a 2D text layer** (background 1, VRAM bank E), in 8-point cells, costing no polygons, rewritten only when it changes. So text is measured by the backend (`Overlay::textWidth`, `lineHeight`), and `Overlay` keeps text as given rather than as triangles. Its shapes are 3D triangles in front of the scene, later over earlier.
   - **Translucent meshes** are drawn farthest first, flushed with manual sorting, each with its own polygon ID (the DS blends a translucent polygon only over another ID's).
   - **Not on the DS yet:** fog (`capabilities().fog` is false), a vertex colour's alpha, and the text's scale.
   - Tests: the display lists read back command by command, with float and Q16.16; the rest checked by screenshots in melonDS (the ragdoll, and the Blender models: lit vertex colours, a translucent rock, lines, labels, and 1,763 polygons against the scene's 1,740 plus its lines and labels, less one wholly off screen), and the overflow message forced once.
4. **Metal** *(done)*: [`scene/metal`](metal), written with Apple's metal-cpp, the default desktop backend on a Mac. What it settled:
   - **What desktops share moved out of the OpenGL backend** into [`scene/desktop`](desktop): the window (`Display`, GLFW), whose backend part is behind `WindowBackend.hpp` (OpenGL: its context and swap; Metal: a `CAMetalLayer` in the window's view, the backend's only Objective-C); the extras' API, `Desktop.hpp`, which each backend defines; and `:desktop`, which apps depend on and which picks the backend: Metal on a Mac, OpenGL elsewhere, or as `--//projects/scratchpadd/scene/desktop:backend=opengl|metal` says. Includes moved from `ScratchPadd/Scene/GL/` to `ScratchPadd/Scene/Desktop/`.
   - **The Metal backend draws what the OpenGL one does**, with its shaders translated by hand into Metal's shading language: 4x multisampled into the window's own textures, then copied into the layer's drawable. Drawing into its own texture lets ImGui draw over the finished frame (debug_ui has a renderer part for each backend), and lets a hidden window keep its frame, which `Desktop::readPixel` reads. So one renderer test runs on both backends, checking the same pixels.
   - **Custom shaders stay GLSL.** `ShaderPanel` translates them when they're loaded: glslang compiles GLSL to SPIR-V under Vulkan's rules relaxed, which gathers an OpenGL shader's loose uniforms into one block, and SPIRV-Cross writes Metal from it, flipping the vertex stage's y so textures come out laid out as OpenGL's. The shaders are compiled from source when they load: Xcode's offline Metal compiler isn't needed. glslang and SPIRV-Cross aren't in Bazel's registry; their BUILD files are in `third_party/` (glslang compiled as C++17: C++26 rejects its arithmetic between enum types).
   - **Colours on screen:** macOS treats the Metal layer's colours as sRGB and converts them to the display; an OpenGL window's aren't converted that way, so on a wide-gamut display OpenGL's look a little more saturated. The frames' pixels are the same.
   - Tests: the renderer test on each backend; every shader of the shader playground translated and compiled by Metal. Checked on screen: the shader playground, Precision Flight, the Cyclone padds window and the Blender models, all on Metal.
5. **Later:** the 3DS, once its budgets are measured (on hardware).
