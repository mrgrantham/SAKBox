# The scene layer's 3DS backend (citro3d), on the top screen, in stereo
# while the 3D slider is up: see ../DESIGN.md. 3DS apps build it in their
# Makefile builds, from `sources` (with //projects/scratchpadd/scene:sources,
# //projects/scratchpadd/scene/overlay_text:sources and stb_easy_font.h, and
# its shaders: the Makefile's SOURCES list src, so devkitPro's rules
# assemble its .pica files with picasso).
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)
