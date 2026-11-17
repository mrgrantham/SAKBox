load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A game's title screen in 3D (Cosmic Hex's round 87, for any game): the
# title in blocky outlined letters in front, a model turning behind, and a
# slowly turning starfield. See include/ScratchPadd/TitleScene/TitleScene.hpp.
cc_library(
    name = "title_scene",
    srcs = [
        "src/TitleScene.cpp",
        "src/TitleScenePieces.cpp",
    ],
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
        ":title_scene",
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
