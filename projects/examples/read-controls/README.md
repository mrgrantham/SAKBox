# 3DS read-controls

Nintendo 3DS homebrew: libctru's "read controls" sample by Aurelio Mannara. It prints the name of each button as it goes down, is held and comes up, and shows the circle pad's position, on the top screen.

## Building

Needs Docker running: on macOS, `brew install colima docker` then `colima start`; on Linux or Windows, see [`tools/devkitpro`](../../../tools/devkitpro/README.md).

```
bazel build //projects/examples/read-controls
```

This builds `bazel-bin/projects/examples/read-controls/read-controls.3dsx` (plus `.smdh` and `.elf`) with the devkitPro toolchain pinned in `//tools/devkitpro`, using this folder's devkitPro `Makefile`.

## Testing in an emulator

[Azahar](https://github.com/azahar-emu/azahar) is the maintained 3DS emulator (the successor to Citra and Lime3DS). Homebrew like this runs without any Nintendo system files.

Bazel downloads Azahar and opens the app in it:

```
bazel run //projects/examples/read-controls
```

Nothing needs installing: Bazel fetches a pinned Azahar for your machine the first time, and Azahar keeps its settings and SD card in the repo's `.azahar/` folder. Closing Azahar ends the command. See [`tools/azahar`](../../../tools/azahar/README.md) for what's supported where and how to use another Azahar.

When it's running the top screen shows:

```
Press Start to exit.
CirclePad position:
0000; 0000
```

### Controls

Azahar's default keyboard mapping (see **Settings → Controls**):

| 3DS | Key |
| --- | --- |
| A / B / X / Y | A / S / Z / X |
| D-pad | T / G / F / H |
| Circle pad | Arrow keys |
| L / R | Q / W |
| ZL / ZR | 1 / 2 |
| Start / Select | M / N |

Each press prints lines like `KEY_A down`, `KEY_A held`, `KEY_A up`; holding an arrow key changes the circle pad position (up to about ±155).

**Start (M) quits the app**, as on a real 3DS where it returns to the Homebrew Launcher. In the emulator that stops emulation and Azahar goes back to its empty game list; it isn't a crash. Relaunch to keep testing, and try Select (N) and the other buttons before Start.

## On a real 3DS

Copy `read-controls.3dsx` to the SD card's `/3ds/` folder and start it from the Homebrew Launcher. Or install `read-controls.cia` with FBI (on a 3DS with custom firmware) and start it from the HOME Menu.
