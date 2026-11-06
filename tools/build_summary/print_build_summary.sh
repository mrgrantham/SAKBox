#!/usr/bin/env bash
# Prints the lines of a `bazel test` log that matter: Bazel's status and
# summary, the tests that didn't pass, Bazel's errors and the compilers'.
# The Linux and VM checks run it on their build's log (tools/linux_check,
# tools/vm_check).
#
# Usage: print_build_summary.sh LOG
log="$1"
grep -aE "Executed|Build (completed|did NOT)|Build succeeded for|Server terminated" "$log"
grep -aE "^//" "$log" | grep -av " PASSED" || true
grep -aE "^ERROR:" "$log" | cut -c1-240 | sort -u || true
grep -aE "error:" "$log" | grep -av "^ERROR:" | cut -c1-240 | sort -u | head -40 || true
