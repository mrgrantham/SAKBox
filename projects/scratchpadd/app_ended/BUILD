load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# What a DS or 3DS app shows once it has ended (see AppEnded.hpp): its text,
# laid out for a text screen, built and tested on desktops too
# (EndedText.hpp), and AppEnded.hpp, which shows it on a DS, which the
# handhelds' Makefile builds take from :sources.
cc_library(
    name = "ended_text",
    hdrs = ["include/ScratchPadd/AppEnded/EndedText.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

cc_test(
    name = "EndedText_test",
    size = "small",
    srcs = ["tests/EndedText_test.cpp"],
    deps = [
        ":ended_text",
        "@gtest//:gtest_main",
    ],
)

# For the DS's and 3DS's Makefile builds (devkitpro_nds_app's and
# devkitpro_3ds_app's extra_srcs)
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)
