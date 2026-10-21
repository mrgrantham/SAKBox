# SAKBox

A Swiss-Army Knife Toolbox: a Bazel monorepo of C++ projects, examples, and the third-party libraries they use.

## Getting Started

**Prerequisites**

- [Bazelisk](https://github.com/bazelbuild/bazelisk) (recommended), which installs and runs the Bazel version pinned in `.bazelversion` (9.2.0). A plain Bazel 9 install also works.
- A compiler that accepts `-std=c++26`: a recent Xcode on macOS (tested with Apple clang 21), or clang 17+ / gcc 14+ on Linux. Everything builds as C++26 (set in `.bazelrc`). The code under `projects/` also builds with `-pedantic-errors` (no compiler extensions), `-Werror` (no warnings), `-Wsuggest-override` (overriding functions must say `override`) and `-Wnon-virtual-dtor` (a class with virtual functions has a virtual destructor); with Clang, also `-Wsuggest-destructor-override` (destructors too) and `-Wdocumentation` (doc comments must match the code). See [Code conventions](#code-conventions). GCC doesn't have the Clang-only checks and rejects them as unknown options, so `.bazelrc` keeps them in a `clang` config: macOS turns it on itself (it always builds with Clang), and on Linux you add `--config=clang` when building with Clang, for example `bazel build --config=clang //...`. These apply to the repo's own code only: external libraries' headers are included as system headers (`--features=external_include_paths`), so their warnings don't count against it.
- On Linux, the system packages listed under [Linux](#linux).
- Optional, for static analysis and formatting: clang-tidy and clang-format (18 or later, for `.clang-format-ignore`), which Xcode doesn't include. On macOS, `brew install llvm clang-format` (Homebrew keeps LLVM off your `PATH`, so your compiler stays Xcode's); on Linux, your distribution's `clang-tidy` and `clang-format` packages. See [Static analysis](#static-analysis), which also covers the optional pre-commit hook that checks both on each commit (`git config core.hooksPath tools/git_hooks`).
- For the 3DS example only: Docker (on macOS, `brew install colima docker`, then `colima start`). Its devkitPro toolchain and libraries come from devkitPro's Docker image, pinned by digest in `tools/devkitpro/defs.bzl`; nothing is installed on the machine (see [`tools/devkitpro`](tools/devkitpro/README.md)). Without Docker running, that one target fails with a message saying so.
- Optional, to check a change on Linux from a Mac: Docker, which [`tools/linux_check`](tools/linux_check/README.md) uses to build and test the repo with GCC or Clang in a Linux container (see [Linux](#linux)).

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
| [Cyclone](projects/cyclone/README.md) | Ian Millington's rigid-body physics engine (MIT), maintained in SAKBox since upstream stopped in 2020. Includes a flexmath build in which every `real` is a `FlexReal`, and demos: `bazel run //projects/cyclone:<demo>` for `ballistic`, `bigballistic`, `blob`, `bridge`, `explosion`, `fireworks`, `flightsim`, `fracture`, `platform`, `ragdoll`, `sailboat` or `stack`. `flightsim_flex` is the flight sim at adjustable precision: `[` and `]` step it, N switches between stochastic, truncating and nearest rounding |
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

To format C++ files with clang-format in the repo's style (see [Code conventions](#code-conventions)), run from the repo root:

``` bash
python3 tools/clang_format/format.py              # the C/C++ files you've changed (staged or not, and new files)
python3 tools/clang_format/format.py --all        # every C/C++ file under projects/
python3 tools/clang_format/format.py FILE ...     # just these files
python3 tools/clang_format/format.py --check      # list what needs formatting, change nothing (add --all for everything)
```

It leaves files in `.clang-format-ignore` alone. `clang-format -i FILE` does the same for one file. Editors that use clangd (VS Code, CLion, Neovim) read `.clang-format` too, so their Format Document or format-on-save gives the same result.

## Code conventions

**Virtual functions use exactly one of `virtual`, `override` or `final`** (as the C++ Core Guidelines, C.128, and Google's style guide recommend):

| Write | When |
| --- | --- |
| `virtual` | Only where a virtual function is introduced, in the class that first declares it, including a base class's destructor: `virtual ~Base() = default;` |
| `override` | On every function that overrides one, destructors included: `void draw() override;`, `~Derived() override;` |
| `final` | Rarely, instead of `override`, to stop further overriding |

Never `virtual void draw() override;`: an override is already virtual, and the extra `virtual` hides whether a function is new or overriding. `override` also makes the compiler check that the function really overrides something, so a mistyped signature is an error rather than a function that's silently never called.

The build enforces it for everything under `projects/`: `-Wsuggest-override` makes an unmarked override an error, and with Clang `-Wsuggest-destructor-override` does the same for destructors. clang-tidy's `modernize-use-override` also flags a redundant `virtual` (see [Static analysis](#static-analysis)).

**A class with virtual functions has a virtual destructor** (or a protected one, if it's never deleted through the base), as the C++ Core Guidelines (C.35) recommend: otherwise deleting a derived object through a base pointer, or destroying it where only the base is known, skips the derived part. The build's `-Wnon-virtual-dtor` makes a missing one an error under `projects/`, at the class itself. Clang has another check, applied where such an object is destroyed rather than where the class is declared, which only fired on Linux, where GCC's standard library destroys `std::variant` members that way; this one reports it everywhere.

**Doc comments must match the code they describe.** With Clang, the build's `-Wdocumentation` makes it an error (GCC has no such check), under `projects/`, for a Doxygen-style comment (`/** */` or `///`) to name a parameter the function doesn't have, to give a `@return` for a function returning `void`, or to contain broken HTML (write `\<pre\>` for a literal tag). It checks only the comments that exist: it doesn't require one on every function or every parameter, so a sentence saying what a function does and why is enough. When you rename or remove a parameter, update its `@param` too.

**C++ files are formatted with clang-format** in the repo's style (`.clang-format`): LLVM style with 100-column lines, which is how most of the code was already written. Waffles keeps its own style (tabs, braces on their own lines), set in `projects/waffles/.clang-format`; clang-format uses the nearest `.clang-format` above a file, so SAKBox's Waffles tests, which have their own in `projects/waffles/tests/`, use the repo style. Third-party code kept as published (libpng's `pngtest.cpp`, libctru's `read-controls` example, and the libraries bundled inside Waffles) is listed in `.clang-format-ignore`, so clang-format leaves it alone. Where a layout matters more than the rule, such as a table aligned in columns by hand, put `// clang-format off` and `// clang-format on` around it, each alone on its line (clang-format doesn't recognise the marker with text after it), with the reason in a comment above. Format files with `python3 tools/clang_format/format.py`, which formats the files you've changed (see [Editor Setup](#editor-setup) for its options); the pre-commit hook checks it (see [Checking each commit](#checking-each-commit)).

**Text files use LF line endings.** `.gitattributes` has Git store and check out every text file with LF on every platform, so an editor or script that rewrites a file can't change its endings. Git detects binary files and leaves them alone.

**Bazel libraries are named after what they hold.** A library built from one main source file is named after that file, spelled as it is: Waffles' `WImagePng.cpp` is `:WImagePng`, its `Gui.cpp` is `:Gui`. A library of many files is named after its folder (`:WClasses`, `:flexmath`), or, when that name is the folder's program, after what the files are together (`:flight_padds`, `:demo_padds`). Don't shorten a name to a category (`:png`, `:gui`): it hides what's inside and can be mistaken for a third-party library (`@libpng`).

**Test files are named `<Source>_test.cpp`**, after the file they test and spelled as it is: `FlexReal_test.cpp` tests `FlexReal.hpp`, `WMatrix_test.cpp` tests `WMatrix.h`, `core_test.cpp` tests Cyclone's `core.h`. A test of something that isn't one file (threading, the README's examples) is named after that: `Threading_test.cpp`, `Examples_test.cpp`. Its Bazel target has the same name (`:FlexReal_test`). This is the googletest and Bazel convention; it also sorts a test next to the code it tests. Libraries kept as published (libpng's `pngtest.cpp`, Waffles' own `src/test/`) keep their names.

## Static analysis

[clang-tidy](https://clang.llvm.org/extra/clang-tidy/) checks the code for likely bugs, avoidable slowness and outdated C++, using the checks chosen (with the reasons for leaving some out) in [`.clang-tidy`](.clang-tidy):

``` bash
python3 tools/compile_commands/refresh.py          # once, and after BUILD changes
python3 tools/clang_tidy/run.py                    # all the repo's own code
python3 tools/clang_tidy/run.py projects/cyclone   # one project
python3 tools/clang_tidy/run.py --fix projects/cyclone   # apply clang-tidy's fixes
python3 tools/clang_tidy/run.py --checks='-*,modernize-use-nullptr' --fix   # one check only
```

It skips `projects/examples/png/src/pngtest.cpp` (libpng's own test program, kept as published). It prints each finding once (a header's findings aren't repeated per file that includes it), then a count per check, and exits 1 if there are any. It finds clang-tidy on your `PATH`, in Homebrew's LLVM, or at `$CLANG_TIDY`. If a header can't be found, it stops and asks you to rebuild instead of listing findings: Bazel removes a dependency's headers from its output when a later build doesn't need them, and a file analysed without its headers gives wrong findings. Rebuild what you're checking (`bazel build //...`, or one project's targets) and run it again. Its findings are advice, not part of the build: review fixes before committing them, and rebuild and test afterwards.

### Checking each commit

A git pre-commit hook in `tools/git_hooks/` checks that the C++ files you commit are formatted, then runs clang-tidy on them. Turn it on once per clone:

``` bash
git config core.hooksPath tools/git_hooks
```

Then each `git commit` checks the staged C++ files under `projects/`: it stops the commit if any isn't formatted as clang-format would write it (listing them; `python3 tools/clang_format/format.py` fixes them, and files in `.clang-format-ignore` pass), or if clang-tidy finds anything in them (in the projects the runner covers), listing the findings. Problems elsewhere don't block it. A staged header is checked through its project's source files, since clang-tidy only sees headers as part of a file that includes them. It takes about half a second for a source file and a few seconds for a header in a big project.

- It checks the files as they are on disk, so stage everything you mean to commit.
- `git commit --no-verify` skips it for one commit.
- Without clang-format installed, it skips the formatting check and says so. Without clang-tidy installed or a `compile_commands.json`, or with a stale build (headers missing), it says so and lets the commit through. A new source file isn't in `compile_commands.json` until you rerun `tools/compile_commands/refresh.py`; the hook tells you when it skips one.

### git blame past mechanical changes

`.git-blame-ignore-revs` lists commits that change many lines without changing what they do, such as renaming Waffles' `G` prefix to `W`, so `git blame` shows who last really changed each line. GitHub reads it itself; for a local checkout, turn it on once per clone:

``` bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

Add a commit to it when it's a rename or reformatting of that kind (its full hash, with its title as a comment above), in a commit of its own after it.

## Platform Notes

Development and testing happen on **macOS** (Apple Silicon, Xcode).

### Linux

Tested on 2026-10-21 with GCC 14 and with Clang 18 (`--config=clang`, on GCC 14's standard library, as Ubuntu sets it up), each on ARM64 in Docker: everything builds and all the tests pass, except the 3DS example, which needs Docker itself (not available inside the container). Results that are exact on one platform can differ slightly on another; see [Floating-point results across platforms](#floating-point-results-across-platforms).

To repeat the check from macOS (or anywhere with Docker), use [`tools/linux_check`](tools/linux_check/README.md):

``` bash
python3 tools/linux_check/check.py gcc     # or clang
```

It builds and tests the repo inside a Linux container and prints Bazel's summary. Give Docker enough memory (Colima: `colima start --memory 8`) or Bazel runs out.

Building with Clang on Linux yourself, add `--config=clang` (`bazel build --config=clang //...`) for the Clang-only checks (see the prerequisites in [Getting Started](#getting-started)); with GCC, nothing extra is needed. Bazelisk provides Bazel for x86-64 and ARM64 (including 64-bit Raspberry Pi OS). The windowing and OpenGL examples need the X11 and Mesa development packages:

``` bash
sudo apt install libxinerama-dev libxrandr-dev libxcursor-dev libxi-dev \
                 libglu1-mesa-dev libgl1-mesa-dev libegl1-mesa-dev
```

`Could NOT find OpenGL` means `libgl1-mesa-dev` is missing; `Could NOT find OpenGL (missing: EGL)` means `libegl1-mesa-dev` is. Package names can differ slightly between Ubuntu and Raspberry Pi OS.

### Windows

Not supported yet. Most libraries build there in principle, but nothing has been tested, and the OpenCV build (used by the camera example) only handles macOS and Linux library names so far. flexmath uses `__int128`, which MSVC lacks; [its README](projects/flexmath/README.md#portability) says what to change.

### Floating-point results across platforms

The same code gives slightly different floating-point results on different platforms, so an exact value recorded on one (a test's expected number, a hash of a rendered image) can fail on another. Within one build, results are exactly repeatable.

**Why they differ:**

- **Fused multiply-add.** Clang turns `a * b + c` into one fused instruction, rounded once (`-ffp-contract=on` is its default). GCC in strict ISO mode, as here (`-std=c++26`), keeps a separate multiply and add, rounded twice. Both are allowed by the language, but the last bit differs.
- **The maths library.** `sin`, `cos`, `exp`, `pow`, `atan2` and the like come from the platform (Apple's libm, glibc on Linux, Microsoft's CRT), and aren't required to be correctly rounded, so they can differ in the last bit. `sqrt` and the basic arithmetic are exact under IEEE 754 and don't vary.
- **Code generation** may also differ (the order of operations once vectorised, for example), though without `-ffast-math` compilers keep to the source's order.

Most code never notices a last-bit difference, but chaotic simulations amplify it. The Waffles jumper demo's Bullet simulation gives a jump height of 0.6288 with Apple clang and 0.6318 with GCC 14 on Linux, and 0.6229 with Apple clang with fused multiply-adds off: fusion alone changes the result, and the maths library (or code generation) accounts for the rest. Its test now checks the height against a range, and exact repeatability within a build.

**Making results match across platforms** (not done yet; it's the first item on [flexmath's roadmap](projects/flexmath/README.md#roadmap)):

1. Turn fusion off everywhere: `build --copt=-ffp-contract=off` in `.bazelrc`, for external libraries too (Bullet's arithmetic is what varies), not only `projects/`. Never use `-ffast-math`, which allows reordering.
2. Use maths functions that give the same result on every platform, instead of the system's: a correctly rounded library such as [CORE-MATH](https://core-math.gitlabpages.inria.fr/) (correct rounding has one right answer, so every platform agrees), or one implementation built from source everywhere, such as [OpenLibm](https://openlibm.org/). Code that calls `std::sin` and friends, Bullet included, has to be pointed at it, for example through a header that wraps the calls.
3. Check it by running the same simulation on macOS and on Linux (the GCC Docker image used for the Linux notes above) and comparing exact results, then pin an exact value in a test again.

Expect a small speed cost: unfused arithmetic is slightly slower, and correctly rounded functions can be slower than the system's.
