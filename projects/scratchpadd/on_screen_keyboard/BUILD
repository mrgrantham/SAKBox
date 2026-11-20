load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Typing a short text on a touch screen or with a pad (OnScreenKeyboard.hpp): a
# field above QWERTY keys, on the Buttons library.
cc_library(
    name = "on_screen_keyboard",
    srcs = ["src/OnScreenKeyboard.cpp"],
    hdrs = ["include/ScratchPadd/OnScreenKeyboard/OnScreenKeyboard.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/scratchpadd/buttons",
        "//projects/scratchpadd/canvas",
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
    name = "OnScreenKeyboard_test",
    size = "small",
    srcs = ["tests/OnScreenKeyboard_test.cpp"],
    deps = [
        ":on_screen_keyboard",
        "@gtest//:gtest_main",
    ],
)
