#!/bin/sh
# Opens a 3DS app in the Azahar emulator: what `bazel run` does for a
# devkitpro_3ds_app target. The rule fills in the app's path and Azahar's
# (downloaded by Bazel: see //tools/azahar). Arguments go to Azahar.
#
# Azahar runs "portable": its settings, saves and SD card go in the `user`
# folder of .azahar in the repo, so it never touches an Azahar installed on
# the machine. Azahar looks for that folder where it starts: on Linux the
# working directory, on macOS the folder holding Azahar.app, so there this
# runs a copy of Bazel's Azahar.app in .azahar (an APFS clone: instant, and
# no extra space). $AZAHAR, if set, is used instead of Bazel's Azahar.

app="$(pwd)/@@APP@@"
bundled="@@AZAHAR@@"

if [ -n "$AZAHAR" ]; then
  emulator="$AZAHAR"
elif [ -n "$bundled" ]; then
  emulator="$(realpath "$(pwd)/$bundled")" # the real file, not the runfiles link
else
  echo "Bazel has no Azahar for this machine (it has macOS, and Linux on x86-64)." >&2
  echo "Set AZAHAR to an Azahar program to use that. The app is at: $app" >&2
  exit 1
fi

home="${BUILD_WORKSPACE_DIRECTORY:-$PWD}/.azahar"
mkdir -p "$home/user"
echo "Opening @@APP@@ in Azahar; close Azahar to finish."
echo "Its SD card is $home/user/sdmc."

case "$emulator" in
*.app/Contents/MacOS/*)
  # macOS: run the copy next to `user`, through `open` as Azahar asks (run
  # directly, it stops to warn that some features may be missing); -W waits
  # for it to quit
  bundle="${emulator%/Contents/MacOS/*}"
  if [ "$(cat "$home/Azahar.app.source" 2>/dev/null)" != "$bundle" ]; then
    rm -rf "$home/Azahar.app"
    cp -cR "$bundle" "$home/Azahar.app" 2>/dev/null || cp -R "$bundle" "$home/Azahar.app" || exit 1
    echo "$bundle" > "$home/Azahar.app.source"
  fi
  exec open -n -W "$home/Azahar.app" --args "$app" "$@"
  ;;
*)
  cd "$home" || exit 1
  export APPIMAGE_EXTRACT_AND_RUN=1 # (Linux: run the AppImage without FUSE)
  exec "$emulator" "$app" "$@"
  ;;
esac
