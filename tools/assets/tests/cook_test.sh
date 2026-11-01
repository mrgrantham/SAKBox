#!/bin/bash
# The cook tool on a model Blender exported in the build (the example's
# rock, about 5,120 triangles, flat-shaded): each over_budget mode against a
# 200-polygon DS budget, and a scene over the DS's frame limit.
set -u
cook="$1"
glb="$2"
out="${TEST_TMPDIR:-$(mktemp -d)}"
failures=0

check() { # NAME EXPECTED-EXIT GREP-PATTERN COMMAND...
  local name="$1" expected="$2" pattern="$3"
  shift 3
  "$@" > "$out/$name.log" 2>&1
  local status=$?
  if [ "$status" != "$expected" ]; then
    echo "FAIL $name: exited $status, not $expected"; cat "$out/$name.log"; failures=$((failures + 1))
  elif [ -n "$pattern" ] && ! grep -q "$pattern" "$out/$name.log"; then
    echo "FAIL $name: no \"$pattern\" in:"; cat "$out/$name.log"; failures=$((failures + 1))
  else
    echo "ok   $name"
  fi
}

model() { # MODE
  "$cook" model --gltf "$glb" --name rock --object Rock --label //test:rock \
    --report "$out/$1.json" --counts "$out/$1.counts" \
    --output ds Assets "$out/$1.cc" "$out/$1.h" 200 6144 "$1"
}

check fail 1 "over the DS's budget: 5120 polygons" model FAIL
check fail_says_how 1 "an object made for it named Rock_DS" model FAIL
check simplify 1 "SIMPLIFY_AGGRESSIVE goes further" model SIMPLIFY
check aggressive 0 "" model SIMPLIFY_AGGRESSIVE
check aggressive_fits 0 "rockTriangles = 200" cat "$out/SIMPLIFY_AGGRESSIVE.h"
check report 0 '"fits": true' cat "$out/SIMPLIFY_AGGRESSIVE.json"
check desktop 0 "rockTriangles = 5120" sh -c "\"$cook\" model --gltf \"$glb\" --name rock --object Rock \
  --output desktop Assets \"$out/d.cc\" \"$out/d.h\" 0 0 FAIL && cat \"$out/d.h\""
check missing 1 "no object named Nothing" "$cook" model --gltf "$glb" --name x --object Nothing \
  --output ds Assets "$out/x.cc" "$out/x.h" 0 0 FAIL

# 11 rocks of 200 polygons: 2200, over the DS's 2048 a frame
check scene_over 1 "shows more than the DS draws a frame" "$cook" scene --label //test:scene \
  --report "$out/scene.txt" --limit ds 2048 6144 --model "$out/SIMPLIFY_AGGRESSIVE.counts" 11
check scene_within 0 "" "$cook" scene --label //test:scene --report "$out/scene.txt" \
  --limit ds 2048 6144 --model "$out/SIMPLIFY_AGGRESSIVE.counts" 10
check scene_report 0 "2000 of 2048 polygons" cat "$out/scene.txt"

exit $((failures > 0))
