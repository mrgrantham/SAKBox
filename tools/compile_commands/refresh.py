#!/usr/bin/env python3
"""Writes compile_commands.json at the repo root so clangd understands the build.

    python3 tools/compile_commands/refresh.py [target patterns...]   (default //...)

Bazel already knows the exact compiler command for every source file;
`bazel aquery` prints them. This script builds the targets (so generated
headers exist), asks aquery for their C/C++ compile commands, and rewrites
them into the format clangd reads:

- Paths stay relative to the repo root, so go-to-definition opens your real
  files. Bazel's bazel-out/ symlink already exists there; this script adds an
  `external` symlink so paths into external repos (fmt, glm, ...) resolve too.
- Bazel's compiler wrapper is replaced with plain clang/clang++, and on macOS
  the Xcode placeholders (__BAZEL_XCODE_SDKROOT__, ...) are filled in.
- An external repo's top folder goes back to -iquote. .bazelrc's
  external_include_paths makes it -isystem, so <...> includes search it too;
  that's harmless in Bazel's sandbox, which holds only declared files, but
  here the whole repo is visible, and a stray file there shadows a standard
  header (Bullet's `version` file is found for <version>).

Re-run it after adding source files or changing BUILD deps. The generated
files (compile_commands.json, external, .cache/) are git-ignored.
"""
import json
import os
import re
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def bazel(*args, check=True):
    return subprocess.run(["bazel", *args], cwd=ROOT, check=check, capture_output=True, text=True)


def xcode_placeholders():
    if platform.system() != "Darwin":
        return {}

    def run(*cmd):
        return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout.strip()

    return {
        "__BAZEL_XCODE_SDKROOT__": run("xcrun", "--show-sdk-path"),
        "__BAZEL_XCODE_DEVELOPER_DIR__": run("xcode-select", "--print-path"),
    }


def source_file(arguments):
    """The file a compile command compiles: the argument after -c."""
    for flag, value in zip(arguments, arguments[1:]):
        if flag == "-c":
            return value
    return None


# An external repo's top folder, as Bazel names it on the command line
REPO_ROOT = re.compile(r"(bazel-out/[^/]+/bin/)?external/[^/]+")


def to_clangd_command(arguments, placeholders):
    """Rewrites one aquery command for clangd, or returns None to skip it."""
    if "-fsyntax-only" in arguments:  # header-parsing check, not a compile
        return None
    file = source_file(arguments)
    if file is None:
        return None
    compiler = "clang" if file.endswith((".c", ".m")) else "clang++"
    fixed = [compiler]
    rest = arguments[1:]
    for i, arg in enumerate(rest):
        for placeholder, value in placeholders.items():
            arg = arg.replace(placeholder, value)
        if arg == "-isystem" and i + 1 < len(rest) and REPO_ROOT.fullmatch(rest[i + 1]):
            arg = "-iquote"  # (see the module docstring)
        fixed.append(arg)
    return {"directory": str(ROOT), "file": file, "arguments": fixed}


def link_external():
    """Points <repo>/external at Bazel's external repos, so paths resolve."""
    output_base = bazel("info", "output_base").stdout.strip()
    target = Path(output_base) / "external"
    link = ROOT / "external"
    if link.is_symlink():
        link.unlink()
    elif link.exists():
        sys.exit(f"{link} exists and is not a symlink; not replacing it.")
    link.symlink_to(target)


def main():
    targets = sys.argv[1:] or ["//..."]
    pattern = " + ".join(targets)

    print(f"Building {pattern} so generated headers exist...")
    build = bazel("build", "--keep_going", *targets, check=False)
    if build.returncode != 0:
        print("Warning: some targets failed to build; their entries may be incomplete.")

    print("Querying compile commands...")
    query = bazel(
        "aquery",
        f'mnemonic("CppCompile|ObjcCompile", {pattern})',
        "--output=jsonproto",
        "--include_artifacts=false",
    )
    actions = json.loads(query.stdout or "{}").get("actions", [])

    placeholders = xcode_placeholders()
    entries = {}
    for action in actions:
        entry = to_clangd_command(action["arguments"], placeholders)
        if entry:
            entries[entry["file"]] = entry  # one entry per file is enough

    link_external()
    output = ROOT / "compile_commands.json"
    output.write_text(json.dumps(sorted(entries.values(), key=lambda e: e["file"]), indent=2))
    print(f"Wrote {len(entries)} entries to {output}")


if __name__ == "__main__":
    main()
