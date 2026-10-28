load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The scene layer's API (Scene.hpp) and what every backend shares: its
# types, meshes built in code, and the overlay's text and rectangles. A
# program also links one backend, which defines Renderer and its Display
# (for OpenGL, //projects/scratchpadd/scene/gl). See DESIGN.md.
cc_library(
    name = "scene",
    hdrs = glob(["include/**/*.hpp"]),
    srcs = [
        "src/MeshBuilder.cpp",
        "src/Overlay.cpp",
        "src/Types.cpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["@stb"],
)

cc_test(
    name = "Scene_test",
    size = "small",
    srcs = ["tests/Scene_test.cpp"],
    deps = [
        ":scene",
        "@gtest//:gtest_main",
    ],
)
