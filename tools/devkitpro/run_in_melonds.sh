#!/bin/sh
# Opens a DS app in the melonDS emulator: what `bazel run` does for a
# devkitpro_nds_app target. The rule fills in the app's path and melonDS's
# (downloaded by Bazel: see //tools/melonds). Arguments go to melonDS.
#
# melonDS keeps its settings in .melonds in the repo, never in the user's
# own: on macOS it reads a `portable` folder next to melonDS.app, so this runs
# a copy of Bazel's melonDS.app in .melonds (an APFS clone: instant, and no
# extra space); on Linux its settings folder follows XDG_CONFIG_HOME.
# $MELONDS, if set, is used instead of Bazel's melonDS.
#
# melonDS boots homebrew straight away with its own free BIOS and firmware,
# so no Nintendo files are needed.

app="$(pwd)/@@APP@@"
bundled="@@MELONDS@@"

if [ -n "$MELONDS" ]; then
  emulator="$MELONDS"
elif [ -n "$bundled" ]; then
  emulator="$(realpath "$(pwd)/$bundled")" # the real file, not the runfiles link
else
  echo "Bazel has no melonDS for this machine (it has macOS, and Linux on x86-64)." >&2
  echo "Set MELONDS to a melonDS program to use that. The app is at: $app" >&2
  exit 1
fi

home="${BUILD_WORKSPACE_DIRECTORY:-$PWD}/.melonds"
mkdir -p "$home/portable"
echo "Opening @@APP@@ in melonDS; close melonDS to finish."
echo "Its settings are in $home."

case "$emulator" in
*.app/Contents/MacOS/*)
  # macOS: run the copy next to `portable`; -W waits for it to quit
  bundle="${emulator%/Contents/MacOS/*}"
  if [ "$(cat "$home/melonDS.app.source" 2>/dev/null)" != "$bundle" ]; then
    rm -rf "$home/melonDS.app"
    cp -cR "$bundle" "$home/melonDS.app" 2>/dev/null || cp -R "$bundle" "$home/melonDS.app" || exit 1
    echo "$bundle" > "$home/melonDS.app.source"
  fi
  exec open -n -W "$home/melonDS.app" --args "$app" "$@"
  ;;
*)
  export XDG_CONFIG_HOME="$home/portable"
  export APPIMAGE_EXTRACT_AND_RUN=1 # (Linux: run the AppImage without FUSE)
  exec "$emulator" "$app" "$@"
  ;;
esac
