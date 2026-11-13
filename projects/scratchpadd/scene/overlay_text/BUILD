load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The overlay's text on desktops and the 3DS: stb_easy_font's letters as
# triangles, and how wide and tall they are (Overlay::textWidth and
# lineHeight). The DS draws its text on a 2D layer instead.
cc_library(
    name = "overlay_text",
    srcs = ["src/OverlayText.cpp"],
    hdrs = ["include/ScratchPadd/Scene/OverlayText.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//projects/scratchpadd/scene:__subpackages__"],
    deps = [
        "//projects/scratchpadd/scene",
        "@stb",
    ],
)

# The sources, for the 3DS's Makefile builds (with stb_easy_font.h)
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
    ],
)
