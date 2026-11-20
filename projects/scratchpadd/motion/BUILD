load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Things moving over time, the same on every platform: curves, tracks,
# scripts of steps, and fixed steps for physics (Motion.hpp); motions baked
# in the animation lab, played back (BakedMotion.hpp).
cc_library(
    name = "motion",
    srcs = [
        "src/BakedMotion.cpp",
        "src/Motion.cpp",
    ],
    hdrs = [
        "include/ScratchPadd/Motion/BakedMotion.hpp",
        "include/ScratchPadd/Motion/Motion.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
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
    name = "Motion_test",
    size = "small",
    srcs = ["tests/Motion_test.cpp"],
    deps = [
        ":motion",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "BakedMotion_test",
    size = "small",
    srcs = ["tests/BakedMotion_test.cpp"],
    deps = [
        ":motion",
        "@gtest//:gtest_main",
    ],
)
