"""Runs a shell script inside devkitPro's pinned Docker image.

Files go into the container as a tar stream on stdin and the results come
back as a tar stream on stdout, so no folders are shared with the
container. That works the same with Docker on Linux, Colima or Docker
Desktop on macOS, and Docker Desktop on Windows, and it never leaves files
owned by the container's user.
"""

import io
import os
import pathlib
import shlex
import shutil
import subprocess
import sys
import tarfile



def c_string(text):
    """`text` as a C string literal."""
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def make_defines(defines, name, title):
    """`make`'s SAKBOX_DEFINES: -D for the app's name and title
    (SCRATCHPADD_APP_NAME and SCRATCHPADD_APP_TITLE, for ScratchPadd's
    AppInfo) and for each of `defines` (NAME or NAME=VALUE). The app's
    Makefile adds them to its CFLAGS, and make's recipes pass those through
    a shell, so each is quoted for it, and the whole again for make's own
    command line."""
    all_defines = [f"SCRATCHPADD_APP_NAME={c_string(name)}",
                   f"SCRATCHPADD_APP_TITLE={c_string(title)}"] + list(defines)
    flags = " ".join(shlex.quote("-D" + define) for define in all_defines)
    return " SAKBOX_DEFINES=" + shlex.quote(flags)

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


def stage(files):
    """A tar archive of each (source, path) in `files`, at its path, keeping
    whether it's executable."""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.USTAR_FORMAT) as tar:
        for src, path in files:
            data = pathlib.Path(src).read_bytes()
            info = tarfile.TarInfo(path)
            info.size = len(data)
            info.mode = 0o755 if os.access(src, os.X_OK) else 0o644
            tar.addfile(info, io.BytesIO(data))
    return buf.getvalue()


def run(image, files, script, outputs, out_dir, what):
    """Runs `script` (sh) in `image` with `files` (source, path) unpacked in
    the current folder, and writes the files it lists, by name, as a tar
    stream on stdout (`outputs`) to `out_dir`, keeping their modes. Its
    other output goes to stderr. `what` names the job for errors."""
    # Docker reads its configuration (including Colima's context) from HOME,
    # which Bazel doesn't pass to actions.
    env = dict(os.environ)
    env.setdefault("HOME", str(pathlib.Path.home()))

    docker = find_docker()
    if not docker:
        sys.exit("devkitPro builds need Docker. On macOS: brew install colima docker && colima start")
    if subprocess.run([docker, "info"], env=env, capture_output=True).returncode != 0:
        sys.exit("Docker isn't running. Start it (colima start, or Docker Desktop) and build again.")

    result = subprocess.run(
        [docker, "run", "-i", "--rm", image, "sh", "-c", "mkdir /work && cd /work && tar -x && " + script],
        input=stage(files),
        stdout=subprocess.PIPE,
        env=env,
    )
    if result.returncode != 0:
        sys.exit(f"{what} failed (exit {result.returncode}); its output is above.")

    os.makedirs(out_dir, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(result.stdout)) as tar:
        for member in tar.getmembers():
            if member.name not in outputs or not member.isfile():
                sys.exit(f"Unexpected file from {what}: {member.name}")
            out = pathlib.Path(out_dir, member.name)
            out.write_bytes(tar.extractfile(member).read())
            out.chmod(0o755 if member.mode & 0o111 else 0o644)


def pairs(values):
    """[a1, b1, a2, b2, ...] as [(a1, b1), (a2, b2), ...]"""
    return list(zip(values[0::2], values[1::2]))
