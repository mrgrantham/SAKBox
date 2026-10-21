"""Builds and tests the repo on Linux, in Docker, with GCC or Clang.

The checkout is mounted read-only and copied inside the container, so the
Linux build can't touch your Bazel output links or lockfile. Each compiler
keeps Bazel's downloads and cache in its own Docker volume
(sakbox-linux-gcc-cache, sakbox-linux-clang-cache), so later runs are quick.
It prints Bazel's summary: the build's result, any test that didn't pass,
and the errors. The full output of the last run is kept in the volume
(--log prints it).

Usage:
  python3 tools/linux_check/check.py gcc|clang [TARGET ...]
  python3 tools/linux_check/check.py gcc|clang --log

It leaves out the 3DS apps (tagged devkitpro), which need Docker themselves,
and that isn't available inside the container. With no targets it builds
and tests everything else. Exits with Bazel's exit code.
"""

import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
COMPILERS = ("gcc", "clang")
DEFAULT_TARGETS = ["//..."]
# The 3DS apps build in devkitPro's Docker image, which can't run in here
SKIP_3DS_APPS = ["--build_tag_filters=-devkitpro", "--test_tag_filters=-devkitpro"]
LOG = "/root/.cache/last-run.log"

# Run inside the container: copy the checkout, build and test, keep the log,
# and print the lines that matter (Bazel's status and summary, tests that
# didn't pass, Bazel's errors and the compilers')
INSIDE = r"""
mkdir /src && cd /repo && tar --exclude="./bazel-*" -cf - . | (cd /src && tar xf -) && cd /src
bazel test --keep_going --curses=no --color=no "$@" > {log} 2>&1
status=$?
grep -E "Executed|Build (completed|did NOT)|Build succeeded for|Server terminated" {log}
grep -E "^//" {log} | grep -v " PASSED" || true
grep -E "^ERROR:" {log} | cut -c1-240 | sort -u || true
grep -E "error:" {log} | grep -v "^ERROR:" | cut -c1-240 | sort -u | head -40 || true
exit $status
""".format(log=LOG)


def docker_running(docker):
    return subprocess.run([docker, "info"], capture_output=True).returncode == 0


def main(argv):
    if not argv or argv[0] not in COMPILERS:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    compiler, rest = argv[0], argv[1:]
    docker = shutil.which("docker")
    if not docker or not docker_running(docker):
        print("Docker isn't running. Start it (colima start --memory 8, or Docker Desktop) "
              "and run this again.", file=sys.stderr)
        return 1
    image = f"sakbox-linux-{compiler}"
    volume = f"sakbox-linux-{compiler}-cache"

    if rest == ["--log"]:
        return subprocess.run([docker, "run", "--rm", "-v", f"{volume}:/root/.cache", image,
                               "cat", LOG]).returncode

    print(f"Building the {compiler} image (quick once it's built)...", flush=True)
    built = subprocess.run([docker, "build", "-q", "-t", image, "-f", str(HERE / f"Dockerfile.{compiler}"),
                            str(HERE)], capture_output=True, text=True)
    if built.returncode != 0:
        print(built.stderr, file=sys.stderr)
        return built.returncode

    # Clang-only checks are in the clang config (see .bazelrc)
    flags = (["--config=clang"] if compiler == "clang" else []) + SKIP_3DS_APPS
    targets = rest or DEFAULT_TARGETS
    print(f"Building and testing with {compiler}: {' '.join(targets)}", flush=True)
    return subprocess.run([docker, "run", "--rm", "-v", f"{REPO}:/repo:ro", "-v", f"{volume}:/root/.cache",
                           image, "bash", "-c", INSIDE, "inside", *flags, "--", *targets]).returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
