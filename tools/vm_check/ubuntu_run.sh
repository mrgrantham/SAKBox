#!/usr/bin/env bash
# Builds and tests the checkout in the Ubuntu VM, as its desktop user
# (check.py starts it as a systemd service, and waits for it). Copies the
# checkout from the VM's read-only share of it into ~/sakbox-COMPILER,
# leaving out what git ignores, runs bazel test there on the VM's desktop,
# and keeps the log and Bazel's summary beside it, and last its exit code
# (the sign it's finished).
#
# Usage: ubuntu_run.sh SHARE COMPILER [BAZEL FLAGS AND TARGETS ...]
share="$1"
compiler="$2"
shift 2
work="$HOME/sakbox-$compiler"
log="$work.log"
rm -f "$work.status" "$work.summary"
# The exit code, written however it ends: check.py waits for it
status=1
trap 'echo "$status" > "$work.status"' EXIT

# (u+w: the share is read-only, and so would the copy be)
rsync -a --chmod=u+w --delete --delete-excluded --filter=":- .gitignore" --exclude=/.git \
  "$share/" "$work/"
cd "$work" || exit 1
case "$compiler" in
  gcc) export CC=gcc-14 CXX=g++-14 ;;
  clang) export CC=clang-18 CXX=clang++-18 ;;
esac
# The desktop's X server, for the tests that open a window
export DISPLAY=:0 XAUTHORITY="/run/user/$(id -u)/gdm/Xauthority"
/usr/local/bin/bazelisk test --keep_going --curses=no --color=no "$@" > "$log" 2>&1
status=$?
bash "$share/tools/build_summary/print_build_summary.sh" "$log" > "$work.summary"
