"""Prints the lines of a `bazel test` log that matter.

Bazel's status and summary, the tests that didn't pass, the test cases
that skipped inside tests that passed, Bazel's errors and the compilers'.
The Linux and VM checks run it on their build's log (tools/linux_check,
tools/vm_check).

A test that calls GTEST_SKIP still passes as a whole, so Bazel's summary
alone hides it. The skipped cases are read from each passing test's
test.xml in bazel-testlogs (there for cached results too), and printed one
line per test and reason.

Usage: python3 print_build_summary.py LOG [WORKSPACE]
WORKSPACE is the checkout the build ran in (default: the current folder).
"""

import collections
import pathlib
import re
import sys
import xml.etree.ElementTree as ElementTree

STATUS = re.compile(r"Executed|Build (completed|did NOT)|Build succeeded for|Server terminated")
LONGEST = 240


def last_results(folder):
    """The test.xml files of a test's last run: one, or one per shard or run
    (shard_1_of_2/test.xml, run_1_of_3/...). Earlier runs' are left beside
    them (run_1_of_10/... from --runs_per_test=10), so the files are grouped
    by layout, and the group written last is the one."""
    layouts = collections.defaultdict(list)
    for results in folder.glob("**/test.xml"):
        layout = tuple(re.sub(r"^(run|shard)_\d+_of_", r"\1_of_", part)
                       for part in results.relative_to(folder).parent.parts)
        layouts[layout].append(results)
    if not layouts:
        return []
    return sorted(max(layouts.values(),
                      key=lambda files: max(results.stat().st_mtime for results in files)))


def skipped_cases(testlogs, target):
    """The reasons the target's test cases skipped, and how many skipped for each."""
    package, name = target[2:].split(":")
    reasons = collections.Counter()
    for results in last_results(pathlib.Path(testlogs, package, name)):
        try:
            cases = ElementTree.parse(results).getroot().iter("testcase")
        except ElementTree.ParseError:
            continue
        for case in cases:
            skipped = case.find("skipped")
            if skipped is None:
                continue
            # GoogleTest's message starts with the file and line it skipped at,
            # and may end with SCOPED_TRACE's trace
            message = skipped.get("message") or skipped.text or ""
            message = message.split("Google Test trace:")[0]
            lines = [line for line in message.splitlines() if line.strip()]
            if lines and re.match(r"^\S+:\d+$", lines[0]):
                lines = lines[1:]
            reasons[" ".join(lines) or "(no reason given)"] += 1
    return reasons


def main(argv):
    if not argv or len(argv) > 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    log = pathlib.Path(argv[0]).read_text(errors="replace").splitlines()
    testlogs = pathlib.Path(argv[1] if len(argv) > 1 else ".", "bazel-testlogs")

    for line in log:
        if STATUS.search(line):
            print(line)
    # Tests' result lines: //package:name, then how it went
    passed = []
    for line in log:
        if not line.startswith("//"):
            continue
        if " PASSED" in line:
            passed.append(line.split()[0])
        else:
            print(line)

    skips = [(target, reason, count) for target in passed
             for reason, count in skipped_cases(testlogs, target).items()]
    if skips:
        print("Test cases skipped inside passing tests:")
        for target, reason, count in skips:
            print(f"  {target}: {count} skipped: {reason}"[:LONGEST])

    bazel_errors = {line[:LONGEST] for line in log if line.startswith("ERROR:")}
    compiler_errors = {line[:LONGEST] for line in log
                       if "error:" in line and not line.startswith("ERROR:")}
    for line in sorted(bazel_errors) + sorted(compiler_errors)[:40]:
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
