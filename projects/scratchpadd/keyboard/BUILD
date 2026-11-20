load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Typing a short text on a touch screen or with a pad (Keyboard.hpp): a
# field above QWERTY keys, on the Buttons library.
cc_library(
    name = "keyboard",
    srcs = ["src/Keyboard.cpp"],
    hdrs = ["include/ScratchPadd/Keyboard/Keyboard.hpp"],
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
    name = "Keyboard_test",
    size = "small",
    srcs = ["tests/Keyboard_test.cpp"],
    deps = [
        ":keyboard",
        "@gtest//:gtest_main",
    ],
)
