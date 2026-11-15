load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A seeded random number generator that gives the same numbers on every
# platform, seeds as short codes, and named streams (see README.md).
# Header-only, with no dependencies, so the consoles' Makefile builds take
# it as it is.
cc_library(
    name = "seeded_random",
    hdrs = [
        "include/SeededRandom/Generator.hpp",
        "include/SeededRandom/Seed.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

# The headers, for a DS or 3DS build (devkitPro's Makefile builds, given
# these by extra_srcs; add seeded_random/include to the Makefile's INCLUDES)
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)

[cc_test(
    name = test,
    srcs = ["tests/" + test + ".cpp"],
    deps = [
        ":seeded_random",
        "@gtest//:gtest_main",
    ],
) for test in [
    "Generator_test",
    "Seed_test",
]]
