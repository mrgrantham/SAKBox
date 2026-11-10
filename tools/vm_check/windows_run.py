"""Builds and tests the checkout in the Windows VM (check.py starts it, as a
scheduled task run as the user at the desktop).

Copies the checkout from the VM's read-only share of it into
C:\\sakbox\\COMPILER: the files git has, and new ones it doesn't ignore.
Runs bazel test there, with Bazel's output in C:\\b (short paths: Windows'
limit is 260 characters), and keeps the log and Bazel's summary beside the
copy, and last its exit code (the sign it's finished).

Usage: windows_run.py SHARE COMPILER [BAZEL FLAGS AND TARGETS ...]
"""

import os
import pathlib
import shutil
import subprocess
import sys

GIT = r"C:\Program Files\Git\cmd\git.exe"
BASH = r"C:\Program Files\Git\bin\bash.exe"
BAZELISK = r"C:\tools\bazelisk.exe"
OUTPUT_ROOT = r"C:\b"
# Visual Studio Build Tools: MSVC's libraries and the Windows SDK, and its
# Clang (clang-cl) for ARM64
VC = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC"
LLVM = VC + r"\Tools\Llvm\ARM64"
# Which Vulkan devices Dozen (Vulkan on Direct3D 12, from Windows' OpenCL,
# OpenGL and Vulkan Compatibility Pack) offers the tests: WARP, Direct3D's
# software renderer, alone. Dozen crashes making a device on Parallels'
# Direct3D 12 driver.
DOZEN_DEVICES = "warp"
# Run by check.py with pythonw, at the user's desktop: no console windows
# for the programs it runs
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)


def checkout_files(share):
    """The checkout's files: what git has, and new files it doesn't ignore."""
    listed = subprocess.run(
        [GIT, "-c", "safe.directory=*", "-C", share, "ls-files", "-z", "--cached", "--others",
         "--exclude-standard"], capture_output=True, check=True, creationflags=NO_WINDOW).stdout
    return sorted({name for name in listed.decode().split("\0") if name})


def copy_checkout(share, work):
    """Makes WORK the checkout's files, copying only what changed."""
    files = checkout_files(share)
    wanted = {pathlib.PurePosixPath(name) for name in files}
    for name in files:
        source = pathlib.Path(share, name)
        target = pathlib.Path(work, name)
        if not source.is_file():
            continue  # deleted in the working tree
        if target.exists():
            same = source.stat()
            have = target.stat()
            if same.st_size == have.st_size and int(same.st_mtime) == int(have.st_mtime):
                continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    # What's gone from the checkout goes from the copy. Bazel's links
    # (bazel-bin and so on, junctions on Windows) are left, and not walked:
    # they lead into all of Bazel's output.
    for folder, subfolders, names in os.walk(work):
        if pathlib.Path(folder) == pathlib.Path(work):
            subfolders[:] = [name for name in subfolders if not name.startswith("bazel-")]
        for name in names:
            path = pathlib.Path(folder, name)
            if pathlib.PurePosixPath(path.relative_to(work).as_posix()) not in wanted:
                path.unlink()


def main(argv):
    share, compiler, rest = argv[0], argv[1], argv[2:]
    work = pathlib.Path(r"C:\sakbox", compiler)
    log, summary, status_file = (work.with_suffix(suffix) for suffix in
                                 (".log", ".summary", ".status"))
    for stale in (summary, status_file):
        stale.unlink(missing_ok=True)
    status = 1
    try:
        copy_checkout(share, work)
        # (Bazelisk's downloads of Bazel beside Bazel's output, shared by
        # whoever runs it: SYSTEM for a quick check, the user for check.py)
        env = dict(os.environ, BAZEL_SH=BASH, BAZEL_VC=VC, BAZEL_LLVM=LLVM,
                   BAZELISK_HOME=OUTPUT_ROOT + r"\bazelisk")
        with open(log, "w") as out:
            status = subprocess.run(
                [BAZELISK, f"--output_user_root={OUTPUT_ROOT}", "test", "--keep_going",
                 "--curses=no", "--color=no", f"--test_env=DZN_DEBUG={DOZEN_DEVICES}", *rest],
                cwd=work, env=env, stdout=out,
                stderr=subprocess.STDOUT, creationflags=NO_WINDOW).returncode
        with open(summary, "w") as out:
            subprocess.run([BASH, pathlib.Path(share, "tools/build_summary/print_build_summary.sh")
                            .as_posix(), pathlib.Path(log).as_posix()], stdout=out,
                           creationflags=NO_WINDOW)
    finally:
        status_file.write_text(str(status))


if __name__ == "__main__":
    main(sys.argv[1:])
