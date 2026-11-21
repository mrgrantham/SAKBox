load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The scene layer's 3DS backend (citro3d), on the top screen, in stereo
# while the 3D slider is up: see ../DESIGN.md. 3DS apps build it in their
# Makefile builds, from `sources` (with //projects/scratchpadd/scene:sources
# and //projects/scratchpadd/scene/overlay_text:sources, and its shaders:
# the Makefile's SOURCES list src, so devkitPro's rules assemble its .pica
# files with picasso).
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

# Where an image's pixels go in a texture (TextureTiles.hpp): no citro3d,
# so the order the GPU reads them in is tested on desktops
cc_library(
    name = "texture_tiles",
    hdrs = ["include/ScratchPadd/Scene/3DS/TextureTiles.hpp"],
    strip_include_prefix = "include/",
)

cc_test(
    name = "TextureTiles_test",
    size = "small",
    srcs = ["tests/TextureTiles_test.cpp"],
    deps = [
        ":texture_tiles",
        "@gtest//:gtest_main",
    ],
)
