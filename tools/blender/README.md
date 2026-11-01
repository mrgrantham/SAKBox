# Blender for the build

[Blender](https://www.blender.org) 5.2.3 LTS, which [`tools/assets`](../assets/README.md)' rules run headless to export models. Bazel downloads it the first time a model is built, for the machine Bazel runs on, and keeps it in its cache; nothing is installed anywhere else.

| Machine | Download | Notes |
| --- | --- | --- |
| macOS, Apple silicon | `blender-5.2.3-macos-arm64.dmg`, 346 MB (907 MB unpacked) | Bazel's own archive download can't open a `.dmg`, so `blender.bzl` mounts it (`hdiutil`) and copies `Blender.app` out (an APFS clone where it can) |
| Linux, x86-64 | `blender-5.2.3-linux-x64.tar.xz`, 385 MB | Needs X11, Wayland and GL client libraries even headless: `libxkbcommon0 libxkbcommon-x11-0 libsm6 libice6 libxi6 libxrender1 libxfixes3 libxxf86vm1 libgl1 libegl1 libwayland-client0 libdecor-0-0` (the [Linux check](../linux_check/README.md)'s images have them) |
| Intel Macs, Linux on ARM | none: blender.org doesn't build Blender for them | A model fails to build, saying so |

Each is pinned by the SHA-256 blender.org publishes. To use another Blender (an installed one, or on a machine blender.org has none for), build with `--action_env=BLENDER=/path/to/blender`.

- **How it runs** (`run_blender.sh`): `blender --background --factory-startup model.blend --python export_gltf.py -- model.glb`, with its home and settings in the action's own temporary folder, never the user's. The first start takes about 8 s; an export about 1 s.
- **Exports are repeatable:** the same `.blend` exported twice gave byte-identical `.glb` files, and Linux x86-64 gave the same bytes as macOS, so Bazel's cache holds, and one shared cache could serve both.
- **Blender is GPL-2.0-or-later**, run as a separate program; nothing in the repo links to it.

## Updating Blender

1. Pick a release at [download.blender.org/release](https://download.blender.org/release/), and its `.sha256` file there.
2. In `blender.bzl`, change `_VERSION` and the two SHA-256s.
3. Build a model (`bazel build //projects/examples/blender_model:all`) and check the cooked models still match: a new exporter can change them.
