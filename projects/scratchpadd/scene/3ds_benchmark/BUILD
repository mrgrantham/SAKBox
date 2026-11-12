load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# How many triangles a 3DS's GPU draws a frame, with citro3d, for the scene
# layer's 3DS budgets (see README.md). Needs Docker running (see
# //tools/devkitpro); tools/fbi_install installs it on a 3DS, and `bazel run`
# opens it in Azahar (whose timings aren't the hardware's).
devkitpro_3ds_app(
    name = "scene-3ds-benchmark",
    srcs = ["Makefile"] + glob(["source/**"]),
)
