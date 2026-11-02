load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_shell//shell:sh_test.bzl", "sh_test")

# Models from Blender for ScratchPadd's scene layer: scene_model and scene
# (defs.bzl), the export script Blender runs, and the cook tool. See
# README.md.

exports_files(["export_gltf.py"])

# Overrides every scene_model's over_budget for a build:
# --//tools/assets:over_budget=FAIL (or SIMPLIFY, SIMPLIFY_AGGRESSIVE). A CI
# run with FAIL lists every model that only fits because it was simplified.
string_flag(
    name = "over_budget",
    build_setting_default = "",
    values = [
        "",
        "FAIL",
        "SIMPLIFY",
        "SIMPLIFY_AGGRESSIVE",
    ],
    visibility = ["//visibility:public"],
)

# The cook runs in the build (Bazel's exec configuration), which the
# .bazelrc's --cxxopt doesn't reach: the same C++ standard, here
COOK_COPTS = ["-std=c++26"]

cc_library(
    name = "cook_lib",
    srcs = [
        "cook/Cook.cpp",
        "cook/Gltf.cpp",
    ],
    hdrs = [
        "cook/Cook.hpp",
        "cook/Gltf.hpp",
    ],
    copts = COOK_COPTS,
    deps = [
        "@cgltf",
        "@meshoptimizer",
    ],
)

cc_binary(
    name = "cook",
    srcs = ["cook/main.cpp"],
    copts = COOK_COPTS,
    visibility = ["//visibility:public"],
    deps = [":cook_lib"],
)

cc_test(
    name = "Cook_test",
    size = "small",
    srcs = ["tests/Cook_test.cpp"],
    deps = [
        ":cook_lib",
        "@gtest//:gtest_main",
    ],
)

# The cook tool on a model Blender exported in the build (the example's
# rock): each over_budget mode, and scenes within and over the DS's frame.
# Tagged blender, as it needs Blender (not on Linux on ARM).
sh_test(
    name = "cook_tool_test",
    size = "small",
    srcs = ["tests/cook_tool_test.sh"],
    args = [
        "$(rootpath :cook)",
        "$(rootpath //projects/examples/blender_model:rock_glb)",
    ],
    data = [
        ":cook",
        "//projects/examples/blender_model:rock_glb",
    ],
    tags = ["blender"],
)
