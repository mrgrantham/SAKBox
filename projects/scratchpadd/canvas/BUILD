load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A picture drawn in pixels by the CPU, for a screen with no 3D of its own (a
# console's bottom screen): rectangles, outlines, words in the overlay's
# font, sprites blended over (Canvas.hpp). From Cosmic Hex.
cc_library(
    name = "canvas",
    srcs = ["src/Canvas.cpp"],
    hdrs = ["include/ScratchPadd/Canvas/Canvas.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/overlay_text",
    ],
)

# The sources, for the consoles' Makefile builds
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "Canvas_test",
    size = "small",
    srcs = ["tests/Canvas_test.cpp"],
    deps = [
        ":canvas",
        "//projects/scratchpadd/scene/overlay_text",
        "@gtest//:gtest_main",
    ],
)
