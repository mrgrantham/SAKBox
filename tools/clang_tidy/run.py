#!/usr/bin/env python3
"""Runs clang-tidy over the repo's own code, with the checks in .clang-tidy.

Usage (from anywhere in the repo):
  python3 tools/clang_tidy/run.py                 # everything except Waffles
  python3 tools/clang_tidy/run.py projects/cyclone projects/flexmath/src/x.cpp
  python3 tools/clang_tidy/run.py --fix projects/cyclone   # apply fixes
  python3 tools/clang_tidy/run.py --checks='-*,modernize-use-nullptr' --fix
                                         # only some checks (and their fixes)

Needs clang-tidy (not part of Xcode: `brew install llvm` on macOS) and a
current compile_commands.json (`python3 tools/compile_commands/refresh.py`).
Prints each finding once, with its path in the repo, then a count per
check. Exits 1 if there are findings. tools/git_hooks/pre-commit uses this
file's functions to check the files being committed.
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

# Upstream code kept as published, checked by its own project, not ours
EXCLUDE = [
    "projects/examples/png/src/pngtest.cpp",  # libpng's test program (C style)
]

# Where LLVM's tools are when they aren't on PATH (Homebrew keeps them off it)
LLVM_DIRS = ["/opt/homebrew/opt/llvm/bin", "/usr/local/opt/llvm/bin", "/usr/lib/llvm/bin"]


def find_tool(name, env_var, required=True):
    """The path to an LLVM tool: $env_var, PATH, or a usual LLVM folder."""
    if os.environ.get(env_var):
        return os.environ[env_var]
    found = shutil.which(name)
    if found:
        return found
    for d in LLVM_DIRS:
        candidate = pathlib.Path(d, name)
        if candidate.exists():
            return str(candidate)
    if required:
        sys.exit(f"Can't find {name}. Install LLVM (macOS: brew install llvm; Linux: your "
                 f"distribution's clang-tidy package) or set {env_var}.")
    return None


def source_regex(paths):
    """A regex for compile_commands.json entries under the given repo folders
    or matching the given files, leaving out EXCLUDE."""
    alternatives = []
    for p in paths:
        p = p.rstrip("/")
        full = re.escape(str(REPO / p))
        alternatives.append(full + "$" if (REPO / p).is_file() else full + "/")
    return ("^(?!" + "|".join(re.escape(str(REPO / e)) for e in EXCLUDE) + ")"
            "(" + "|".join(alternatives) + ")")


def repo_path(reported):
    """A path clang-tidy reported, as a path in the repo. Headers are often
    reported through Bazel's _virtual_includes, which link back to the
    sources, or as a/../b; both resolve to the real file."""
    resolved = os.path.realpath(reported if os.path.isabs(reported) else REPO / reported)
    try:
        return os.path.relpath(resolved, os.path.realpath(REPO))
    except ValueError:
        return resolved


def tidy(file_regex, fix=False, checks=None, jobs=None):
    """Runs clang-tidy on the compile_commands.json entries matching
    file_regex. Returns (findings, result), findings being sorted, unique
    (path, line, kind, message, checks) tuples."""
    clang_tidy = find_tool("clang-tidy", "CLANG_TIDY")
    run_clang_tidy = find_tool("run-clang-tidy", "RUN_CLANG_TIDY")
    command = [run_clang_tidy, "-p", str(REPO), "-clang-tidy-binary", clang_tidy,
               "-quiet", "-j", str(jobs or os.cpu_count())]
    if fix:
        # run-clang-tidy applies the fixes with clang-apply-replacements
        command += ["-fix", "-clang-apply-replacements-binary",
                    find_tool("clang-apply-replacements", "CLANG_APPLY_REPLACEMENTS")]
    if checks:
        command.append("-checks=" + checks)
    command.append(file_regex)
    result = subprocess.run(command, cwd=REPO, capture_output=True, text=True)

    # A header is reported once per file that includes it; keep each once
    findings = set()
    for line in result.stdout.splitlines():
        m = re.match(r"(.+?):(\d+):(\d+): (warning|error): (.*) \[([\w.,-]+)\]$", line)
        if m:
            findings.add((repo_path(m.group(1)), int(m.group(2)), m.group(4), m.group(5),
                          m.group(6)))
    return sorted(findings), result


def print_findings(findings, fixed=False):
    for path, line, kind, message, check in findings:
        print(f"{path}:{line}: {kind}: {message} [{check}]")
    counts = collections.Counter(c for *_, checks in findings for c in checks.split(","))
    print(f"\n{len(findings)} findings" + (" (fixes applied where clang-tidy has one)" if fixed else ""))
    for check, n in counts.most_common():
        print(f"{n:5}  {check}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", help="repo folders or files to check (default: all but Waffles)")
    parser.add_argument("--fix", action="store_true", help="apply clang-tidy's suggested fixes")
    parser.add_argument("--checks", help="checks to run instead of .clang-tidy's (clang-tidy's -checks syntax)")
    parser.add_argument("-j", type=int, default=os.cpu_count(), help="parallel jobs")
    args = parser.parse_args()

    if not (REPO / "compile_commands.json").exists():
        sys.exit("No compile_commands.json: run python3 tools/compile_commands/refresh.py first.")

    findings, result = tidy(source_regex(args.paths or DEFAULT_PATHS), args.fix, args.checks, args.j)
    print_findings(findings, args.fix)
    if not findings and result.returncode != 0:
        # No findings but a failure: clang-tidy itself didn't run properly
        print(result.stdout[-2000:] + result.stderr, file=sys.stderr)
        sys.exit(f"clang-tidy failed (exit {result.returncode})")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
