# Linux check

Builds and tests the repo on Linux, in a Docker container, with GCC or Clang, so a change can be checked on Linux from a Mac. It's how the top-level README's [Linux](../../README.md#linux) notes were tested.

| File | What it is |
| --- | --- |
| `check.py` | Builds the image, runs the build and tests in it, and prints Bazel's summary |
| `Dockerfile.gcc` | GCC 14 (the official `gcc:14` image) |
| `Dockerfile.clang` | Clang 18 on Ubuntu 24.04, with GCC 14's standard library (libstdc++), as Clang on Ubuntu uses by default |

Both images add the X11, Mesa and ALSA packages the README's Linux notes list, Python, and [Bazelisk](https://github.com/bazelbuild/bazelisk) 1.25.0, which runs the Bazel version in `.bazelversion`.

## Setup

Docker, with enough memory for Bazel: on macOS, `brew install colima docker`, then `colima start --memory 8` (Colima's default of 2 GB isn't enough; Bazel's server is killed partway through). Colima keeps the setting for later starts.

## Using it

``` bash
python3 tools/linux_check/check.py gcc                       # build and test everything with GCC
python3 tools/linux_check/check.py clang                     # ... with Clang (adds --config=clang)
python3 tools/linux_check/check.py gcc //projects/waffles/...  # just these targets
python3 tools/linux_check/check.py gcc --log                 # the last run's full output
python3 tools/linux_check/check.py gcc --x86-64 //projects/waffles/demos/jumper/...  # on x86-64
```

With no targets it builds and tests everything except the 3DS apps, which need Docker themselves, and that isn't available inside the container. `devkitpro_3ds_app` tags them `devkitpro`, and the check passes `--build_tag_filters=-devkitpro`, so new 3DS apps are left out too. It prints Bazel's summary: whether the build completed, how many tests ran and passed, every test that didn't pass, Bazel's `ERROR:` lines and the compilers' errors. It exits with Bazel's exit code.

- **Your checkout is safe.** It's mounted read-only and copied inside the container, so the Linux build can't change your Bazel output links (`bazel-bin` and so on) or the lockfile.
- **Later runs are quick.** Each compiler keeps Bazel's downloads and cache in its own Docker volume (`sakbox-linux-gcc-cache`, `sakbox-linux-clang-cache`). The first run downloads everything and builds from scratch, which takes a while. `docker volume rm` on a volume starts that compiler afresh.
- **The images** are built from the Dockerfiles on each run, which is instant once Docker has them cached. They're tagged `sakbox-linux-gcc` and `sakbox-linux-clang`.

The containers run on whatever architecture Docker does: ARM64 on Apple silicon (tested), x86-64 on an Intel machine (not tried). **`--x86-64`** runs them as x86-64 on Apple silicon instead, under emulation (Colima runs x86-64 containers out of the box), with their own images and caches (`sakbox-linux-gcc-x86-64`, `...-cache`). It's many times slower, so name the targets: it's for checking that results match across processors, as the jumper's pinned jump height does. It needs Colima's Rosetta emulation (`colima start --vm-type vz --vz-rosetta`, kept for later starts): under its default emulation (QEMU) GCC and Clang crashed with segmentation faults partway through building. Rosetta builds and runs them, much faster (see the top-level README's [Floating-point results across platforms](../../README.md#floating-point-results-across-platforms)).
