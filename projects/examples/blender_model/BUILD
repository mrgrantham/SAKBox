load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//tools/assets:defs.bzl", "scene", "scene_model")

# Models from Blender (models/models.blend, made by models/make_models.py),
# cooked for each platform: see README.md and tools/assets.

# The DS's version is made by hand in the .blend (Ship_DS), within the DS's
# budget, so it's used as it is
scene_model(
    name = "ship",
    src = "models/models.blend",
    object = "Ship",
    visibility = ["//projects/examples/blender_model:__subpackages__"],  # ds/
)

# No DS version in the .blend: the build simplifies it to 200 polygons.
# SIMPLIFY, which keeps its look (the surface moves at most 5% of its size),
# gets it only to about 400; SIMPLIFY_AGGRESSIVE goes on until it fits.
scene_model(
    name = "rock",
    src = "models/models.blend",
    budget = {"ds": 200},
    object = "Rock",
    over_budget = {"ds": "SIMPLIFY_AGGRESSIVE"},
    visibility = ["//projects/examples/blender_model:__subpackages__"],  # ds/
)

# The rock as Blender exported it, for //tools/assets:cook_tool_test
filegroup(
    name = "rock_glb",
    srcs = [":rock_cooked"],
    output_group = "glb",
    tags = ["blender"],
    visibility = ["//tools/assets:__pkg__"],
)

# What the example shows at once on the DS: a ship and two rocks, within a
# frame's 2048 polygons
scene(
    name = "hangar",
    models = {
        ":rock": 2,
        ":ship": 1,
    },
)

# Each model as desktops draw it, beside the DS's version (_ds_preview)
cc_binary(
    name = "blender_model",
    srcs = ["src/main.cpp"],
    data = [":hangar"],
    tags = ["blender"],
    deps = [
        "//projects/scratchpadd/debug_log",
        "//projects/scratchpadd/storage:storage_padd",
        ":rock",
        ":rock_ds_preview",
        ":ship",
        ":ship_ds_preview",
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop",
    ],
)
