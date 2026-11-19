load("@rules_cc//cc:defs.bzl", "cc_library")

# A single-frame allocator (Gregory 6.2.1): FrameArena.hpp. It
# takes effect through the counting allocator
# (//projects/scratchpadd/memory_stats:counting_allocator); without it,
# using an arena changes nothing. Header-only, with no dependencies, so any
# library can use it (GameBots' search does).
cc_library(
    name = "frame_arena",
    hdrs = ["include/ScratchPadd/FrameArena.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

# The header, for the consoles' Makefile builds
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)
