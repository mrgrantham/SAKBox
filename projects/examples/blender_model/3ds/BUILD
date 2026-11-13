load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# The example's models on a 3DS, as the build cooked them for it, drawn with
# ScratchPadd's scene layer (its 3DS backend), in stereo while the 3D slider
# is up: see ../README.md. Needs Docker running (see //tools/devkitpro) and
# Blender (tagged blender, as the models are); tools/fbi_install installs it
# on a 3DS, and `bazel run` opens it in Azahar.
devkitpro_3ds_app(
    name = "blender-model-3ds",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/examples/blender_model:rock_3ds_sources": "assets",
        "//projects/examples/blender_model:ship_3ds_sources": "assets",
        "//projects/scratchpadd/scene:sources": "scratchpadd/scene",
        "//projects/scratchpadd/scene/3ds:sources": "scratchpadd/scene_3ds",
        "//projects/scratchpadd/scene/overlay_text:sources": "scratchpadd/overlay_text",
        "@stb//:stb_easy_font.h": "third_party/stb",
    },
    tags = ["blender"],
)
