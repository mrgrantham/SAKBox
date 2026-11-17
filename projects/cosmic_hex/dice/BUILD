load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//projects/cyclone:real.bzl", "real_binary")

# Cosmic Hex's physics dice (round 87): two dice thrown in Cyclone into a
# glass container the size of the top screen; the faces they settle on are
# the roll. dice_fairness throws them many times to check they're fair.
cc_library(
    name = "dice",
    srcs = ["src/DiceTray.cpp"],
    hdrs = ["include/CosmicHex/Dice/DiceTray.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//projects/cosmic_hex:__subpackages__"],
    deps = [
        "//projects/cyclone",
        "//projects/seeded_random",
    ],
)

cc_test(
    name = "DiceTray_test",
    size = "small",
    srcs = ["tests/DiceTray_test.cpp"],
    deps = [
        ":dice",
        "@gtest//:gtest_main",
    ],
)

cc_binary(
    name = "dice_fairness",
    srcs = ["src/dice_fairness.cpp"],
    deps = [":dice"],
)

# The same in float (the 3DS's) and in 16.16 fixed point (the DS's)
real_binary(
    name = "dice_fairness_float",
    real = "float",
    target = ":dice_fairness",
)

real_binary(
    name = "dice_fairness_flex_point16",
    real = "flex_point16",
    target = ":dice_fairness",
)

# The sources, for the consoles' Makefile builds
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/DiceTray.cpp",
    ]),
    visibility = ["//visibility:public"],
)
