load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Buttons on a touch screen, worked by touch or by the pad, drawn on a
# Canvas (Buttons.hpp): a panel of them, each usable or dimmed, a D-pad
# focus, a flash and a message when one's used.
cc_library(
    name = "buttons",
    srcs = ["src/Buttons.cpp"],
    hdrs = ["include/ScratchPadd/Buttons/Buttons.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["//projects/scratchpadd/canvas"],
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
    name = "Buttons_test",
    size = "small",
    srcs = ["tests/Buttons_test.cpp"],
    deps = [
        ":buttons",
        "@gtest//:gtest_main",
    ],
)
