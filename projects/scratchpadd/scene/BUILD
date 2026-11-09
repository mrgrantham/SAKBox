load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The scene layer's API (Scene.hpp) and what every backend shares: its
# types, meshes built in code, and the overlay's text and rectangles. A
# program also links one backend, which defines Renderer and its Display
# (on desktops, //projects/scratchpadd/scene/desktop picks one). See
# DESIGN.md.
[cc_library(
    name = "scene" + suffix,
    hdrs = glob(["include/**/*.hpp"]),
    srcs = [
        "src/MeshBuilder.cpp",
        "src/Overlay.cpp",
        "src/Primitives.cpp",
        "src/Types.cpp",
    ],
    defines = defines,
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = deps,
) for suffix, defines, deps in [
    ("", [], []),
    # Scalar as the DS has it (Q16.16), on desktops, to build and test the
    # shared code and the DS's display lists with it
    ("_fixed_point", ["SCRATCHPADD_SCENE_FIXED_POINT"], ["//projects/flexmath"]),
]]

# The sources, for the DS's Makefile builds (with a backend's: see ds/BUILD)
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

[cc_test(
    name = "Scene" + suffix + "_test",
    size = "small",
    srcs = ["tests/Scene_test.cpp"],
    deps = [
        ":scene" + suffix,
        "@gtest//:gtest_main",
    ],
) for suffix in ["", "_fixed_point"]]
