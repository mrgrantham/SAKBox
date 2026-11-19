load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The overlay's text on every platform: stb_easy_font's letters as
# rectangles (the DS fills them into its 2D text layer) or triangles, and how
# wide and tall they are (Overlay::textWidth and lineHeight).
cc_library(
    name = "overlay_text",
    srcs = ["src/OverlayText.cpp"],
    hdrs = ["include/ScratchPadd/Scene/OverlayText.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/scratchpadd/scene",
        "@stb",
    ],
)

# The sources, for the consoles' Makefile builds (with stb_easy_font.h)
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "OverlayText_test",
    size = "small",
    srcs = ["tests/OverlayText_test.cpp"],
    deps = [
        ":overlay_text",
        "@gtest//:gtest_main",
        "@stb",  # (stb_easy_font_print, to check fontRectangles against)
    ],
)
