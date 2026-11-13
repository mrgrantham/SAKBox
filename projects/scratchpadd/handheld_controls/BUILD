load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Every padd's controls on a DS's or 3DS's text screen, as tabs (see
# ControlTabs.hpp): the shared part, built and tested on desktops too, and
# ConsolePanel.hpp, its input and printing on the handhelds, which their
# Makefile builds take from :sources.
cc_library(
    name = "handheld_controls",
    hdrs = ["include/ScratchPadd/HandheldControls/ControlTabs.hpp"],
    srcs = ["src/ControlTabs.cpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/input",
        "//projects/scratchpadd/messages",
    ],
)

# For the DS's and 3DS's Makefile builds (devkitpro_nds_app's and
# devkitpro_3ds_app's extra_srcs)
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "ControlTabs_test",
    size = "small",
    srcs = ["tests/ControlTabs_test.cpp"],
    deps = [
        ":handheld_controls",
        "@gtest//:gtest_main",
    ],
)
