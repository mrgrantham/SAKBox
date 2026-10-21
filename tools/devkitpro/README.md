# devkitPro for Nintendo 3DS

Builds Nintendo 3DS homebrew with [devkitPro](https://devkitpro.org)'s toolchain, managed by Bazel: the compiler and every library come from devkitPro's official Docker image, pinned by digest in [`defs.bzl`](defs.bzl). Nothing is installed in `/opt` and no `sudo` is needed.

| File | What it is |
| --- | --- |
| `defs.bzl` | `DEVKITARM_IMAGE` (the pinned image) and the `devkitpro_3ds_app` rule |
| `build_3ds.py` | Runs devkitPro's `make` inside the image. Sources (each with the path the rule chose for it) go in and the built files come out as tar streams on stdin/stdout, so no folders are shared with the container and nothing it writes ends up owned by root. Python, so it runs the same on every host. |

The pinned image is `devkitpro/devkitarm:20260610`: devkitARM r68 (GCC 16.1), libctru 2.7.0, citro2d 1.7.0, citro3d 1.7.1, 3dstools 1.3.1 and the `3ds-*` libraries (zlib, curl, freetype, box2d, ...). It is built for both arm64 (Apple Silicon, runs natively) and amd64.

## Setup

The image contains Linux programs, so you need a container runtime. The output is the same whichever you use: the `.3dsx` built on macOS and on Linux is byte-for-byte identical.

| Host | Runtime | Status |
| --- | --- | --- |
| macOS | `brew install colima docker`, then `colima start` (or Docker Desktop) | Tested (Apple Silicon) |
| Linux | Docker Engine; your user needs to be in the `docker` group (or use rootless Docker) so builds can reach it without `sudo` | Build step tested on Linux (ARM, in a container); a full Bazel build on a Linux machine hasn't been tried |
| Windows | Docker Desktop (WSL2 backend) | Untested |

On macOS, `colima start` is needed again after a reboot; `brew services start colima` starts it at login instead.

The first build pulls the image (a 520 MB download, about 2 GB unpacked) into the runtime; later builds reuse it, and Bazel caches the results like any other action.

If Docker isn't running, 3DS targets fail with `Docker isn't running. Start it (colima start, or Docker Desktop) and build again.` Everything else in the repo still builds.

## Using it

```
load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

devkitpro_3ds_app(
    name = "my-app",
    srcs = ["Makefile"] + glob(["source/**", "include/**", "romfs/**", "gfx/**"]),
)
```

`srcs` must include a devkitPro 3DS `Makefile` (the standard template from devkitPro's 3ds examples) with the sources at the paths it expects. The rule builds `my-app.3dsx` (the app), `my-app.smdh` (its title and icon) and `my-app.elf` (with debug symbols) in `bazel-bin`. `app_name` sets a different output name.

`extra_srcs` brings in sources from other packages or external repositories, each label mapped to the folder its files go in. Files keep their paths within their own package, under that folder, and the `Makefile` lists the folders like any other (`SOURCES`, `INCLUDES`). For example, the [ScratchPadd audio self-test](../../projects/scratchpadd/audio/3ds/selftest/BUILD) builds the audio core and miniaudio from where they live:

```
devkitpro_3ds_app(
    name = "scratchpadd-audio-selftest",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/scratchpadd/audio:audio_core_sources": "scratchpadd_audio",  # core/src/..., core/include/...
        "@miniaudio//:sources": "third_party/miniaudio",                          # miniaudio.c, miniaudio.h
    },
)
```

Every app is tagged `devkitpro`, so a build that can't run Docker can leave them all out with `--build_tag_filters=-devkitpro` (as [`tools/linux_check`](../linux_check/README.md) does).

Only source files can be staged (not files other rules generate), and a C++ library built this way must suit devkitPro's 3DS `Makefile`, which compiles with `-fno-rtti -fno-exceptions`.

See [`projects/examples/read-controls`](../../projects/examples/read-controls/README.md) for an example, including how to run the result in an emulator.

## Updating the toolchain

1. Pick a dated tag from [hub.docker.com/r/devkitpro/devkitarm/tags](https://hub.docker.com/r/devkitpro/devkitarm/tags).
2. `docker pull devkitpro/devkitarm:<tag>`, then `docker inspect --format '{{index .RepoDigests 0}}' devkitpro/devkitarm:<tag>` to get its digest.
3. Put the digest in `DEVKITARM_IMAGE` in `defs.bzl`, update the version comment above it, and rebuild the 3DS targets.

To see exactly what an image contains: `docker run --rm <image> dkp-pacman -Q`.

## Why Docker

devkitPro also publishes macOS packages, installed with their `pacman` into `/opt/devkitpro`. Bazel can't pin those reliably: devkitPro removes old package versions from its server when new ones are published (so pinned URLs stop working), asks that its packages not be used for automated builds (pointing to these Docker images instead), and its server is behind Cloudflare, which has blocked some networks outright (HTTP 403). Docker Hub's images are immutable once pinned by digest.
