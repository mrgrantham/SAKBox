load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A moment's message across the middle of a screen (Banner.hpp): a band
# fading in and out, its words sweeping in, holding, sweeping out; timed by
# Motion, drawn on an overlay. Cosmic Hex's turn banners.
cc_library(
    name = "banner",
    srcs = ["src/Banner.cpp"],
    hdrs = ["include/ScratchPadd/Banner/Banner.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/scratchpadd/motion",
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
    name = "Banner_test",
    size = "small",
    srcs = ["tests/Banner_test.cpp"],
    deps = [
        ":banner",
        "@gtest//:gtest_main",
    ],
)
