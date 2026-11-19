load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Words (and a small mark) shown while an app's start runs, fading in and
# out (StartupSplash.hpp); on the DS the fade is the screens' hardware
# brightness, stepped by the refresh interrupt while the work runs.
cc_library(
    name = "startup_splash",
    srcs = ["src/StartupSplash.cpp"],
    hdrs = ["include/ScratchPadd/StartupSplash/StartupSplash.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":fade",
        "//projects/scratchpadd/scene",
    ],
)

# The fades' brightness, on its own (tested without a screen)
cc_library(
    name = "fade",
    hdrs = ["include/ScratchPadd/StartupSplash/Fade.hpp"],
    strip_include_prefix = "include/",
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
    name = "Fade_test",
    size = "small",
    srcs = ["tests/Fade_test.cpp"],
    deps = [
        ":fade",
        "@gtest//:gtest_main",
    ],
)
