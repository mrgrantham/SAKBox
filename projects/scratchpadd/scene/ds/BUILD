load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The scene layer's DS backend (libnds' GX), on the top screen: see
# ../DESIGN.md. DS apps build it in their Makefile builds, from `sources`
# (with //projects/scratchpadd/scene:sources and flexmath's
# fixed_point_sources); only its display lists, which need no libnds, are
# built and tested here.
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

# Screenshots to the SD card (include/ScratchPadd/Scene/DS/Screenshot.hpp),
# for apps that want them: staged into the same folder as `sources` (its
# source includes ../src/Bitmap.hpp), with screenshot in the Makefile's
# SOURCES and -lfat in its LIBS. Apart, so other apps needn't link libfat.
filegroup(
    name = "screenshot_sources",
    srcs = ["screenshot/Screenshot.cpp"],
    visibility = ["//visibility:public"],
)

# The display lists, with Scalar as each platform has it: float (desktops)
# and Q16.16 (the DS)
[cc_library(
    name = "display_list" + suffix,
    srcs = ["src/DisplayList.cpp"],
    hdrs = [
        "src/Bitmap.hpp",
        "src/DisplayList.hpp",
        "src/ToonTable.hpp",
    ],
    strip_include_prefix = "src/",
    deps = ["//projects/scratchpadd/scene:scene" + suffix],
) for suffix in ["", "_fixed_point"]]

[cc_test(
    name = "DisplayList" + suffix + "_test",
    size = "small",
    srcs = ["tests/DisplayList_test.cpp"],
    deps = [
        ":display_list" + suffix,
        "@gtest//:gtest_main",
    ],
) for suffix in ["", "_fixed_point"]]

cc_test(
    name = "ToonTable_test",
    size = "small",
    srcs = ["tests/ToonTable_test.cpp"],
    deps = [
        ":display_list",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Bitmap_test",
    size = "small",
    srcs = ["tests/Bitmap_test.cpp"],
    deps = [
        ":display_list",
        "@gtest//:gtest_main",
    ],
)
