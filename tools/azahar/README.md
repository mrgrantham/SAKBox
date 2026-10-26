# Azahar for `bazel run`

[Azahar](https://github.com/azahar-emu/azahar) is the maintained Nintendo 3DS emulator (the successor to Citra and Lime3DS). Homebrew runs in it without any Nintendo system files. Bazel downloads it and sets it up, so `bazel run` on any 3DS app (a `devkitpro_3ds_app` target, see [`tools/devkitpro`](../devkitpro/README.md)) builds the app and opens it in Azahar:

```
bazel run //projects/examples/read-controls
bazel run //projects/scratchpadd/audio/3ds/selftest:scratchpadd-audio-selftest
```

The command finishes when Azahar is closed.

## What Bazel does

- **Installs it.** `MODULE.bazel` pins Azahar 2126.2 by checksum, one download per kind of machine, and [`BUILD`](BUILD) picks the one for the machine Bazel runs on. Only that one is downloaded, the first time a 3DS app is built (about 100 MB), and Bazel keeps it in its cache. Nothing is installed in `/Applications` or anywhere else on the machine.
- **Keeps its own settings.** Azahar runs "portable", with its settings, saves, logs and SD card in `.azahar/user` in the repo (git-ignored), never in `~/Library/Application Support/Azahar` or `~/.local/share`. So it doesn't change, or depend on, an Azahar you installed yourself. A 3DS app's files on the SD card are in `.azahar/user/sdmc`: the audio self-test, for example, writes `scratchpadd-audio-selftest.txt` there. Delete `.azahar` to start afresh.
- **On macOS** Azahar looks for that `user` folder next to `Azahar.app`, so the launcher keeps a copy of Bazel's `Azahar.app` in `.azahar` (an APFS clone, made in an instant and taking no extra space), remade when the pinned version changes. It starts it with `open`, as Azahar asks: run any other way, Azahar first stops to warn that some features (the camera) may be missing.

| Machine | Azahar | Status |
| --- | --- | --- |
| macOS, Apple Silicon | `azahar-macos-arm64-2126.2.zip` | Tested |
| macOS, Intel | `azahar-macos-x86_64-2126.2.zip` | Download checked; not run |
| Linux, x86-64 | `azahar.AppImage` (run without FUSE) | Download checked; not run (needs a desktop) |
| Linux on ARM, Windows | none published that Bazel can use | `bazel run` says so; set `AZAHAR` (below) |

To use another Azahar (an installed one, a newer version, or on a machine Bazel has none for), set `AZAHAR` to its program, e.g. `AZAHAR=/Applications/Azahar.app/Contents/MacOS/azahar bazel run ...`. It still keeps its settings in `.azahar`.

## Updating Azahar

1. Pick a release from [github.com/azahar-emu/azahar/releases](https://github.com/azahar-emu/azahar/releases) (not a pre-release).
2. In `MODULE.bazel`, change the version in the three `azahar_*` URLs and `strip_prefix`es, and put in each file's sha256 (shown on the release page for each file).
3. `bazel run` a 3DS app to check it.

Azahar is GPL-2.0. Bazel only downloads and runs it as a separate program; nothing in the repo links to it.
