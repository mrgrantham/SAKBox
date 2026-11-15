load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A game as its starting state and the list of what happened, written as
# text, replayed and checked (see README.md). Header-only, with no
# dependencies, so the consoles' Makefile builds take it as it is.
cc_library(
    name = "action_log",
    hdrs = ["include/ActionLog/ActionLog.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

# The headers, for a DS or 3DS build (devkitPro's Makefile builds, given
# these by extra_srcs; add action_log/include to the Makefile's INCLUDES)
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "ActionLog_test",
    srcs = ["tests/ActionLog_test.cpp"],
    deps = [
        ":action_log",
        "//projects/seeded_random",
        "@gtest//:gtest_main",
    ],
)
