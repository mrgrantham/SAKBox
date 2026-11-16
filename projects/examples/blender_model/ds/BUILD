load("//tools/devkitpro:defs.bzl", "devkitpro_nds_app")

# The example's models on a DS, as the build cooked them for it, drawn with
# ScratchPadd's scene layer (its DS backend): see ../README.md. Needs Docker
# running (see //tools/devkitpro) and Blender (tagged blender, as the models
# are); `bazel run` opens it in melonDS.
devkitpro_nds_app(
    name = "blender-model-ds",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/examples/blender_model:look": "look",
        "//projects/scratchpadd/input:sources": "scratchpadd/input",
        "//projects/scratchpadd/app_ended:sources": "scratchpadd/app_ended",
        # (a debug build's log: //projects/scratchpadd/debug_log)
        "//projects/scratchpadd/debug_log:sources": "scratchpadd/debug_log",
        "//projects/scratchpadd/storage:sources": "scratchpadd/storage",
        "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
        "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
        "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
        "@spdlog//:sources": "third_party/spdlog",
        # (std::chrono's clock on a DS, which the log needs)
        "//projects/scratchpadd/ds/platform:sources": "scratchpadd_ds/platform",
        "//projects/examples/blender_model:rock_ds_sources": "assets",
        "//projects/examples/blender_model:ship_ds_sources": "assets",
        "//projects/flexmath:fixed_point_sources": "flexmath",
        "//projects/scratchpadd/scene:sources": "scratchpadd/scene",
        "//projects/scratchpadd/scene/ds:sources": "scratchpadd/scene_ds",
        "//projects/scratchpadd/scene/ds:screenshot_sources": "scratchpadd/scene_ds",
    },
    tags = ["blender"],
)
