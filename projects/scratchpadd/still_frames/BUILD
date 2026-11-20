load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# When a screen can show its last frame again (StillFrames.hpp): never
# while anything moves or changes, and one frame more after a move.
cc_library(
    name = "still_frames",
    hdrs = ["include/ScratchPadd/StillFrames/StillFrames.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

# The sources, for the consoles' Makefile builds
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "StillFrames_test",
    size = "small",
    srcs = ["tests/StillFrames_test.cpp"],
    deps = [
        ":still_frames",
        "@gtest//:gtest_main",
    ],
)
