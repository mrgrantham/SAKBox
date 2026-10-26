load("@rules_cc//cc:defs.bzl", "cc_library")

# TinySoundFont (https://github.com/schellingb/TinySoundFont, MIT): plays
# SoundFonts (tsf.h) and reads MIDI files (tml.h). Single-header libraries:
# one source file in the audio core defines TSF_IMPLEMENTATION and
# TML_IMPLEMENTATION.
cc_library(
    name = "tinysoundfont",
    hdrs = ["tml.h", "tsf.h"],
    include_prefix = "tinysoundfont",  # <tinysoundfont/tsf.h>
    visibility = ["//visibility:public"],
)

# The sources, for builds outside Bazel's C++ rules (the 3DS)
filegroup(
    name = "sources",
    srcs = ["LICENSE", "tml.h", "tsf.h"],
    visibility = ["//visibility:public"],
)
