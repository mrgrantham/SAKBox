# devkitPro for Nintendo 3DS

Builds Nintendo 3DS homebrew with [devkitPro](https://devkitpro.org)'s toolchain, managed by Bazel: the compiler and every library come from devkitPro's official Docker image, pinned by digest in [`defs.bzl`](defs.bzl). Nothing is installed in `/opt` and no `sudo` is needed.

| File | What it is |
| --- | --- |
| `defs.bzl` | `DEVKITARM_IMAGE` (the pinned image) and the `devkitpro_3ds_app` rule |
| `build_3ds.sh` | Runs devkitPro's `make` inside the image. Sources go in and the built files come out through `tar` on stdin/stdout, so no folders are shared with the container. |

The pinned image is `devkitpro/devkitarm:20260610`: devkitARM r68 (GCC 16.1), libctru 2.7.0, citro2d 1.7.0, citro3d 1.7.1, 3dstools 1.3.1 and the `3ds-*` libraries (zlib, curl, freetype, box2d, ...). It is built for both arm64 (Apple Silicon, runs natively) and amd64.

## Setup

The image contains Linux programs, so you need a container runtime. On macOS:

```
brew install colima docker
colima start
```

`colima start` is needed again after a reboot; `brew services start colima` starts it at login instead. Docker Desktop works too. On Linux, install Docker.

That's all. The first build pulls the image (about 520 MB) into the runtime's VM; later builds reuse it, and Bazel caches the results like any other action.

If Docker isn't running, 3DS targets fail with `Docker isn't running. Start it (colima start) and build again.` Everything else in the repo still builds.

## Using it

```
load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

devkitpro_3ds_app(
    name = "my-app",
    srcs = ["Makefile"] + glob(["source/**", "include/**", "romfs/**", "gfx/**"]),
)
```

`srcs` must include a devkitPro 3DS `Makefile` (the standard template from devkitPro's 3ds examples) with the sources at the paths it expects. The rule builds `my-app.3dsx` (the app), `my-app.smdh` (its title and icon) and `my-app.elf` (with debug symbols) in `bazel-bin`. `app_name` sets a different output name.

See [`projects/examples/read-controls`](../../projects/examples/read-controls/README.md) for an example, including how to run the result in an emulator.

## Updating the toolchain

1. Pick a dated tag from [hub.docker.com/r/devkitpro/devkitarm/tags](https://hub.docker.com/r/devkitpro/devkitarm/tags).
2. `docker pull devkitpro/devkitarm:<tag>`, then `docker inspect --format '{{index .RepoDigests 0}}' devkitpro/devkitarm:<tag>` to get its digest.
3. Put the digest in `DEVKITARM_IMAGE` in `defs.bzl`, update the version comment above it, and rebuild the 3DS targets.

To see exactly what an image contains: `docker run --rm <image> dkp-pacman -Q`.

## Why Docker

devkitPro also publishes macOS packages, installed with their `pacman` into `/opt/devkitpro`. Bazel can't pin those reliably: devkitPro removes old package versions from its server when new ones are published (so pinned URLs stop working), asks that its packages not be used for automated builds (pointing to these Docker images instead), and its server is behind Cloudflare, which has blocked some networks outright (HTTP 403). Docker Hub's images are immutable once pinned by digest.
