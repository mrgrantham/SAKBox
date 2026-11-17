load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A game's title screen in 3D (Cosmic Hex's round 87, for any game): the
# title in blocky outlined letters in front, a model turning behind, and a
# slowly turning starfield. See include/ScratchPadd/TitleScene/TitleScene.hpp.
cc_library(
    name = "title_scene",
    srcs = ["src/TitleScene.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":title_scene_pieces",
        "//projects/scratchpadd/scene",
    ],
)

# The pieces made in code (the letters, the sky), apart from the scene that
# draws them with a platform's renderer, so their test needs no renderer
cc_library(
    name = "title_scene_pieces",
    srcs = ["src/TitleScenePieces.cpp"],
    hdrs = ["include/ScratchPadd/TitleScene/TitleScene.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/scratchpadd/scene",
        "@stb",
    ],
)

cc_test(
    name = "TitleScene_test",
    size = "small",
    srcs = ["tests/TitleScene_test.cpp"],
    deps = [
        ":title_scene_pieces",
        "@gtest//:gtest_main",
    ],
)

# The sources, for the consoles' Makefile builds (with stb_easy_font.h)
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)
