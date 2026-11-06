"""Builds and tests the repo on Linux, in Docker, with GCC or Clang.

The checkout is mounted read-only and copied inside the container, so the
Linux build can't touch your Bazel output links or lockfile. Each compiler
keeps Bazel's downloads and cache in its own Docker volume
(sakbox-linux-gcc-cache, sakbox-linux-clang-cache), so later runs are quick.
It prints Bazel's summary: the build's result, any test that didn't pass,
and the errors. The full output of the last run is kept in the volume
(--log prints it).

Usage:
  python3 tools/linux_check/check.py gcc|clang [--x86-64] [TARGET ...]
  python3 tools/linux_check/check.py gcc|clang [--x86-64] --log

The container runs on Docker's own architecture (ARM64 on Apple silicon).
--x86-64 runs it as x86-64 instead, under Docker's emulation, which is much
slower: name the targets to check rather than building everything.

It leaves out the 3DS apps (tagged devkitpro), which need Docker themselves,
and that isn't available inside the container; doesn't run the tests that
open a window (tagged display), as the container has no display; and, on
ARM64, leaves out models from Blender (tagged blender), as blender.org has
no Blender for Linux on ARM (--x86-64 builds them). With no targets it
builds and tests everything else. Exits with Bazel's exit
code.
"""

import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
COMPILERS = ("gcc", "clang")
DEFAULT_TARGETS = ["//..."]
# The 3DS apps build in devkitPro's Docker image, which can't run in here;
# tests that open a window (tagged display) are built but not run, as there's
# no display in here; and on ARM64, models from Blender (tagged blender) are
# left out, as blender.org has no Blender for Linux on ARM (--x86-64 builds
# them)
def skipped(x86_64):
    left_out = "-devkitpro" if x86_64 else "-devkitpro,-blender"
    return [f"--build_tag_filters={left_out}", f"--test_tag_filters={left_out},-display"]
LOG = "/root/.cache/last-run.log"

# Run inside the container: copy the checkout, build and test, keep the log,
# and print the lines that matter (tools/build_summary)
INSIDE = r"""
mkdir /src && cd /repo && tar --exclude="./bazel-*" -cf - . | (cd /src && tar xf -) && cd /src
bazel test --keep_going --curses=no --color=no "$@" > {log} 2>&1
status=$?
bash tools/build_summary/print_build_summary.sh {log}
exit $status
""".format(log=LOG)


def docker_running(docker):
    return subprocess.run([docker, "info"], capture_output=True).returncode == 0


def main(argv):
    if not argv or argv[0] not in COMPILERS:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    compiler, rest = argv[0], argv[1:]
    x86_64 = "--x86-64" in rest
    rest = [arg for arg in rest if arg != "--x86-64"]
    # An emulated x86-64 container has its own image and cache
    platform = ["--platform", "linux/amd64"] if x86_64 else []
    suffix = "-x86-64" if x86_64 else ""
    docker = shutil.which("docker")
    if not docker or not docker_running(docker):
        print("Docker isn't running. Start it (colima start --memory 8, or Docker Desktop) "
              "and run this again.", file=sys.stderr)
        return 1
    image = f"sakbox-linux-{compiler}{suffix}"
    volume = f"sakbox-linux-{compiler}{suffix}-cache"

    if rest == ["--log"]:
        return subprocess.run([docker, "run", "--rm", *platform, "-v", f"{volume}:/root/.cache",
                               image, "cat", LOG]).returncode

    print(f"Building the {compiler} image (quick once it's built)...", flush=True)
    built = subprocess.run([docker, "build", "-q", *platform, "-t", image, "-f",
                            str(HERE / f"Dockerfile.{compiler}"), str(HERE)],
                           capture_output=True, text=True)
    if built.returncode != 0:
        print(built.stderr, file=sys.stderr)
        return built.returncode
    # Docker's classic builder (Colima's, without buildx) ignores --platform
    # and builds for the machine's own architecture
    if x86_64:
        architecture = subprocess.run([docker, "image", "inspect", image, "--format",
                                       "{{.Architecture}}"], capture_output=True,
                                      text=True).stdout.strip()
        if architecture != "amd64":
            print(f"Docker built {image} for {architecture}, not x86-64: its classic builder "
                  "ignores --platform. Install buildx (brew install docker-buildx, then add "
                  '"cliPluginsExtraDirs": ["/opt/homebrew/lib/docker/cli-plugins"] to '
                  "~/.docker/config.json) and run this again.", file=sys.stderr)
            return 1

    # Clang-only checks are in the clang config (see .bazelrc)
    flags = (["--config=clang"] if compiler == "clang" else []) + skipped(x86_64)
    targets = rest or DEFAULT_TARGETS
    print(f"Building and testing with {compiler}{' on x86-64' if x86_64 else ''}: "
          f"{' '.join(targets)}", flush=True)
    return subprocess.run([docker, "run", "--rm", *platform, "-v", f"{REPO}:/repo:ro", "-v",
                           f"{volume}:/root/.cache", image, "bash", "-c", INSIDE, "inside", *flags,
                           "--", *targets]).returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
