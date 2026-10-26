# melonDS for `bazel run`

[melonDS](https://melonds.kuribo64.net) is a Nintendo DS emulator. Homebrew runs in it without any Nintendo files: it boots a `.nds` straight away with its own free BIOS and firmware. Bazel downloads it and sets it up, so `bazel run` on any DS app (a `devkitpro_nds_app` target, see [`tools/devkitpro`](../devkitpro/README.md)) builds the app and opens it in melonDS:

```
bazel run //projects/scratchpadd/ds/demo:scratchpadd-ds-demo
bazel run //projects/cyclone/ds:cyclone-ds-ragdoll
```

The command finishes when melonDS is closed.

## What Bazel does

- **Installs it.** `MODULE.bazel` pins melonDS 1.1 by checksum, one download per kind of machine, and [`BUILD`](BUILD) picks the one for the machine Bazel runs on. Only that one is downloaded, the first time a DS app is run, and Bazel keeps it in its cache. Nothing is installed in `/Applications` or anywhere else on the machine.
- **Keeps its own settings**, in `.melonds` in the repo (git-ignored), never in `~/Library/Preferences` or `~/.config`, so it doesn't change, or depend on, a melonDS you installed yourself. melonDS reads a `portable` folder next to `melonDS.app` on macOS, so the launcher ([`run_in_melonds.sh`](../devkitpro/run_in_melonds.sh)) keeps a copy of Bazel's `melonDS.app` in `.melonds` (an APFS clone, made in an instant and taking no extra space) beside `.melonds/portable`; on Linux it points `XDG_CONFIG_HOME` there. Delete `.melonds` to start afresh.

| Machine | melonDS | Status |
| --- | --- | --- |
| macOS, Apple Silicon and Intel | `melonDS-1.1-macOS-universal.zip` (one app for both) | Tested on Apple Silicon |
| Linux, x86-64 | `melonDS-1.1-appimage-x86_64.zip` (run without FUSE) | Download checked; not run (needs a desktop) |
| Linux on ARM, Windows | not set up | `bazel run` says so; set `MELONDS` (below) |

To use another melonDS (an installed one, a newer version, or on a machine Bazel has none for), set `MELONDS` to its program, e.g. `MELONDS=/Applications/melonDS.app/Contents/MacOS/melonDS bazel run ...`. It still keeps its settings in `.melonds`.

## Updating melonDS

1. Pick a release from [github.com/melonDS-emu/melonDS/releases](https://github.com/melonDS-emu/melonDS/releases).
2. In `MODULE.bazel`, change the version in the two `melonds_*` URLs, and put in each file's sha256.
3. `bazel run` a DS app to check it.

melonDS is GPL-3.0. Bazel only downloads and runs it as a separate program; nothing in the repo links to it.
