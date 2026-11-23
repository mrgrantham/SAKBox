"""Prints line coverage per file from an LCOV report, worst first.

`bazel coverage` (with the repo's .bazelrc on a Mac) writes one combined
report for every test it ran, bazel-out/_coverage/_coverage_report.dat;
this reads it and prints each source file's lines run, of those that can
be, and the total. Files whose path doesn't start with one of the PREFIXes
are left out.

Usage: python3 tools/coverage/lcov_table.py [REPORT] [PREFIX...]
REPORT defaults to bazel-out/_coverage/_coverage_report.dat.
"""

import collections
import sys

DEFAULT_REPORT = "bazel-out/_coverage/_coverage_report.dat"


def line_counts(report):
    """Each source file's lines that can run, and how often each did (a
    file can appear once per test: their counts are added)"""
    files = collections.defaultdict(collections.Counter)
    source = None
    with open(report, encoding="utf-8") as lcov:
        for line in lcov:
            if line.startswith("SF:"):
                source = line[3:].strip()
            elif line.startswith("DA:") and source:
                number, count = line[3:].split(",")[:2]
                files[source][int(number)] += int(count)
    return files


def main(arguments):
    report = arguments[0] if arguments else DEFAULT_REPORT
    prefixes = tuple(arguments[1:])
    rows = []
    for source, lines in line_counts(report).items():
        if prefixes and not source.startswith(prefixes) or not lines:
            continue
        run = sum(1 for count in lines.values() if count > 0)
        rows.append((run / len(lines), run, len(lines), source))
    rows.sort()
    for share, run, lines, source in rows:
        print(f"{100 * share:6.1f}% {run:6}/{lines:<6} {source}")
    run = sum(row[1] for row in rows)
    lines = sum(row[2] for row in rows)
    if lines:
        print(f"{100 * run / lines:6.1f}% {run:6}/{lines:<6} in all")


if __name__ == "__main__":
    main(sys.argv[1:])
