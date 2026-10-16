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

Azahar publishes builds for macOS, Linux and Windows. The steps below are for macOS (tested); on Linux or Windows, download the build for your system from [its releases](https://github.com/azahar-emu/azahar/releases) and open `read-controls.3dsx` with **File → Load File…** (or pass its full path on the command line). The controls and behaviour below are the same everywhere.

### Installing Azahar on macOS

It isn't in Homebrew. Download the build for your Mac from [its releases](https://github.com/azahar-emu/azahar/releases) (`azahar-macos-arm64-<version>.zip` for Apple Silicon), unzip it and move `Azahar.app` to `/Applications`. For example (tested with 2126.2):

```
cd ~/Downloads
curl -LO https://github.com/azahar-emu/azahar/releases/download/2126.2/azahar-macos-arm64-2126.2.zip
shasum -a 256 azahar-macos-arm64-2126.2.zip   # compare with the sha256 on the release page
unzip azahar-macos-arm64-2126.2.zip
mv azahar-macos-arm64-2126.2/Azahar.app /Applications/
```

The app isn't notarized by Apple. Downloaded with `curl` it opens normally; if it was downloaded with a browser and macOS refuses to open it, right-click it and choose **Open**.

### Running the app (macOS)

```
open -a Azahar --args ~/Programming_Projects/SAKBox/bazel-bin/projects/examples/read-controls/read-controls.3dsx
```

Use the full path: it works from any folder. (A path relative to the current folder fails when run from elsewhere, and Azahar reports a missing file as "The application file format not supported".) You can also use **File → Load File…** in Azahar.

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

Copy `read-controls.3dsx` to the SD card's `/3ds/` folder and start it from the Homebrew Launcher.
