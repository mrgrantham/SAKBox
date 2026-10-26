# devkitPro for Nintendo 3DS and DS

Builds Nintendo 3DS and DS homebrew with [devkitPro](https://devkitpro.org)'s toolchain, managed by Bazel: the compiler and every library come from devkitPro's official Docker image, pinned by digest in [`defs.bzl`](defs.bzl). Nothing is installed in `/opt` and no `sudo` is needed.

| File | What it is |
| --- | --- |
| `defs.bzl` | `DEVKITARM_IMAGE` (the pinned image), and the `devkitpro_3ds_app` and `devkitpro_nds_app` rules |
| `run_in_azahar.sh`, `run_in_melonds.sh` | What `bazel run` runs: opens a 3DS app in Azahar (see [`tools/azahar`](../azahar/README.md)), or a DS app in melonDS (see [`tools/melonds`](../melonds/README.md)) |
| `build_nds.py` | Builds a DS app inside the image: devkitPro's `make`, which makes the `.nds` |
| `build_3ds.py` | Builds an app inside the image: devkitPro's `make`, then the CIA (bannertool's banner, makerom) |
| `build_cia_tools.py` | Builds makerom and bannertool from source inside the image (the `cia_tools` target) |
| `devkitpro_image.py` | Runs a script in the image for the two above. Files go in and results come out as tar streams on stdin/stdout, so no folders are shared with the container and nothing it writes ends up owned by root. Python, so it runs the same on every host. |

The pinned image is `devkitpro/devkitarm:20260610`: devkitARM r68 (GCC 16.1), libctru 2.7.0, citro2d 1.7.0, citro3d 1.7.1, 3dstools 1.3.1 and the `3ds-*` libraries (zlib, curl, freetype, box2d, ...), and for the DS libnds on calico, with ndstool. It is built for both arm64 (Apple Silicon, runs natively) and amd64.

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

`srcs` must include a devkitPro 3DS `Makefile` (the standard template from devkitPro's 3ds examples) with the sources at the paths it expects. The rule builds, in `bazel-bin`:

- `my-app.3dsx`: the app, for the Homebrew Launcher (copy it to the SD card's `/3ds/` folder).
- `my-app.cia`: the same app as a title to install to the HOME Menu, with [FBI](https://github.com/lifehackerhansol/FBI) on a 3DS with custom firmware, or **File → Install CIA** (or `azahar -i`) in Azahar. It has its own icon (the `.smdh`) and a HOME Menu banner.
- `my-app.smdh` (its title and icon) and `my-app.elf` (with debug symbols).

`app_name` sets a different output name. For the CIA:

| Attribute | What | Default |
| --- | --- | --- |
| `unique_id` | The title's ID, `0xF0000`-`0xF7FFF` for homebrew. Two CIAs with the same ID replace each other when installed. | One worked out from the target's label, so each app has its own |
| `product_code` | Its product code | `CTR-H-` and the last four digits of the ID |
| `banner_image` | The HOME Menu banner, a 256x128 PNG | A plain blue one |
| `banner_sound` | The banner's sound, a WAV of up to 3 seconds | A short chime |

The CIA is unencrypted, made the standard way for homebrew: [makerom](https://github.com/3DSGuy/Project_CTR) (from Project_CTR) builds it from the `.elf` with the settings file (`template.rsf`) and boot logo of [Steveice10's buildtools](https://github.com/Steveice10/buildtools), and [bannertool](https://github.com/diasurgical/bannertool) makes the banner. All three are MIT, pinned in `MODULE.bazel`, and built from source inside the devkitPro image by `//tools/devkitpro:cia_tools` the first time a 3DS app is built (Bazel then caches them), so they run on whatever processor Docker does. bannertool is compiled directly rather than with its own Makefile, which only knows x86 hosts.

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

**DS apps** build the same way, with a devkitPro DS `Makefile` (the template from devkitPro's `examples/nds`):

```
load("//tools/devkitpro:defs.bzl", "devkitpro_nds_app")

devkitpro_nds_app(
    name = "my-ds-app",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {...},  # as for a 3DS app
)
```

It builds `my-ds-app.nds` (for a flash card, or an emulator) and `my-ds-app.elf`, and `bazel run` opens it in melonDS. See [the ScratchPadd DS demo](../../projects/scratchpadd/ds/demo/BUILD), and [`projects/scratchpadd/ds/platform`](../../projects/scratchpadd/ds/platform/BUILD) for what C++ code that uses `std::atomic` or `std::chrono`'s clocks needs on the DS.

Every app is tagged `devkitpro`, so a build that can't run Docker can leave them all out with `--build_tag_filters=-devkitpro` (as [`tools/linux_check`](../linux_check/README.md) does).

Files other rules generate can be staged as well as sources, at their package's paths (the DS and 3DS ScratchPadd demos stage sounds rendered when they're built). devkitPro's template `Makefile` compiles C++ with `-fno-rtti -fno-exceptions`; code that needs either keeps them in its own `Makefile`, as the [ScratchPadd 3DS demo](../../projects/scratchpadd/3ds/demo/Makefile) does (ScratchPadd uses both).

`bazel run` on the target builds the app and opens it in the Azahar emulator, which Bazel downloads and sets up (see [`tools/azahar`](../azahar/README.md)). See [`projects/examples/read-controls`](../../projects/examples/read-controls/README.md) for an example.

## Updating the toolchain

1. Pick a dated tag from [hub.docker.com/r/devkitpro/devkitarm/tags](https://hub.docker.com/r/devkitpro/devkitarm/tags).
2. `docker pull devkitpro/devkitarm:<tag>`, then `docker inspect --format '{{index .RepoDigests 0}}' devkitpro/devkitarm:<tag>` to get its digest.
3. Put the digest in `DEVKITARM_IMAGE` in `defs.bzl`, update the version comment above it, and rebuild the 3DS targets.

To see exactly what an image contains: `docker run --rm <image> dkp-pacman -Q`.

## Why Docker

devkitPro also publishes macOS packages, installed with their `pacman` into `/opt/devkitpro`. Bazel can't pin those reliably: devkitPro removes old package versions from its server when new ones are published (so pinned URLs stop working), asks that its packages not be used for automated builds (pointing to these Docker images instead), and its server is behind Cloudflare, which has blocked some networks outright (HTTP 403). Docker Hub's images are immutable once pinned by digest.
