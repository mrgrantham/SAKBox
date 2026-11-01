#!/bin/sh
# Runs Blender headless for a build action: `run_blender.sh PROGRAM ARGS...`.
# PROGRAM is Bazel's Blender for this machine (//tools/blender), or empty
# where blender.org has none; BLENDER, if set (with --action_env=BLENDER),
# is used instead. Blender keeps its settings and caches in the action's own
# temporary folder, never the user's.
program="$1"
shift
if [ -n "$BLENDER" ]; then
  program="$BLENDER"
fi
if [ -z "$program" ]; then
  echo "No Blender for this machine: blender.org builds Blender for macOS on Apple silicon," >&2
  echo "Linux x86-64 and Windows. Build models on one of those (the Linux check's --x86-64)," >&2
  echo "or point BLENDER at a Blender: bazel build --action_env=BLENDER=/path/to/blender ..." >&2
  exit 1
fi
home="${TMPDIR:-/tmp}/blender-home-$$"
mkdir -p "$home"
export HOME="$home"
export BLENDER_USER_RESOURCES="$home/resources"
exec "$program" --background --factory-startup "$@"
