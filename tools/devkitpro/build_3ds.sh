#!/bin/bash
# Builds a 3DS homebrew app with devkitPro's Makefile inside the pinned
# devkitARM image. Sources go in, and the .3dsx/.smdh/.elf come out, through
# tar on stdin/stdout, so no folders are shared with the container.
#
# Usage: build_3ds.sh IMAGE NAME OUT_DIR PACKAGE SRC...
set -euo pipefail

image=$1 name=$2 out_dir=$3 package=$4
shift 4

# Bazel runs actions without the user's PATH or HOME; find Docker and its
# configuration (Colima registers itself as the current Docker context).
export HOME=${HOME:-$(eval echo "~$(id -un)")}
export PATH="$PATH:/opt/homebrew/bin:/usr/local/bin"
if ! command -v docker >/dev/null; then
  echo "devkitPro builds need Docker. On macOS: brew install colima docker && colima start" >&2
  exit 1
fi
if ! docker info >/dev/null 2>&1; then
  echo "Docker isn't running. Start it (colima start) and build again." >&2
  exit 1
fi

# Stage the sources at their paths within the package
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for src in "$@"; do
  rel=${src#"$package"/}
  mkdir -p "$stage/$(dirname "$rel")"
  cp "$src" "$stage/$rel"
done

# macOS tar would otherwise add extended attributes Linux tar warns about
tar_flags=()
[[ $(uname) == Darwin ]] && tar_flags=(--no-xattrs --no-mac-metadata)

mkdir -p "$out_dir"
COPYFILE_DISABLE=1 tar ${tar_flags[@]+"${tar_flags[@]}"} -C "$stage" -c . |
  docker run -i --rm "$image" sh -c "
    set -e
    mkdir /app && cd /app && tar -x
    make TARGET=$name >&2
    tar -c $name.3dsx $name.smdh $name.elf
  " |
  tar -x -C "$out_dir"
