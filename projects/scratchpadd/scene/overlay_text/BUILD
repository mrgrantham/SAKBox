load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The overlay's fonts (Font.hpp: Silkscreen, a pixel font, on the DS;
# Orbitron, a smooth one, elsewhere), their letters as C++ data written by
# make_fonts.py from //third_party/fonts, and the overlay drawn in them by
# the backends (OverlayText.hpp), measured by Overlay::textWidth,
# lineHeight and inkOf.
cc_library(
    name = "overlay_text",
    srcs = [
        "src/Font.cpp",
        "src/OrbitronFont.cpp",
        "src/OverlayText.cpp",
        "src/SilkscreenFont.cpp",
    ],
    hdrs = [
        "include/ScratchPadd/Scene/Font.hpp",
        "include/ScratchPadd/Scene/OverlayText.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["//projects/scratchpadd/scene"],
)

# The sources, for the consoles' Makefile builds (each links only its own
# font's data: the other's is never used, and the linker drops it)
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "Font_test",
    size = "small",
    srcs = ["tests/Font_test.cpp"],
    deps = [
        ":overlay_text",
        "@gtest//:gtest_main",
    ],
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
