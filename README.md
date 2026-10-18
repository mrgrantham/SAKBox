# SAKBox

A Swiss-Army Knife Toolbox: a Bazel monorepo of C++ projects, examples, and the third-party libraries they use.

## Getting Started

**Prerequisites**

- [Bazelisk](https://github.com/bazelbuild/bazelisk) (recommended), which installs and runs the Bazel version pinned in `.bazelversion` (9.2.0). A plain Bazel 9 install also works.
- A compiler that accepts `-std=c++26`: a recent Xcode on macOS (tested with Apple clang 21), or clang 17+ / gcc 14+ on Linux. Everything builds as C++26 (set in `.bazelrc`). The code under `projects/` also builds with `-pedantic-errors` (no compiler extensions), `-Werror` (no warnings) and `-Wsuggest-override` (overriding functions must say `override`; Waffles is exempt from this one for now).
- On Linux, the system packages listed under [Linux](#linux).
- Optional, for static analysis: clang-tidy, which Xcode doesn't include. On macOS, `brew install llvm` (Homebrew keeps it off your `PATH`, so your compiler stays Xcode's); on Linux, your distribution's `clang-tidy` package. See [Static analysis](#static-analysis).
- For the 3DS example only: Docker (on macOS, `brew install colima docker`, then `colima start`). Its devkitPro toolchain and libraries come from devkitPro's Docker image, pinned by digest in `tools/devkitpro/defs.bzl`; nothing is installed on the machine (see [`tools/devkitpro`](tools/devkitpro/README.md)). Without Docker running, that one target fails with a message saying so.

All other dependencies are declared in `MODULE.bazel` and downloaded by Bazel on the first build.

**Build, test and run**

``` bash
bazel build //...      # build everything
bazel test //...       # run all the tests
bazel run //projects/examples/precision_flight:precision_flight   # run one target
```

The first build takes a while, mostly to compile OpenCV, SDL2 and Boost; later builds are cached. Arguments for the program go after `--`, e.g. `bazel run //projects/examples/camera:exampleCamera -- 1`.

## Projects

| Project | Description |
| --- | --- |
| [ScratchPadd](projects/scratchpadd/README.md) | Message passing and thread management between subsystems ("padds") of one process, with an OpenGL/ImGui display padd. Demo: `bazel run //projects/scratchpadd/demo` |
| [flexmath](projects/flexmath/README.md) | Numeric precision that can change at runtime: fixed-point values, precision groups and powerup-style overrides |
| [Waffles](projects/waffles/README.md) | Mike Gashler's machine learning and data mining library (public domain), built with Bazel here. Includes its command-line tools (`learn`, `cluster`, `plot`, ...) and demos such as `chess` and `caronhill` |
| [Cyclone](projects/cyclone/README.md) | Ian Millington's rigid-body physics engine (MIT), maintained in SAKBox since upstream stopped in 2020. Includes a flexmath build in which every `real` is a `FlexReal`, and demos: `bazel run //projects/cyclone:<demo>` for `ballistic`, `bigballistic`, `blob`, `bridge`, `explosion`, `fireworks`, `flightsim`, `fracture`, `platform`, `ragdoll` or `sailboat`. `flightsim_flex` is the flight sim at adjustable precision: `[` and `]` step it, N switches between stochastic, truncating and nearest rounding |
| data_dep_retriever | Small helper that finds a binary's data files (shaders, textures, ...) in Bazel's runfiles at runtime |

## Examples

Small programs under `projects/examples/`, mostly one per library. Run any of them with `bazel run <target>`.

| Example | Target | What it does |
| --- | --- | --- |
| [Precision Flight](projects/examples/precision_flight/README.md) | `//projects/examples/precision_flight:precision_flight` | Star Fox-style rail game built on flexmath and ScratchPadd, where powerups change numeric precision. `--autopilot` plays itself |
| Camera | `//projects/examples/camera:exampleCamera` | Live webcam feed with OpenCV and SDL2. M mirrors, Q quits; optional camera index argument |
| OpenGL tutorial | `//projects/examples/opengl_tutorial:opengl_tutorial` | GLFW/GLEW scene with a textured, lit pyramid; WASD and Space move the camera |
| Shader playground | `//projects/examples/shader_playground:shader_playground` | Shadertoy-style fragment shaders rendered through ScratchPadd |
| ImGui + SDL2 | `//projects/examples/imgui_sdl2:exampleImguiSDL2` | Dear ImGui's demo window on SDL2 and OpenGL 3 |
| SDL2 | `//projects/examples/sdl2:exampleSDL2` | Opens an SDL2 window and renders a blank streaming texture (a starting template) |
| PNG | `//projects/examples/png:examplePng` | libpng's `pngtest`: reads a PNG and writes it back out |
| Logging | `//projects/examples/logging:spd_example`, `:greet` | spdlog and fmt basics |
| Backtrace | `//projects/examples/backtrace:exampleBacktrace` | Prints a stack trace with backward-cpp |
| Tabulate | `//projects/examples/tabulate:exampleTabulate` | Formatted terminal tables |
| YAML table | `//projects/examples/yamltable:exampleYaml` | Parses YAML with rapidyaml and yaml-cpp and prints a table |
| [3DS read-controls](projects/examples/read-controls/README.md) | `//projects/examples/read-controls:read-controls` | Nintendo 3DS homebrew: shows the buttons pressed and the circle pad's position. Builds `read-controls.3dsx` (run it with the Homebrew Launcher, or in the Azahar emulator as its README describes) using the devkitPro toolchain from `//tools/devkitpro`, which needs Docker |

## Third Party

`third_party/` holds Bazel build files for libraries that aren't in the [Bazel Central Registry](https://registry.bazel.build/); `MODULE.bazel` points each one at its upstream source. Libraries that are in the registry (fmt, googletest, Boost, Catch2, ...) are plain `bazel_dep`s.

## Editor Setup

Editors that use clangd (VS Code's clangd extension, CLion, Neovim, ...) need a `compile_commands.json` to know the include paths and C++26 flags Bazel uses. Generate it with:

``` bash
python3 tools/compile_commands/refresh.py
```

Re-run it after adding source files or changing BUILD deps, then restart clangd. The generated files are git-ignored.

To format a package's C++ files with clang-format, run its `clang_format` target, e.g. `bazel run //projects/flexmath:clang_format`. Most packages have one: `bazel query 'attr(name, "^clang_format$", //...)'` lists them.

## Static analysis

[clang-tidy](https://clang.llvm.org/extra/clang-tidy/) checks the code for likely bugs, avoidable slowness and outdated C++, using the checks chosen (with the reasons for leaving some out) in [`.clang-tidy`](.clang-tidy):

``` bash
python3 tools/compile_commands/refresh.py          # once, and after BUILD changes
python3 tools/clang_tidy/run.py                    # everything except Waffles
python3 tools/clang_tidy/run.py projects/cyclone   # one project
python3 tools/clang_tidy/run.py --fix projects/cyclone   # apply clang-tidy's fixes
python3 tools/clang_tidy/run.py --checks='-*,modernize-use-nullptr' --fix   # one check only
```

It prints each finding once (a header's findings aren't repeated per file that includes it), then a count per check, and exits 1 if there are any. It finds clang-tidy on your `PATH`, in Homebrew's LLVM, or at `$CLANG_TIDY`. Its findings are advice, not part of the build: review fixes before committing them, and rebuild and test afterwards.

## Platform Notes

Development and testing happen on **macOS** (Apple Silicon, Xcode).

### Linux

Not tested recently, but the build is meant to work. Bazelisk provides Bazel for x86-64 and ARM64 (including 64-bit Raspberry Pi OS). The windowing and OpenGL examples need the X11 and Mesa development packages:

``` bash
sudo apt install libxinerama-dev libxrandr-dev libxcursor-dev libxi-dev \
                 libglu1-mesa-dev libgl1-mesa-dev libegl1-mesa-dev
```

`Could NOT find OpenGL` means `libgl1-mesa-dev` is missing; `Could NOT find OpenGL (missing: EGL)` means `libegl1-mesa-dev` is. Package names can differ slightly between Ubuntu and Raspberry Pi OS.

### Windows

Not supported yet. Most libraries build there in principle, but nothing has been tested, and the OpenCV build (used by the camera example) only handles macOS and Linux library names so far.
