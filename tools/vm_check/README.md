# VM check

Builds and tests the repo in a [Parallels](https://www.parallels.com) VM on a Mac: Ubuntu ARM64, with GCC or Clang. It's like the [Linux check](../linux_check/README.md)'s Docker containers, with two things they lack: a desktop, so the tests that open a window (tagged `display`) run, and a real MIDI system (ALSA's sequencer), so `MidiInputPadd_test` runs instead of skipping.

| File | What it is |
| --- | --- |
| `check.py` | Brings the VM up, shares the checkout with it, runs the build and tests there, prints Bazel's summary, and puts the VM back as it was |
| `ubuntu_setup.sh` | Installs what the build needs in the VM (`--setup` runs it) |
| `ubuntu_run.sh` | Copies the checkout and runs `bazel test` in the VM, as its desktop user |

The summary is the Linux check's: [`tools/build_summary`](../build_summary/print_build_summary.sh) prints Bazel's status, the tests that didn't pass, Bazel's `ERROR:` lines and the compilers' errors.

## Setup

1. **A VM:** Ubuntu 24.04 for ARM64 in Parallels Desktop (tested with 24.04.1 and Parallels Desktop 27), with **Parallels Tools** installed in it (Parallels offers them when the VM is made; `check.py` needs them to run commands in it). Give it enough for Bazel: 8 CPUs and 16 GB of memory were used (`prlctl set NAME --cpus 8 --memsize 16384`, with the VM shut down); Ubuntu's default 2 GB isn't enough.
2. **The tools in it:** `python3 tools/vm_check/check.py ubuntu --setup`. It installs, as the Linux check's Docker images have, GCC 14 and Clang 18 (with GCC 14's standard library), the X11, Mesa and ALSA packages from the top-level README's [Linux](../../README.md#linux) notes, Python, rsync, and [Bazelisk](https://github.com/bazelbuild/bazelisk) 1.25.0 (as `/usr/local/bin/bazelisk`, so a `bazel` already in the VM doesn't matter). It also makes the desktop user (the VM's first account) log in by itself at boot, on X11 rather than Wayland, so windows can open with no one at the VM, and turns off the screen's blanking after 15 idle minutes, so they're seen. Restart the VM once after it.

Running `--setup` again is harmless.

## Using it

``` bash
python3 tools/vm_check/check.py ubuntu gcc                        # build and test everything with GCC
python3 tools/vm_check/check.py ubuntu clang                      # ... with Clang (adds --config=clang)
python3 tools/vm_check/check.py ubuntu gcc //projects/scratchpadd/...  # just these targets
python3 tools/vm_check/check.py ubuntu gcc --runs_per_test=20 //projects/scratchpadd/midi_input/...  # Bazel flags are passed on
python3 tools/vm_check/check.py ubuntu gcc --log                  # the last run's full output
python3 tools/vm_check/check.py ubuntu gcc --keep-running         # leave the VM running afterwards
```

It uses the one Parallels VM whose OS is Ubuntu; with more than one, name it with `--vm NAME`. It starts or resumes the VM, and afterwards suspends or stops it again if that's how it was (`--keep-running` leaves it running, quicker for several runs). With no targets it builds and tests everything except the 3DS and DS apps (tagged `devkitpro`), which need Docker, and models from Blender and what uses them (tagged `blender`), as blender.org has no Blender for Linux on ARM. It exits with Bazel's exit code.

- **Your checkout is safe.** `check.py` shares it with the VM read-only (a Parallels shared folder named `SAKBox`, at `/media/psf/SAKBox` in the VM; it adds the share if it isn't there). The VM copies it to `~/sakbox-gcc` or `~/sakbox-clang`, leaving out what git ignores and `.git`, and builds there. So it tests your working tree, uncommitted changes too.
- **Later runs are quick.** Each compiler's copy keeps its own Bazel cache in the VM. The first run downloads everything and builds from scratch.
- **Test times can be off** in a run just after the VM was resumed: Parallels sets the VM's clock forward as it catches up, and a test running then can show 30 s for 1.
- **The logs** stay in the VM beside each copy: `~/sakbox-gcc.log` (`--log` prints it), `~/sakbox-gcc.summary`.
- **The window tests** draw with the VM's OpenGL: Parallels' virtual GPU through Mesa (virgl), OpenGL 4.0, enough for the scene layer's OpenGL 3.3.
- **To see a window** (a demo run in the VM), take the screenshot inside it: `prlctl capture` gave a black screen while the VM's window on the Mac was hidden. With ImageMagick in the VM (`apt install imagemagick`), `import -window ID file.png` as the desktop user, with `DISPLAY=:0` and `XAUTHORITY=/run/user/UID/gdm/Xauthority` (`xwininfo -root -tree` lists the windows' IDs).

Apple silicon runs only ARM guests, so this covers Linux on ARM64; x86-64 Linux is the Linux check's `--x86-64`.

`prlctl exec` now and then fails with `PrlJob_GetResult: Invalid argument` before running anything; `check.py` tries again. Long commands are more likely to hit it, so the build (and `--setup`) runs on its own in the VM, as a systemd service (`sakbox-gcc`, `sakbox-clang`, `sakbox-setup`), and `check.py` checks on it every 10 seconds until it's written its exit code. (Started with `sudo` or `setsid` from `prlctl exec` instead, it was now and then killed as `prlctl exec` returned.) If the service stops without writing one, `check.py` says so; `journalctl -u sakbox-gcc` in the VM says why.
