"""Builds a 3DS homebrew app with devkitPro's Makefile inside the pinned
devkitARM image.

Sources go into the container as a tar stream on stdin and the built
.3dsx/.smdh/.elf come back as a tar stream on stdout, so no folders are
shared with the container. That works the same with Docker on Linux, Colima
or Docker Desktop on macOS, and Docker Desktop on Windows, and it never
leaves files owned by the container's user.

Usage: build_3ds.py IMAGE NAME OUT_DIR PACKAGE SRC...
"""

import io
import os
import pathlib
import shlex
import shutil
import subprocess
import sys
import tarfile


def find_docker():
    # Bazel runs actions with a minimal PATH, which may not include where
    # Homebrew or Docker Desktop installed the docker command.
    found = shutil.which("docker")
    if found:
        return found
    for candidate in ["/opt/homebrew/bin/docker", "/usr/local/bin/docker", "/usr/bin/docker"]:
        if os.access(candidate, os.X_OK):
            return candidate
    return None


def stage_sources(package, srcs):
    """A tar archive of `srcs` at their paths within `package`."""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.USTAR_FORMAT) as tar:
        for src in srcs:
            rel = pathlib.PurePosixPath(pathlib.Path(src).as_posix()).relative_to(package)
            data = pathlib.Path(src).read_bytes()
            info = tarfile.TarInfo(str(rel))
            info.size = len(data)
            info.mode = 0o644
            tar.addfile(info, io.BytesIO(data))
    return buf.getvalue()


def main(argv):
    if len(argv) < 5:
        sys.exit(__doc__)
    image, name, out_dir, package, *srcs = argv

    # Docker reads its configuration (including Colima's context) from HOME,
    # which Bazel doesn't pass to actions.
    env = dict(os.environ)
    env.setdefault("HOME", str(pathlib.Path.home()))

    docker = find_docker()
    if not docker:
        sys.exit("devkitPro builds need Docker. On macOS: brew install colima docker && colima start")
    if subprocess.run([docker, "info"], env=env, capture_output=True).returncode != 0:
        sys.exit("Docker isn't running. Start it (colima start, or Docker Desktop) and build again.")

    outputs = [f"{name}{ext}" for ext in (".3dsx", ".smdh", ".elf")]
    script = " && ".join([
        "mkdir /app",
        "cd /app",
        "tar -x",
        f"make TARGET={shlex.quote(name)} >&2",
        "tar -c " + " ".join(shlex.quote(o) for o in outputs),
    ])
    result = subprocess.run(
        [docker, "run", "-i", "--rm", image, "sh", "-c", script],
        input=stage_sources(package, srcs),
        stdout=subprocess.PIPE,
        env=env,
    )
    if result.returncode != 0:
        sys.exit(f"The devkitPro build failed (exit {result.returncode}); its output is above.")

    os.makedirs(out_dir, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(result.stdout)) as tar:
        for member in tar.getmembers():
            if member.name not in outputs or not member.isfile():
                sys.exit(f"Unexpected file from the build: {member.name}")
            pathlib.Path(out_dir, member.name).write_bytes(tar.extractfile(member).read())


if __name__ == "__main__":
    main(sys.argv[1:])
