load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# The animation lab (desktop only): motions made with physics (Cyclone) in
# three ways (a solved throw, a guided spring, a search), flat or in 3D, and
# baked into ScratchPadd's BakedMotion for the consoles to play (Lab.hpp).
cc_library(
    name = "lab",
    srcs = [
        "src/Lab.cpp",
        "src/Settings.cpp",
    ],
    hdrs = [
        "include/AnimationLab/Lab.hpp",
        "include/AnimationLab/Settings.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/cyclone",
        "//projects/scratchpadd/motion",
        "//projects/seeded_random",
    ],
)

cc_test(
    name = "Lab_test",
    size = "small",
    srcs = ["tests/Lab_test.cpp"],
    deps = [
        ":lab",
        "@gtest//:gtest_main",
    ],
)

# The lab's window: the history of runs (and one compared beside this
# one), each way's settings, the shape flying the motion in 3D, the plots,
# the bake, and Save
#   bazel run -c opt //projects/animation_lab:animation_lab
cc_binary(
    name = "animation_lab",
    srcs = [
        "app/LabShapes.cpp",
        "app/LabShapes.hpp",
        "app/LabWindowPadd.cpp",
        "app/LabWindowPadd.hpp",
        "app/main.cpp",
    ],
    deps = [
        ":lab",
        "//projects/cosmic_hex/art",
        "//projects/cosmic_hex/handheld",
        "//projects/cosmic_hex/models:rocket",
        "//projects/examples/blender_model:rock",
        "//projects/examples/blender_model:ship",
        "//projects/scratchpadd/storage",
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/debug_log",
        "//projects/scratchpadd/debug_ui",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop",
        "@imgui",
        "@imgui//:imgui_stdlib",
        "@implot",
    ],
)
