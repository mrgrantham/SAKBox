# VM check

Builds and tests the repo in a [Parallels](https://www.parallels.com) VM on a Mac: Ubuntu ARM64 with GCC or Clang, or Windows 11 ARM64 with clang-cl. On Ubuntu it's like the [Linux check](../linux_check/README.md)'s Docker containers, with two things they lack: a desktop, so the tests that open a window (tagged `display`) run, and a real MIDI system (ALSA's sequencer), so `MidiInputPadd_test` runs instead of skipping. On Windows it's the only check there is (see the top-level README's [Windows](../../README.md#windows) notes).

| File | What it is |
| --- | --- |
| `check.py` | Brings the VM up, shares the checkout with it, runs the build and tests there as the desktop's user, prints Bazel's summary, and puts the VM back as it was |
| `ubuntu_setup.sh` | Installs what the build needs in the Ubuntu VM (`--setup` runs it) |
| `ubuntu_run.sh` | Copies the checkout and runs `bazel test` in the Ubuntu VM |
| `windows_setup.ps1` | Installs what the build needs in the Windows VM (`--setup` runs it) |
| `windows_run.py` | Copies the checkout and runs `bazel test` in the Windows VM |

The summary is the Linux check's: [`tools/build_summary`](../build_summary/print_build_summary.sh) prints Bazel's status, the tests that didn't pass, Bazel's `ERROR:` lines and the compilers' errors.

## Setup

Both VMs need **Parallels Tools** installed in them (Parallels offers them when the VM is made; `check.py` runs commands through them) and someone logged in at their desktop. `--setup` also turns off Parallels' "pause when idle" for the VM, which otherwise pauses it during a long build.

**Ubuntu**

1. **A VM:** Ubuntu 24.04 for ARM64 (tested with 24.04.1 and Parallels Desktop 27). Give it enough for Bazel: 8 CPUs and 16 GB of memory were used (`prlctl set NAME --cpus 8 --memsize 16384`, with the VM shut down); Ubuntu's default 2 GB isn't enough.
2. **The tools in it:** `python3 tools/vm_check/check.py ubuntu --setup`. It installs, as the Linux check's Docker images have, GCC 14 and Clang 18 (with GCC 14's standard library), the X11, Mesa and ALSA packages from the top-level README's [Linux](../../README.md#linux) notes, Python, rsync, and [Bazelisk](https://github.com/bazelbuild/bazelisk) 1.25.0 (as `/usr/local/bin/bazelisk`, so a `bazel` already in the VM doesn't matter). It also makes the desktop user (the VM's first account) log in by itself at boot, on X11 rather than Wayland, so windows can open with no one at the VM, and turns off the screen's blanking after 15 idle minutes, so they're seen. Restart the VM once after it.

**Windows**

1. **A VM:** Windows 11 for ARM64 (tested with 11 Pro), with you logged in: the build runs as you, in your desktop's session, so the window tests can open windows.
2. **The tools in it:** `python3 tools/vm_check/check.py windows --setup`. It installs Git for Windows (its bash runs Bazel's shell steps), Python 3.14, Bazelisk 1.25.0 (`C:\tools\bazelisk.exe`), and Visual Studio 2022 Build Tools with MSVC's ARM64 libraries, the Windows SDK and Clang (clang-cl), several GB, so the first `--setup` takes a while. It also turns on **Developer Mode**, which lets an ordinary account make symbolic links: Bazel's runfiles and rules_python's tools are made of them.

Running `--setup` again is harmless: it skips what's there.

## Using it

``` bash
python3 tools/vm_check/check.py ubuntu gcc                        # build and test everything with GCC
python3 tools/vm_check/check.py ubuntu clang                      # ... with Clang (adds --config=clang)
python3 tools/vm_check/check.py windows                           # ... on Windows, with clang-cl
python3 tools/vm_check/check.py ubuntu gcc //projects/scratchpadd/...  # just these targets
python3 tools/vm_check/check.py ubuntu gcc --runs_per_test=20 //projects/scratchpadd/midi_input/...  # Bazel flags are passed on
python3 tools/vm_check/check.py windows --log                     # the last run's full output
python3 tools/vm_check/check.py windows --keep-running            # leave the VM running afterwards
```

It uses the one Parallels VM with that OS; with more than one, name it with `--vm NAME`. It starts or resumes the VM, and afterwards suspends or stops it again if that's how it was (`--keep-running` leaves it running, quicker for several runs). With no targets it builds and tests everything except the 3DS and DS apps (tagged `devkitpro`), which need Docker, and models from Blender and what uses them (tagged `blender`). On Windows, Bazel also skips what's marked as not for Windows (see the top-level README). It exits with Bazel's exit code.

- **Your checkout is safe.** `check.py` shares it with the VM read-only (a Parallels shared folder named `SAKBox`: `/media/psf/SAKBox` in Ubuntu, `\\Mac\SAKBox` in Windows; it adds the share if it isn't there). The VM copies it, leaving out what git ignores and `.git` (`~/sakbox-gcc`, `~/sakbox-clang`, `C:\sakbox\clang-cl`), and builds there. So it tests your working tree, uncommitted changes too.
- **Later runs are quick.** Each copy keeps its own Bazel cache in the VM (on Windows in `C:\b`, short for Windows' 260-character path limit). The first run downloads everything and builds from scratch.
- **Test times can be off** in a run just after the VM was resumed: Parallels sets the VM's clock forward as it catches up, and a test running then can show 30 s for 1.
- **The logs** stay in the VM beside each copy: `~/sakbox-gcc.log`, `C:\sakbox\clang-cl.log` (`--log` prints it), and the summary beside it.
- **The window tests** draw with the VM's OpenGL: in Ubuntu, Parallels' virtual GPU through Mesa (virgl), OpenGL 4.0; in Windows, Parallels' own driver. Both are enough for the scene layer's OpenGL 3.3.
- **To see a window** (a demo run in the VM), take the screenshot inside it: `prlctl capture` gave a black screen of Ubuntu while the VM's window on the Mac was hidden, and in Windows it captures the whole desktop. In Ubuntu, with ImageMagick (`apt install imagemagick`), `import -window ID file.png` as the desktop user, with `DISPLAY=:0` and `XAUTHORITY=/run/user/UID/gdm/Xauthority` (`xwininfo -root -tree` lists the windows' IDs). In Windows, copy the window's rectangle from the screen with .NET (`System.Drawing.Graphics.CopyFromScreen`), finding it by its process's `MainWindowTitle`.

Apple silicon runs only ARM guests, so this covers Linux and Windows on ARM64; x86-64 Linux is the Linux check's `--x86-64`.

**How it runs things in the VM.** `prlctl exec` now and then fails with `PrlJob_GetResult: Invalid argument` before running anything, or never returns; `check.py` gives each `prlctl` 2 minutes and tries again. Long commands are the likeliest to hit it, so the build (and `--setup`) runs on its own in the VM, writes its exit code last, and `check.py` checks on it every 10 seconds:

- **Ubuntu:** as a systemd service (`sakbox-gcc`, `sakbox-clang`, `sakbox-setup`; `systemd-run`, as the desktop user for the build). Started with `sudo` or `setsid` from `prlctl exec` instead, it was now and then killed as `prlctl exec` returned. If the service stops without writing its code, `check.py` says so; `journalctl -u sakbox-gcc` in the VM says why.
- **Windows:** as a process of its own (`Start-Process`), the build as you through `prlctl exec --current-user`, which runs in your desktop's session, and `--setup` as SYSTEM. Commands go to PowerShell base64-encoded (`-EncodedCommand`): `prlctl exec` joins its arguments into one line, which breaks quoting. (A scheduled task with an interactive logon, the usual way to run as the logged-in user, never started in this VM.)
