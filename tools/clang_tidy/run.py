#!/usr/bin/env python3
"""Runs clang-tidy over the repo's own code, with the checks in .clang-tidy.

Usage (from anywhere in the repo):
  python3 tools/clang_tidy/run.py                 # everything except Waffles
  python3 tools/clang_tidy/run.py projects/cyclone projects/flexmath
  python3 tools/clang_tidy/run.py --fix projects/cyclone   # apply fixes

Needs clang-tidy (not part of Xcode: `brew install llvm` on macOS) and a
current compile_commands.json (`python3 tools/compile_commands/refresh.py`).
Prints each finding, then a count per check. Exits 1 if there are findings.
"""

import argparse
import collections
import os
import pathlib
import re
import shutil
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]

# Waffles has thousands of findings and isn't converted yet
DEFAULT_PATHS = [
    "projects/cyclone",
    "projects/data_dep_retriever",
    "projects/examples",
    "projects/flexmath",
    "projects/scratchpadd",
]

# Where LLVM's tools are when they aren't on PATH (Homebrew keeps them off it)
LLVM_DIRS = ["/opt/homebrew/opt/llvm/bin", "/usr/local/opt/llvm/bin", "/usr/lib/llvm/bin"]


def find_tool(name, env_var):
    if os.environ.get(env_var):
        return os.environ[env_var]
    found = shutil.which(name)
    if found:
        return found
    for d in LLVM_DIRS:
        candidate = pathlib.Path(d, name)
        if candidate.exists():
            return str(candidate)
    sys.exit(f"Can't find {name}. Install LLVM (macOS: brew install llvm; Linux: your "
             f"distribution's clang-tidy package) or set {env_var}.")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", help="repo folders to check (default: all but Waffles)")
    parser.add_argument("--fix", action="store_true", help="apply clang-tidy's suggested fixes")
    parser.add_argument("-j", type=int, default=os.cpu_count(), help="parallel jobs")
    args = parser.parse_args()

    if not (REPO / "compile_commands.json").exists():
        sys.exit("No compile_commands.json: run python3 tools/compile_commands/refresh.py first.")

    clang_tidy = find_tool("clang-tidy", "CLANG_TIDY")
    run_clang_tidy = find_tool("run-clang-tidy", "RUN_CLANG_TIDY")
    paths = [p.rstrip("/") for p in (args.paths or DEFAULT_PATHS)]
    file_regex = "^(" + "|".join(re.escape(str(REPO / p)) + "/" for p in paths) + ")"

    command = [run_clang_tidy, "-p", str(REPO), "-clang-tidy-binary", clang_tidy,
               "-quiet", "-j", str(args.j)]
    if args.fix:
        command.append("-fix")
    command.append(file_regex)
    result = subprocess.run(command, cwd=REPO, capture_output=True, text=True)

    # A header is reported once per file that includes it; show each once
    findings = []
    seen = set()
    for line in result.stdout.splitlines():
        m = re.match(r"(.+?):(\d+):(\d+): (warning|error): (.*) \[([\w.,-]+)\]$", line)
        if not m:
            continue
        path = re.sub(r"^.*?/_virtual_includes/[^/]+/", "", m.group(1))
        path = os.path.relpath(path, REPO) if path.startswith("/") else path
        key = (path, m.group(2), m.group(6))
        if key not in seen:
            seen.add(key)
            findings.append((path, int(m.group(2)), m.group(4), m.group(5), m.group(6)))

    for path, line, kind, message, check in sorted(findings):
        print(f"{path}:{line}: {kind}: {message} [{check}]")
    counts = collections.Counter(c for *_, checks in findings for c in checks.split(","))
    print(f"\n{len(findings)} findings" + (" (fixes applied where clang-tidy has one)" if args.fix else ""))
    for check, n in counts.most_common():
        print(f"{n:5}  {check}")
    if result.returncode not in (0, 1) and not findings:
        print(result.stderr, file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
