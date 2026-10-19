#!/usr/bin/env python3
"""Formats the repo's C/C++ files with clang-format, in the repo's style
(.clang-format). Files listed in .clang-format-ignore (upstream code) are
left alone.

    python3 tools/clang_format/format.py               # files you've changed
    python3 tools/clang_format/format.py --all         # every file under projects/
    python3 tools/clang_format/format.py FILE ...      # just these
    python3 tools/clang_format/format.py --check ...   # report, don't change (exits 1
                                                       # if any file needs formatting)

"Files you've changed" means C/C++ files under projects/ that differ from
the last commit, staged or not, plus new files git doesn't track yet.
"""

import argparse
import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
EXTENSIONS = (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx")

# clang-tidy's runner knows where LLVM's tools are (without leaving a
# __pycache__ folder in the repo)
sys.dont_write_bytecode = True
sys.path.insert(0, str(REPO / "tools/clang_tidy"))
from run import find_tool  # noqa: E402


def git_files(*args):
    out = subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True, check=True)
    return out.stdout.split("\n")


def cpp_under_projects(paths):
    return sorted({p for p in paths if p.startswith("projects/") and p.endswith(EXTENSIONS)})


def changed_files():
    changed = git_files("diff", "--name-only", "--diff-filter=ACMR", "HEAD")
    untracked = git_files("ls-files", "--others", "--exclude-standard")
    return cpp_under_projects(changed + untracked)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("files", nargs="*", help="files to format (default: the ones you've changed)")
    parser.add_argument("--all", action="store_true", help="every C/C++ file under projects/")
    parser.add_argument("--check", action="store_true",
                        help="list files that need formatting instead of changing them")
    args = parser.parse_args()

    if args.all:
        files = cpp_under_projects(git_files("ls-files"))
    elif args.files:
        files = [str(pathlib.Path(f).resolve().relative_to(REPO)) for f in args.files]
    else:
        files = changed_files()
    if not files:
        print("No C/C++ files to format.")
        return 0

    clang_format = find_tool("clang-format", "CLANG_FORMAT")
    if args.check:
        unformatted = [f for f in files
                       if subprocess.run([clang_format, "--dry-run", "--Werror", f], cwd=REPO,
                                         capture_output=True).returncode != 0]
        for f in unformatted:
            print(f"needs formatting: {f}")
        print(f"{len(unformatted)} of {len(files)} files need formatting.")
        return 1 if unformatted else 0

    # (one call for every file: clang-format skips those in .clang-format-ignore)
    subprocess.run([clang_format, "-i", *files], cwd=REPO, check=True)
    print(f"Ran clang-format on {len(files)} file(s) "
          "(leaving any in .clang-format-ignore as they were).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
